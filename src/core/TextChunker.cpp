#include "TextChunker.h"
#include <QStringList>
#include <QString>
#include <QDebug>

QStringList TextChunker::getSeparatorsForType(FileType fileType)
{
    switch (fileType) {
        case FileType::CodeCpp:
            // C-Family: Prioritize splitting after closing braces to keep function bodies intact
            return QStringList{"}\n\n", "}\n", ";\n", "{\n", "\n\n", "\n", " ", ""};
        
        case FileType::CodePython:
            // Python: Prioritize splitting before new top-level definitions
            return QStringList{"\nclass ", "\ndef ", "\n\n", "\n", " ", ""};
        
        case FileType::CodeRexx:
            // Rexx: Split before directives (::), after labels (:), or after flow control exits
            return QStringList{"\n::routine", "\n::method", "\n::requires", ":\n", 
                              "return\n", "exit\n", "\n\n", "\n", " ", ""};
        
        case FileType::CodeSql:
            // SQL: Statement-based splitting (handles PL/SQL / and T-SQL GO)
            return QStringList{"\n/\n", ";\n\n", ";\n", "\nGO\n", "\n\n", "\n", " ", ""};
        
        case FileType::CodeShell:
            // Shell (Bash/PowerShell): Command-based splitting
            return QStringList{"\nfunction ", "}\n\n", "}\n", ";;\n", "\n\n", "\n", " ", ""};
        
        case FileType::CodeCobol:
            // Cobol: Division/Section based splitting
            return QStringList{"\nDIVISION.", "\nSECTION.", ".\n\n", ".\n", "\n\n", "\n", " ", ""};
        
        case FileType::CodeMarkdown:
            // Markdown: Structure-aware splitting
            // Level 1 (Headers): Split before headers to keep section titles with content
            // Level 2 (Code Blocks): Split at code block boundaries
            // Level 3 (Tables): Split before table rows (lines starting with |)
            // Level 4 (Lists/Paragraphs): Standard structural splitting
            return QStringList{"\n# ", "\n## ", "\n### ", "\n```", "\n|", "\n- ", "\n* ", "\n\n", "\n", " ", ""};
        
        case FileType::CodeYaml:
            // YAML/Terraform: Indentation and resource-based splitting
            return QStringList{"\nresource ", "\nmodule ", "\n- ", "\n  ", "\n\n", "\n", " ", ""};
        
        case FileType::PlainText:
        default:
            // Generic text splitting
            return QStringList{"\n\n", "\n", " ", ""};
    }
}

bool TextChunker::isCommentStart(const QString& line, FileType fileType)
{
    QString trimmed = line.trimmed();
    
    switch (fileType) {
        case FileType::CodeCpp:
            // C-family comments: // or /*
            return trimmed.startsWith("//") || trimmed.startsWith("/*");
        
        case FileType::CodePython:
            // Python comments: #
            return trimmed.startsWith("#");
        
        case FileType::CodeRexx:
            // Rexx comments: -- or /*
            return trimmed.startsWith("--") || trimmed.startsWith("/*");
        
        case FileType::CodeSql:
            // SQL comments: --
            return trimmed.startsWith("--");
        
        case FileType::CodeShell:
            // Shell comments: #
            return trimmed.startsWith("#");
        
        case FileType::CodeCobol:
            // Cobol comments: * at start of line
            return trimmed.startsWith("*");
        
        case FileType::CodeMarkdown:
            // Markdown: No traditional comments; # is for headers, not comments
            return false;
        
        case FileType::CodeYaml:
            // YAML/Terraform comments: #
            return trimmed.startsWith("#");
        
        case FileType::PlainText:
        default:
            return false;
    }
}

QStringList TextChunker::split(const QString& text, int chunkSize, int chunkOverlap, FileType fileType)
{
    // Handle edge cases
    if (text.isEmpty()) {
        return QStringList();
    }
    
    if (chunkSize <= 0) {
        return QStringList{text};
    }
    
    // Ensure overlap is not larger than chunk size
    if (chunkOverlap >= chunkSize) {
        chunkOverlap = chunkSize - 1;
    }
    if (chunkOverlap < 0) {
        chunkOverlap = 0;
    }
    
    // If text fits in one chunk, return it
    if (text.length() <= chunkSize) {
        return QStringList{text};
    }
    
    // Get separator hierarchy based on file type
    QStringList separators = getSeparatorsForType(fileType);
    
    return splitRecursive(text, chunkSize, chunkOverlap, separators, fileType);
}

