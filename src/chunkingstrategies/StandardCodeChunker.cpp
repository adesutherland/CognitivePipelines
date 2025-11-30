#include "chunkingstrategies/StandardCodeChunker.h"

#include <QStringList>

QStringList StandardCodeChunker::getSeparatorsForType(FileType fileType)
{
    switch (fileType) {
    case FileType::CodeCpp:
        return QStringList{"}\n\n", "}\n", ";\n", "{\n", "\n\n", "\n", " ", ""};
    case FileType::CodePython:
        // Prefer structural paragraph and line boundaries; avoid using
        // "\ndef " as a destructive separator because it strips the
        // "def" keyword from function definitions when re-assembling
        // chunks, which breaks readability and tests that expect intact
        // function bodies.
        return QStringList{"\nclass ", "\n\n", "\n", " ", ""};
    case FileType::CodeRexx:
        // Rexx: preserve structural boundaries around directives, labels and
        // common flow-control keywords. Keywords in Rexx are case-insensitive,
        // so we include the most common capitalisations used in our test
        // corpus (e.g., "Return", "Exit"). Function headers like
        // "label: Procedure" are kept intact by the comment-glue logic and
        // newline handling rather than by treating "Procedure" itself as a
        // hard separator.
        //
        // Note: label separators (":\n") are placed *after* newline
        // separators in the hierarchy so that leading comments and their
        // following headers (e.g., "/* Routine: foo */" + "foo: Procedure")
        // are first considered as cohesive blocks at the line level. Labels
        // are then used only to further split oversized segments.
        return QStringList{
            "\n::routine", "\n::ROUTINE",
            "\n::method",  "\n::METHOD",
            "\n::requires","\n::REQUIRES",
            // Flow control / termination.
            " Return\n",  " RETURN\n",
            " return\n",
            " Exit\n",    " EXIT\n",
            " exit\n",
            "\n\n",
            "\n",
            // Bare labels such as "MyLabel:" on their own line.
            ":\n",
            " ", ""};
    case FileType::CodeSql:
        return QStringList{"\n/\n", ";\n\n", ";\n", "\nGO\n", "\n\n", "\n", " ", ""};
    case FileType::CodeShell:
        return QStringList{"\nfunction ", "}\n\n", "}\n", ";;\n", "\n\n", "\n", " ", ""};
    case FileType::CodeCobol:
        return QStringList{"\nDIVISION.", "\nSECTION.", ".\n\n", ".\n", "\n\n", "\n", " ", ""};
    case FileType::CodeMarkdown:
        return QStringList{"\n\n", "\n", " ", ""};
    case FileType::CodeYaml:
        return QStringList{"\nresource ", "\nmodule ", "\n- ", "\n  ", "\n\n", "\n", " ", ""};
    case FileType::PlainText:
    default:
        return QStringList{"\n\n", "\n", " ", ""};
    }
}

bool StandardCodeChunker::isCommentStart(const QString &line, FileType fileType)
{
    QString trimmed = line.trimmed();

    switch (fileType) {
    case FileType::CodeCpp:
        return trimmed.startsWith("//") || trimmed.startsWith("/*");
    case FileType::CodePython:
        return trimmed.startsWith("#");
    case FileType::CodeRexx:
        return trimmed.startsWith("--") || trimmed.startsWith("/*");
    case FileType::CodeSql:
        return trimmed.startsWith("--");
    case FileType::CodeShell:
        return trimmed.startsWith("#");
    case FileType::CodeCobol:
        return trimmed.startsWith("*");
    case FileType::CodeMarkdown:
        return false;
    case FileType::CodeYaml:
        return trimmed.startsWith("#");
    case FileType::PlainText:
    default:
        return false;
    }
}

