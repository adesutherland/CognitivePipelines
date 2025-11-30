#include "chunkingstrategies/MarkdownChunker.h"

#include <QStringList>

namespace {

static QStringList splitLinesPreserveEmpty(const QString &text)
{
    // QString::split with KeepEmptyParts preserves structure-relevant blank
    // lines which are important for Markdown semantics.
    return text.split('\n', Qt::KeepEmptyParts);
}

}

QStringList MarkdownChunker::chunk(const QString &text)
{
    QStringList chunks;

    if (text.isEmpty() || max_chunk_size_ <= 0) {
        if (!text.isEmpty()) {
            chunks.append(text);
        }
        return chunks;
    }

    // Normalise invalid overlap values similar to TextChunker::split
    int effectiveChunkSize = max_chunk_size_;
    int effectiveOverlap = chunk_overlap_;
    if (effectiveOverlap >= effectiveChunkSize) {
        effectiveOverlap = effectiveChunkSize - 1;
    }
    if (effectiveOverlap < 0) {
        effectiveOverlap = 0;
    }

    QStringList lines = splitLinesPreserveEmpty(text);
    QString currentChunk;

    // Tracks whether currentChunk only contains a single Markdown header line
    // plus optional whitespace/blank lines. Used to implement "sticky
    // headers" that always pull at least the following content line into the
    // same chunk, even if that slightly exceeds the target size.
    bool currentChunkIsHeaderOnly = false;

    // Tracks whether the previously processed line was a table row (starts
    // with '|', ignoring leading whitespace). This helps us detect that we are
    // inside a contiguous table block when deciding whether to allow a small
    // size overflow to keep the table intact.
    bool lastLineWasTableRow = false;

    const int tableMaxChunkSize = effectiveChunkSize + effectiveChunkSize / 4; // +25%

    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines[i];
        const bool isLastLine = (i == lines.size() - 1);
        const bool isHeader = isHeaderLine(line);

        // Header Hard-Split: if line is header and current buffer has content,
        // flush current chunk and start a new one from the header line. This
        // boundary is a *clean* break: there is intentionally no overlap
        // carried from the previous paragraph into the header chunk.
        if (!currentChunk.isEmpty() && isHeader) {
            chunks.append(currentChunk);
            currentChunk.clear();
            currentChunkIsHeaderOnly = false;
        }

        QString lineWithSep = line;

        // Table-aware handling: detect whether we are inside a contiguous
        // Markdown table block. We now *preserve* the real newlines between
        // table rows so that the visual structure (one row per line) is
        // retained. Chunk-size enforcement later uses these row boundaries as
        // preferred split points when a table no longer fits.
        bool isTableRow = line.trimmed().startsWith('|');
        bool nextIsTableRow = false;
        if (!isLastLine) {
            const QString &nextLine = lines[i + 1];
            nextIsTableRow = nextLine.trimmed().startsWith('|');
        }

        // Newline is always preserved; separator "priority" is encoded in
        // how we later decide to flush or overflow (tables get a larger
        // allowance via tableMaxChunkSize but still honour row boundaries).
        if (!isLastLine) {
            lineWithSep.append('\n');
        }

        QString candidate = currentChunk + lineWithSep;

        // Sticky Headers: if the current chunk so far only consists of a
        // header (and maybe blank lines), force the *next* block of text to be
        // appended to it, even if that means slightly exceeding
        // effectiveChunkSize. This prevents "lonely" tiny header-only chunks
        // that would otherwise create poor RAG context and odd overlaps.
        if (currentChunkIsHeaderOnly && candidate.length() > effectiveChunkSize) {
            currentChunk = candidate;

            // As soon as we append a non-header, non-empty line, this chunk is
            // no longer considered header-only.
            if (!line.trimmed().isEmpty() && !isHeader) {
                currentChunkIsHeaderOnly = false;
            }

            lastLineWasTableRow = isTableRow;
            continue;
        }

        if (candidate.length() <= effectiveChunkSize) {
            currentChunk = candidate;

            // Update header-only tracking: only remain header-only while we
            // see headers and blank lines; any real content clears it.
            if (currentChunk.isEmpty()) {
                currentChunkIsHeaderOnly = false;
            } else if (currentChunk.size() == lineWithSep.size()) {
                // Starting a new chunk from this line.
                currentChunkIsHeaderOnly = isHeader;
            } else if (currentChunkIsHeaderOnly && !line.trimmed().isEmpty() && !isHeader) {
                currentChunkIsHeaderOnly = false;
            }

            lastLineWasTableRow = isTableRow;
            continue;
        }

        // Allow tables to overflow the target chunk size by up to ~25% in
        // order to keep the header and rows together. This prevents splits
        // between "| Title |" and "| :-- |" lines while still keeping
        // pathological tables bounded.
        const bool inTableRegion = isTableRow || lastLineWasTableRow || nextIsTableRow;
        if (inTableRegion && candidate.length() <= tableMaxChunkSize) {
            currentChunk = candidate;
            lastLineWasTableRow = isTableRow;
            continue;
        }

        // If the line itself is larger than a chunk, perform a hard character
        // split using word-boundary aware logic.
        if (currentChunk.isEmpty() && lineWithSep.length() > effectiveChunkSize) {
            int start = 0;
            while (start < lineWithSep.length()) {
                int idealEnd = start + effectiveChunkSize;
                if (idealEnd >= lineWithSep.length()) {
                    chunks.append(lineWithSep.mid(start));
                    break;
                }

                int actualEnd = findWordBoundary(lineWithSep, idealEnd, 50);
                QString chunkPiece = lineWithSep.mid(start, actualEnd - start);
                chunks.append(chunkPiece);

                if (effectiveOverlap > 0) {
                    QString overlap = ChunkerStrategy::extractOverlapSmart(chunkPiece, effectiveOverlap);
                    start = actualEnd - overlap.length();
                } else {
                    start = actualEnd;
                }
            }
            lastLineWasTableRow = isTableRow;
            continue;
        }

        // Normal case: flush currentChunk, compute overlap, then start new chunk
        if (!currentChunk.isEmpty()) {
            chunks.append(currentChunk);

            if (effectiveOverlap > 0 && currentChunk.length() > effectiveOverlap) {
                QString overlap = ChunkerStrategy::extractOverlapSmart(currentChunk, effectiveOverlap);
                currentChunk = overlap + lineWithSep;
            } else {
                currentChunk = lineWithSep;
            }

            // Starting from an overlap or fresh line means the chunk can no
            // longer be "header only".
            currentChunkIsHeaderOnly = isHeader && currentChunk.size() == lineWithSep.size();
        } else {
            currentChunk = lineWithSep;
            currentChunkIsHeaderOnly = isHeader;
        }

        lastLineWasTableRow = isTableRow;
    }

    if (!currentChunk.isEmpty()) {
        chunks.append(currentChunk);
    }

    return chunks;
}

bool MarkdownChunker::isHeaderLine(const QString &line)
{
    QString trimmed = line.trimmed();
    if (!trimmed.startsWith('#')) {
        return false;
    }

    int hashCount = 0;
    while (hashCount < trimmed.length() && trimmed[hashCount] == '#') {
        ++hashCount;
    }

    if (hashCount < 1 || hashCount > 6) {
        return false;
    }

    if (hashCount == trimmed.length()) {
        return true;
    }

    return trimmed[hashCount] == ' ';
}