// Helper function to find a word boundary for smart cutting
// Searches backward from 'idealPos' up to 'maxLookback' characters to find a space or newline
// Returns the adjusted position, or idealPos if no boundary found
static int findWordBoundary(const QString& text, int idealPos, int maxLookback)
{
    // Don't search beyond the start of the text
    int searchStart = qMax(0, idealPos - maxLookback);
    
    // Search backward from idealPos
    for (int i = idealPos - 1; i >= searchStart; --i) {
        QChar c = text[i];
        if (c == ' ' || c == '\n') {
            // Found a word boundary, split after this character
            return i + 1;
        }
    }
    
    // No boundary found, use the ideal position
    return idealPos;
}

// Helper function to extract overlap with word boundary protection
// Instead of blindly taking the last N chars, find a word boundary
static QString extractOverlapSmart(const QString& chunk, int overlapSize)
{
    if (chunk.length() <= overlapSize) {
        return chunk;
    }
    
    // Ideal start position for overlap
    int idealStart = chunk.length() - overlapSize;
    
    // Search BACKWARD from idealStart to find a space or newline
    // This ensures we start the overlap at the beginning of a word, not mid-word
    // Search up to 50 chars backward
    int searchStart = qMax(0, idealStart - 50);
    
    for (int i = idealStart - 1; i >= searchStart; --i) {
        QChar c = chunk[i];
        if (c == ' ' || c == '\n') {
            // Found word boundary, start overlap after this character
            return chunk.mid(i + 1);
        }
    }
    
    // No boundary found within search range, use the ideal position
    return chunk.right(overlapSize);
}

QStringList TextChunker::splitRecursive(
    const QString& text,
    int chunkSize,
    int chunkOverlap,
    const QStringList& separators,
    FileType fileType)
{
    // If text is small enough, return it
    if (text.length() <= chunkSize) {
        return QStringList{text};
    }
    
    // If no separators left, force split by characters
    if (separators.isEmpty()) {
        QStringList result;
        int pos = 0;
        QString overlap;
        
        while (pos < text.length()) {
            QString chunk;
            
            // Add overlap from previous chunk
            if (!overlap.isEmpty()) {
                chunk = overlap;
            }
            
            // Add new content up to chunkSize with smart cut (word boundary protection)
            int remaining = chunkSize - chunk.length();
            if (remaining > 0) {
                int idealEnd = pos + remaining;
                int actualEnd = idealEnd;
                
                // If we're not at the end of text, try to find a word boundary
                if (idealEnd < text.length()) {
                    // Search backward up to 50 chars for a space or newline
                    actualEnd = findWordBoundary(text, idealEnd, 50);
                    
                    // Make sure we're making progress (avoid infinite loop)
                    // If the boundary is before or at our current position, use ideal position
                    if (actualEnd <= pos) {
                        actualEnd = idealEnd;
                    }
                }
                
                int actualLength = actualEnd - pos;
                chunk += text.mid(pos, actualLength);
                pos = actualEnd;
            }
            
            if (!chunk.isEmpty()) {
                result.append(chunk);
                
                // Prepare overlap for next chunk with word boundary protection
                if (chunkOverlap > 0 && chunk.length() > chunkOverlap) {
                    overlap = extractOverlapSmart(chunk, chunkOverlap);
                } else {
                    overlap = chunk;
                }
            }
        }
        
        return result;
    }
    
    // Try current separator
    QString separator = separators.first();
    QStringList remainingSeparators = separators.mid(1);
    
    QStringList splits;
    
    if (separator.isEmpty()) {
        // Empty separator means split by character
        for (int i = 0; i < text.length(); ++i) {
            splits.append(text.mid(i, 1));
        }
    } else {
        // Split by the separator
        splits = text.split(separator, Qt::KeepEmptyParts);
    }
    
    // Process each split - if any split is too large, recursively split it
    QStringList processedSplits;
    for (const QString& split : splits) {
        if (split.length() > chunkSize) {
            // This split is too large, recursively split it with next separator
            QStringList subSplits = splitRecursive(split, chunkSize, chunkOverlap, remainingSeparators, fileType);
            processedSplits.append(subSplits);
        } else {
            processedSplits.append(split);
        }
    }
    
    // Merge the splits into chunks respecting size and overlap
    return mergeSplits(processedSplits, chunkSize, chunkOverlap, separator, fileType);
}

