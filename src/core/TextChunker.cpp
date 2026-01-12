#include "TextChunker.h"

#include "chunkingstrategies/MarkdownChunker.h"
#include "chunkingstrategies/StandardCodeChunker.h"

#include <memory>

QStringList TextChunker::split(const QString &text, int chunkSize, int chunkOverlap,
                               FileType fileType)
{
    std::unique_ptr<ChunkerStrategy> strategy;

    if (fileType == FileType::CodeMarkdown) {
        strategy = std::make_unique<MarkdownChunker>(chunkSize, chunkOverlap);
    } else {
        strategy = std::make_unique<StandardCodeChunker>(chunkSize, chunkOverlap, fileType);
    }

    return strategy->chunk(text);
}