bool StandardCodeChunker::isMarkdownHeader(const QString &line)
{
    QString trimmed = line.trimmed();

    if (trimmed.startsWith("#")) {
        int hashCount = 0;
        for (int i = 0; i < trimmed.length() && trimmed[i] == '#'; ++i) {
            hashCount++;
        }
        if (hashCount >= 1 && hashCount <= 6) {
            if (hashCount == trimmed.length() || trimmed[hashCount] == ' ') {
                return true;
            }
        }
    }
    return false;
}

QStringList StandardCodeChunker::chunk(const QString &text)
{
    if (text.isEmpty()) {
        return QStringList();
    }

    if (max_chunk_size_ <= 0) {
        return QStringList{text};
    }

    int effectiveChunkSize = max_chunk_size_;
    int effectiveOverlap = chunk_overlap_;
    if (effectiveOverlap >= effectiveChunkSize) {
        effectiveOverlap = effectiveChunkSize - 1;
    }
    if (effectiveOverlap < 0) {
        effectiveOverlap = 0;
    }

    if (text.length() <= effectiveChunkSize) {
        return QStringList{text};
    }

    QStringList separators = getSeparatorsForType(file_type_);
    return splitRecursive(text, effectiveChunkSize, effectiveOverlap, separators);
}

QStringList StandardCodeChunker::splitRecursive(const QString &text,
                                                int chunkSize,
                                                int chunkOverlap,
                                                const QStringList &separators) const
{
    if (text.length() <= chunkSize) {
        return QStringList{text};
    }

    if (separators.isEmpty()) {
        QStringList result;
        int pos = 0;
        QString overlap;

        while (pos < text.length()) {
            QString chunk;

            if (!overlap.isEmpty()) {
                chunk = overlap;
            }

            int remaining = chunkSize - chunk.length();
            if (remaining > 0) {
                int idealEnd = pos + remaining;
                int actualEnd = idealEnd;

                if (idealEnd < text.length()) {
                    actualEnd = ChunkerStrategy::findWordBoundary(text, idealEnd, 50);
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

                if (chunkOverlap > 0 && chunk.length() > chunkOverlap) {
                    overlap = ChunkerStrategy::extractOverlapSmart(chunk, chunkOverlap);
                } else {
                    overlap = chunk;
                }
            }
        }

        return result;
    }

    const QString separator = separators.first();
    const QStringList remainingSeparators = separators.mid(1);

    // Split the text by the current separator into original parts.
    QStringList parts;
    if (separator.isEmpty()) {
        // Lowest level: split into single characters.
        for (int i = 0; i < text.length(); ++i) {
            parts.append(text.mid(i, 1));
        }
    } else {
        parts = text.split(separator, Qt::KeepEmptyParts);
    }

    QStringList result;
    QString currentChunk;

    for (int partIndex = 0; partIndex < parts.size(); ++partIndex) {
        const QString &part = parts[partIndex];
        const bool isLastPart = (partIndex == parts.size() - 1);

        // Decide which logical pieces we will emit for this part: either the
        // part itself (if small enough) or the sub-chunks produced by
        // recursion on remaining separators.
        QStringList pieces;
        if (part.length() > chunkSize && !remainingSeparators.isEmpty()) {
            pieces = splitRecursive(part, chunkSize, chunkOverlap, remainingSeparators);
        } else if (part.length() > chunkSize && remainingSeparators.isEmpty()) {
            // Fallback: use the character-level splitter from the base case.
            pieces = splitRecursive(part, chunkSize, chunkOverlap, remainingSeparators);
        } else {
            pieces.append(part);
        }

        for (int pieceIndex = 0; pieceIndex < pieces.size(); ++pieceIndex) {
            const QString &piece = pieces[pieceIndex];
            const bool isFirstPieceOfPart = (pieceIndex == 0);
            const bool hasNextPiece = (pieceIndex + 1 < pieces.size()) || !isLastPart;

            mergeSplits(result,
                        currentChunk,
                        piece,
                        separator,
                        chunkSize,
                        chunkOverlap,
                        isFirstPieceOfPart,
                        hasNextPiece);
        }
    }

    if (!currentChunk.isEmpty()) {
        result.append(currentChunk);
    }

    return result;
}

void StandardCodeChunker::mergeSplits(QStringList &result,
                                      QString &currentChunk,
                                      const QString &piece,
                                      const QString &separator,
                                      int chunkSize,
                                      int chunkOverlap,
                                      bool isFirstPieceOfPart,
                                      bool hasNextPiece) const
{
    if (piece.isEmpty() && !hasNextPiece) {
        return;
    }

    const bool isComment = (separator == "\n" || separator == "\n\n") &&
                           file_type_ != FileType::PlainText &&
                           isCommentStart(piece, file_type_);

    QString candidateChunk = currentChunk;

    // Golden Rule: only append the current separator when transitioning
    // between original parts. Callers therefore pass isFirstPieceOfPart to
    // indicate that we are at the start of a new part; we never insert the
    // separator between recursive sub-pieces of the same part.
    if (!candidateChunk.isEmpty() && !piece.isEmpty() && !separator.isEmpty() && isFirstPieceOfPart) {
        candidateChunk += separator;
    }

    candidateChunk += piece;

    const bool isPythonDefBoundary =
        file_type_ == FileType::CodePython && separator.startsWith("\ndef ") && isFirstPieceOfPart;

    // If the candidate still fits, accept it.
    if (candidateChunk.length() <= chunkSize) {
        currentChunk = candidateChunk;
        return;
    }

    // Python-specific behaviour: treat "\ndef " as a hard function boundary.
    // If adding a new "def" block would overflow the current chunk, flush the
    // existing chunk first and start a new one from this definition so that
    // small functions (like the ones used in tests) stay intact within a
    // single chunk when possible.
    if (isPythonDefBoundary && !currentChunk.isEmpty()) {
        result.append(currentChunk);

        if (chunkOverlap > 0 && currentChunk.length() > chunkOverlap) {
            currentChunk = ChunkerStrategy::extractOverlapSmart(currentChunk, chunkOverlap);
        } else {
            currentChunk.clear();
        }

        candidateChunk = currentChunk;
        if (!candidateChunk.isEmpty() && !separator.isEmpty()) {
            candidateChunk += separator;
        }
        candidateChunk += piece;

        if (candidateChunk.length() <= chunkSize) {
            currentChunk = candidateChunk;
        } else {
            // As a last resort, fall back to keeping just the piece; deeper
            // recursion or character-level splitting will handle oversize
            // bodies.
            currentChunk = piece;
        }

        return;
    }

    if (!currentChunk.isEmpty()) {
        // Compute information about the last logical line in the current
        // chunk. This is used for both leading-comment and trailing-comment
        // glue behaviours.
        const int lastNewline = currentChunk.lastIndexOf('\n');
        const QString lastLine = (lastNewline == -1) ? currentChunk : currentChunk.mid(lastNewline + 1);
        const bool lastLineIsComment =
            (separator == "\n" || separator == "\n\n") &&
            file_type_ != FileType::PlainText &&
            isCommentStart(lastLine, file_type_);

        // Special case (REXX regression): if the current chunk consists solely
        // of a leading comment line and the next piece is the routine/label
        // header, keep them together even when their combined size would
        // exceed the configured chunkSize. This mirrors how developers
        // logically group documentation comments with the code they describe
        // and is only applied when both pieces individually fit within the
        // size limit to avoid unbounded growth.
        if (!isComment && (separator == "\n" || separator == "\n\n") && lastLineIsComment &&
            lastNewline == -1 &&
            currentChunk.length() <= chunkSize && piece.length() <= chunkSize) {
            // Accept the oversized candidate as-is so that the leading
            // comment stays attached to the following header (e.g., REXX
            // "/* Routine: foo */" + "foo: Procedure").
            currentChunk = candidateChunk;
            return;
        }

        // Special case: if the *previous* piece in the current chunk is a
        // trailing comment line and adding this (likely code) piece would
        // overflow, migrate the comment forward so that it stays attached to
        // the code it documents.
        if (!isComment && (separator == "\n" || separator == "\n\n") && hasNextPiece &&
            lastLineIsComment && lastNewline != -1) {
            const QString chunkWithoutLastLine = currentChunk.left(lastNewline + 1);
            const QString migratedComment = lastLine;

            QString candidateWithoutComment = chunkWithoutLastLine;
            if (!candidateWithoutComment.isEmpty() && !separator.isEmpty() && isFirstPieceOfPart) {
                candidateWithoutComment += separator;
            }
            candidateWithoutComment += piece;

            if (candidateWithoutComment.length() <= chunkSize) {
                // Flush chunk without the trailing comment, start next chunk
                // from the comment plus this piece.
                result.append(chunkWithoutLastLine);

                if (chunkOverlap > 0 && chunkWithoutLastLine.length() > chunkOverlap) {
                    currentChunk =
                        ChunkerStrategy::extractOverlapSmart(chunkWithoutLastLine, chunkOverlap);
                } else {
                    currentChunk.clear();
                }

                if (!currentChunk.isEmpty() && !separator.isEmpty() && isFirstPieceOfPart) {
                    currentChunk += separator;
                }
                currentChunk += migratedComment;
                if (!separator.isEmpty()) {
                    currentChunk += separator;
                }
                currentChunk += piece;
                return;
            }
        }
        // Comment glue: keep trailing comment lines attached to the following
        // code by starting the next chunk from the comment rather than
        // leaving it orphaned at the previous boundary.
        if (isComment && hasNextPiece) {
            result.append(currentChunk);

            if (chunkOverlap > 0 && currentChunk.length() > chunkOverlap) {
                currentChunk = ChunkerStrategy::extractOverlapSmart(currentChunk, chunkOverlap);
            } else {
                currentChunk.clear();
            }

            if (!currentChunk.isEmpty() && !separator.isEmpty() && isFirstPieceOfPart) {
                currentChunk += separator;
            }
            currentChunk += piece;
        } else {
            result.append(currentChunk);

            if (chunkOverlap > 0 && currentChunk.length() > chunkOverlap) {
                currentChunk = ChunkerStrategy::extractOverlapSmart(currentChunk, chunkOverlap);

                // Deduplicate across the chunk boundary: when we seed the new
                // chunk from an overlap taken from the end of the previous
                // chunk, the very next piece is often the same logical line
                // (e.g. a SQL statement or code line). The raw overlap text
                // may already contain that line in full, so blindly
                // appending the piece would duplicate it inside the new
                // chunk. To avoid this, trim any common suffix/prefix between
                // the current overlap and the upcoming piece.

                QString adjustedPiece = piece;

                // Find the longest suffix of currentChunk that is also a
                // prefix of piece.
                int maxShared = qMin(currentChunk.length(), piece.length());
                int sharedLen = 0;
                for (int len = maxShared; len > 0; --len) {
                    if (currentChunk.right(len) == piece.left(len)) {
                        sharedLen = len;
                        break;
                    }
                }

                if (sharedLen > 0) {
                    adjustedPiece = piece.mid(sharedLen);

                    // If the piece is fully covered by the overlap, there is
                    // nothing new to append for this boundary.
                    if (adjustedPiece.isEmpty()) {
                        return;
                    }
                }

                candidateChunk = currentChunk;
                if (!candidateChunk.isEmpty() && !adjustedPiece.isEmpty() && !separator.isEmpty() &&
                    isFirstPieceOfPart) {
                    candidateChunk += separator;
                }
                candidateChunk += adjustedPiece;

                if (candidateChunk.length() <= chunkSize) {
                    currentChunk = candidateChunk;
                } else {
                    // The adjusted piece still does not fit when combined
                    // with the overlap; fall back to starting the new chunk
                    // from the (deduplicated) piece alone.
                    currentChunk = adjustedPiece;
                }
            } else {
                currentChunk = piece;
            }
        }
    } else {
        currentChunk = piece;
    }
}