QStringList TextChunker::mergeSplits(
    const QStringList& splits,
    int chunkSize,
    int chunkOverlap,
    const QString& separator,
    FileType fileType)
{
    QStringList result;
    QString currentChunk;
    
    for (int i = 0; i < splits.size(); ++i) {
        const QString& split = splits[i];
        
        // Comment Glue: Check if this split starts with a comment
        // If it does, try harder to keep it with the previous content
        bool isComment = (separator == "\n" || separator == "\n\n") && 
                        fileType != FileType::PlainText && 
                        isCommentStart(split, fileType);
        
        // Calculate what the chunk would be if we add this split
        QString candidateChunk = currentChunk;
        
        if (!candidateChunk.isEmpty() && !split.isEmpty()) {
            // Add separator between splits (but not if separator was empty string)
            if (!separator.isEmpty()) {
                candidateChunk += separator;
            }
        }
        
        candidateChunk += split;
        
        // Check if adding this split would exceed the chunk size
        if (candidateChunk.length() <= chunkSize) {
            // It fits, update current chunk
            currentChunk = candidateChunk;
        } else {
            // It doesn't fit
            if (!currentChunk.isEmpty()) {
                // Comment Glue: If the next split is a comment, try to avoid splitting
                // by emitting the current chunk without this split
                if (isComment && i + 1 < splits.size()) {
                    // Emit current chunk and include the comment with the next section
                    result.append(currentChunk);
                    
                    // Start new chunk with overlap (word boundary protected)
                    if (chunkOverlap > 0 && currentChunk.length() > chunkOverlap) {
                        currentChunk = extractOverlapSmart(currentChunk, chunkOverlap);
                    } else {
                        currentChunk = QString();
                    }
                    
                    // Add the comment split to the new chunk
                    if (!currentChunk.isEmpty() && !separator.isEmpty()) {
                        currentChunk += separator;
                    }
                    currentChunk += split;
                } else {
                    // Normal splitting behavior
                    result.append(currentChunk);
                    
                    // Start new chunk with overlap (word boundary protected)
                    if (chunkOverlap > 0 && currentChunk.length() > chunkOverlap) {
                        currentChunk = extractOverlapSmart(currentChunk, chunkOverlap);
                        
                        // Check for exact duplication: if overlap equals the split, skip it
                        // This happens when the split is an overlap chunk from a previous recursive pass
                        if (currentChunk == split) {
                            // Don't add the split, it's already captured in overlap
                            // Just continue with current overlap as the chunk
                        } else {
                            // Try to add the current split to the new chunk with overlap
                            candidateChunk = currentChunk;
                            if (!candidateChunk.isEmpty() && !split.isEmpty() && !separator.isEmpty()) {
                                candidateChunk += separator;
                            }
                            candidateChunk += split;
                            
                            if (candidateChunk.length() <= chunkSize) {
                                currentChunk = candidateChunk;
                            } else {
                                // Even with overlap, the split doesn't fit
                                // Don't emit tiny overlap-only chunks; just start fresh with the split
                                currentChunk = split;
                            }
                        }
                    } else {
                        // No overlap, start fresh
                        currentChunk = split;
                    }
                }
            } else {
                // Current chunk is empty, just use the split
                currentChunk = split;
            }
        }
    }
    
    // Don't forget the last chunk
    if (!currentChunk.isEmpty()) {
        result.append(currentChunk);
    }
    
    return result;
}
