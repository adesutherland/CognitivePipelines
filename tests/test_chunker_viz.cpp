//
// Cognitive Pipeline Application
//
// Visual Inspection Harness for TextChunker
// Run this via 'unit_tests' and check stderr for output.
//

#include <gtest/gtest.h>
#include <QDir>
#include <QFileInfo>
#include <iostream>
#include <vector>
#include <algorithm>

#include "core/TextChunker.h"
#include "core/DocumentLoader.h"

// Helper to print a visible separator line
void printSeparator(const std::string& title) {
    std::cerr << "\n================================================================================\n";
    std::cerr << "  " << title << "\n";
    std::cerr << "================================================================================\n";
}

// Helper to print a chunk with visible boundaries
void printChunk(int index, const QString& content) {
    std::string utf8 = content.toStdString();
    // Replace newlines with a visible symbol for inspection, or keep them for structure check
    // We keep them to verify code block structure preservation
    std::cerr << "\n[CHUNK " << index << "] (" << content.length() << " chars)\n";
    std::cerr << "----------------------------------------\n";
    std::cerr << utf8 << "\n";
    std::cerr << "----------------------------------------\n";
}

class ChunkerVizTest : public ::testing::Test {
protected:
    const QString testDataDir = "tests/test_data";
    const int CHUNK_SIZE = 500;
    const int CHUNK_OVERLAP = 50;
};

TEST_F(ChunkerVizTest, VisualizeBowieCorpus) {
    // List of files to inspect (order matters for report consistency)
    std::vector<QString> files = {
        "starman.txt",
        "bowie.md",
        "ziggie.cpp",
        "major_tom.py",
        "golden_years.sql",
        "heroes.rexx",
        "fame.cob"
    };

    QDir dir(testDataDir);
    if (!dir.exists()) {
        // Try looking one level up (for build dir vs root execution)
        dir.setPath("../tests/test_data");
    }
    
    if (!dir.exists()) {
        // Only fail if we absolutely can't find the data
        std::cerr << "[WARN] Could not find tests/test_data directory. Skipping visualization.\n";
        return;
    }

    for (const auto& filename : files) {
        QString fullPath = dir.filePath(filename);
        QFileInfo info(fullPath);
        
        if (!info.exists()) {
            std::cerr << "[WARN] File not found: " << filename.toStdString() << "\n";
            continue;
        }

        // 1. Load Content
        QString content = DocumentLoader::readTextFile(fullPath);
        
        // 2. Detect Type
        FileType type = DocumentLoader::getFileTypeFromExtension(fullPath);
        
        // 3. Chunk
        QStringList chunks = TextChunker::split(content, CHUNK_SIZE, CHUNK_OVERLAP, type);

        // 4. Visualize
        printSeparator("FILE: " + filename.toStdString());
        std::cerr << "Detected Type: " << static_cast<int>(type) << "\n";
        std::cerr << "Total Length:  " << content.length() << " chars\n";
        std::cerr << "Chunk Count:   " << chunks.size() << "\n";

        for (int i = 0; i < chunks.size(); ++i) {
            printChunk(i, chunks[i]);
        }
    }
}
