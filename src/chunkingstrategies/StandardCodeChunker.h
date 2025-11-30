#ifndef STANDARDCODECHUNKER_H
#define STANDARDCODECHUNKER_H

#include "chunkingstrategies/ChunkerStrategy.h"
#include "core/TextChunker.h"

/**
 * @brief Default chunking strategy for all non-Markdown file types.
 *
 * This class preserves the legacy TextChunker recursive splitting
 * behaviour for C++/Python/REXX/SQL/Shell/COBOL/YAML and plain text,
 * using file-type specific separator hierarchies and comment glue
 * logic.
 */
class StandardCodeChunker : public ChunkerStrategy
{
public:
    StandardCodeChunker(int maxChunkSize, int chunkOverlap, FileType type)
        : ChunkerStrategy(maxChunkSize, chunkOverlap)
        , file_type_(type)
    {
    }

    QStringList chunk(const QString &text) override;

private:
    static QStringList getSeparatorsForType(FileType fileType);
    static bool isCommentStart(const QString &line, FileType fileType);
    static bool isMarkdownHeader(const QString &line);

    QStringList splitRecursive(const QString &text,
                               int chunkSize,
                               int chunkOverlap,
                               const QStringList &separators) const;

    ///
    /// @brief Append a single logical piece of text to the current chunk list.
    ///
    /// This helper encapsulates chunk size enforcement and overlap behaviour.
    /// It is intentionally unaware of recursion topology: callers are
    /// responsible for honouring the "Golden Rule" that high-level separators
    /// are only re-inserted between original top-level parts, never between
    /// recursively produced sub-chunks.
    ///
    void mergeSplits(QStringList &result,
                     QString &currentChunk,
                     const QString &piece,
                     const QString &separator,
                     int chunkSize,
                     int chunkOverlap,
                     bool isFirstPieceOfPart,
                     bool hasNextPiece) const;

private:
    FileType file_type_;
};

#endif // STANDARDCODECHUNKER_H
