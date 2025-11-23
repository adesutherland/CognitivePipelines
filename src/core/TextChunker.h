#ifndef TEXTCHUNKER_H
#define TEXTCHUNKER_H

#include <QString>
#include <QStringList>

/**
 * @brief FileType enum for code-aware splitting strategies
 */
enum class FileType
{
    PlainText,   // Generic text, use standard separators
    CodeCpp,     // C-family: C, C++, C#, Java, JS, TS, Go, Rust, Swift, Kotlin
    CodePython,  // Python-family: Python, GDScript
    CodeRexx,    // Rexx-family: Rexx, NetRexx
    CodeSql,     // SQL-family: SQL, PL/SQL, T-SQL
    CodeShell,   // Shell-family: Bash, PowerShell, Zsh
    CodeCobol,   // Cobol-family: Cobol
    CodeMarkdown,// Markdown-family: Markdown with structure-aware splitting
    CodeYaml     // YAML/Terraform-family: YAML, Terraform, HCL
};

/**
 * @brief TextChunker implements a Recursive Character Text Splitter strategy
 *        for breaking large documents into overlapping chunks suitable for
 *        vector embedding in RAG systems.
 *
 * The algorithm respects natural text boundaries by using a hierarchy of
 * separators. For code files, it uses syntax-aware separators and implements
 * "Comment Glue" logic to keep comments attached to their associated code.
 */
class TextChunker
{
public:
    /**
     * @brief Split text into overlapping chunks using recursive character splitting.
     *
     * @param text The input text to split
     * @param chunkSize Maximum size of each chunk in characters
     * @param chunkOverlap Number of characters to overlap between consecutive chunks
     * @param fileType Type of file content (determines separator hierarchy and comment handling)
     * @return QStringList List of text chunks
     *
     * Algorithm:
     * 1. Use separator hierarchy based on fileType (code-aware for source files)
     * 2. Apply "Comment Glue" logic to keep comments with their associated code
     * 3. Accumulate splits into chunks respecting chunkSize
     * 4. When a chunk would exceed chunkSize, emit it and start a new one
     * 5. Maintain chunkOverlap characters from the end of the previous chunk
     * 6. Recursively apply next separator if a segment is still too large
     * 7. Force split at character boundary if no separator helps
     */
    static QStringList split(const QString& text, int chunkSize, int chunkOverlap, 
                            FileType fileType = FileType::PlainText);

private:
    /**
     * @brief Get the prioritized list of separators for a given file type.
     *
     * @param fileType The type of file content
     * @return QStringList List of separators in priority order
     */
    static QStringList getSeparatorsForType(FileType fileType);

    /**
     * @brief Check if a line starts with a comment marker for the given file type.
     *
     * @param line The line to check
     * @param fileType The type of file content
     * @return bool True if the line starts with a comment marker
     */
    static bool isCommentStart(const QString& line, FileType fileType);

    /**
     * @brief Internal recursive splitting helper.
     *
     * @param text Text to split
     * @param chunkSize Maximum chunk size
     * @param chunkOverlap Overlap size
     * @param separators List of separators to try in order
     * @param fileType Type of file content (for Comment Glue logic)
     * @return QStringList List of chunks
     */
    static QStringList splitRecursive(
        const QString& text,
        int chunkSize,
        int chunkOverlap,
        const QStringList& separators,
        FileType fileType
    );

    /**
     * @brief Merge small splits into chunks respecting size limits and overlap.
     *
     * @param splits List of text segments to merge
     * @param chunkSize Maximum chunk size
     * @param chunkOverlap Overlap size
     * @param separator The separator used to create these splits
     * @param fileType Type of file content (for Comment Glue logic)
     * @return QStringList List of merged chunks
     */
    static QStringList mergeSplits(
        const QStringList& splits,
        int chunkSize,
        int chunkOverlap,
        const QString& separator,
        FileType fileType
    );
};

#endif // TEXTCHUNKER_H
