#ifndef CHUNKERSTRATEGY_H
#define CHUNKERSTRATEGY_H

#include <QString>
#include <QStringList>

/**
 * @brief Abstract base class for text chunking strategies.
 *
 * Concrete strategies implement domain-specific chunking (e.g. Markdown,
 * source code) while reusing shared helper utilities such as smart
 * word-boundary detection and overlap extraction.
 */
class ChunkerStrategy
{
public:
    ChunkerStrategy(int maxChunkSize, int chunkOverlap)
        : max_chunk_size_(maxChunkSize)
        , chunk_overlap_(chunkOverlap)
    {
    }

    virtual ~ChunkerStrategy() = default;

    /**
     * @brief Split the given text into chunks according to the concrete
     *        strategy's rules.
     */
    virtual QStringList chunk(const QString &text) = 0;

protected:
    /**
     * @brief Find a natural word boundary near the given ideal position.
     *
     * Searches backwards from @p idealPos up to @p maxLookback characters for
     * a space or newline. If found, the position just after the boundary is
     * returned; otherwise @p idealPos is returned unchanged.
     */
    static int findWordBoundary(const QString &text, int idealPos, int maxLookback)
    {
        int searchStart = qMax(0, idealPos - maxLookback);
        for (int i = idealPos - 1; i >= searchStart; --i) {
            QChar c = text[i];
            if (c == ' ' || c == '\n') {
                return i + 1;
            }
        }
        return idealPos;
    }

    /**
     * @brief Extract an overlap segment from the end of a chunk while trying
     *        to start at a semantic boundary.
     *
     * This mirrors TextChunker::extractOverlapSmart: prefer newlines, then
     * sentence endings, then simple word boundaries, finally falling back to a
     * raw suffix when no boundary is found within the search window.
     */
    static QString extractOverlapSmart(const QString &chunk, int overlapSize)
    {
        if (chunk.length() <= overlapSize) {
            return chunk;
        }

        int idealStart = chunk.length() - overlapSize;
        int searchStart = qMax(0, idealStart - 150);

        // Phase 1: strong separators (newlines)
        for (int i = idealStart - 1; i >= searchStart; --i) {
            QChar c = chunk[i];
            if (c == '\n' || c == '\r') {
                int boundaryPos = i + 1;
                QString candidate = chunk.mid(boundaryPos);

                int firstWordLen = 0;
                for (int j = 0; j < candidate.length(); ++j) {
                    QChar ch = candidate[j];
                    if (ch == ' ' || ch == '\n' || ch == '\r') {
                        break;
                    }
                    if (!ch.isSpace()) {
                        firstWordLen++;
                    }
                }

                if (firstWordLen > 3 || candidate.contains(' ')) {
                    return candidate;
                }
            }
        }

        // Phase 2: weak separators (period followed by space/newline)
        for (int i = idealStart - 1; i >= searchStart; --i) {
            if (i + 1 < chunk.length()) {
                QChar c = chunk[i];
                QChar next = chunk[i + 1];
                if (c == '.' && (next == ' ' || next == '\n' || next == '\r')) {
                    int boundaryPos = i + 1;
                    while (boundaryPos < chunk.length() && chunk[boundaryPos].isSpace()) {
                        boundaryPos++;
                    }
                    if (boundaryPos < chunk.length()) {
                        QString candidate = chunk.mid(boundaryPos);
                        if (candidate.length() >= 10) {
                            return candidate;
                        }
                    }
                }
            }
        }

        // Phase 3: simple word boundaries (spaces)
        for (int i = idealStart - 1; i >= searchStart; --i) {
            QChar c = chunk[i];
            if (c == ' ') {
                int boundaryPos = i + 1;
                QString candidate = chunk.mid(boundaryPos);
                if (candidate.length() >= 10) {
                    return candidate;
                }
            }
        }

        // Fallback: raw suffix
        return chunk.right(overlapSize);
    }

protected:
    int max_chunk_size_;
    int chunk_overlap_;
};

#endif // CHUNKERSTRATEGY_H
