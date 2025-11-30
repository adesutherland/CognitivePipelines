#ifndef MARKDOWNCHUNKER_H
#define MARKDOWNCHUNKER_H

#include "chunkingstrategies/ChunkerStrategy.h"

/**
 * @brief Chunking strategy specialized for Markdown documents.
 *
 * Rules implemented:
 *  - Header hard-split (priority 0): headers (#, ##, ...) start new chunks
 *    when there is existing buffered content.
 *  - Paragraph-aware accumulation (priority 1): blank lines naturally form
 *    paragraph boundaries because we operate on a line-by-line model.
 *  - Table-aware splitting (priority 2): consecutive Markdown table rows
 *    (lines starting with '|') keep their real newlines so row structure is
 *    preserved. The chunker will try to keep the entire table within a
 *    single chunk, allowing a small overflow window, but when a split is
 *    unavoidable it happens *between* rows (at the table newline) rather than
 *    inside a cell.
 *  - Standard accumulation (priority 3): other lines are appended until
 *    maxChunkSize is reached, then overlap-aware splitting is applied via
 *    extractOverlapSmart().
 */
class MarkdownChunker : public ChunkerStrategy
{
public:
    MarkdownChunker(int maxChunkSize, int chunkOverlap)
        : ChunkerStrategy(maxChunkSize, chunkOverlap)
    {
    }

    QStringList chunk(const QString &text) override;

private:
    static bool isHeaderLine(const QString &line);
};

#endif // MARKDOWNCHUNKER_H
