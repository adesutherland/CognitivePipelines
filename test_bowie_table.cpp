#include <QCoreApplication>
#include <QFile>
#include <QDebug>
#include <QTextStream>
#include "src/core/TextChunker.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    // Read bowie.md
    QFile file("tests/test_data/bowie.md");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "ERROR: Could not open bowie.md";
        return 1;
    }
    
    QTextStream in(&file);
    QString text = in.readAll();
    file.close();
    
    qDebug() << "Input text length:" << text.length();
    
    // Chunk with reasonable size
    int chunkSize = 1000;
    int chunkOverlap = 100;
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap, FileType::CodeMarkdown);
    
    qDebug() << "\n=== VERIFICATION RESULTS ===";
    qDebug() << "Total chunks:" << chunks.size();
    
    // Check 1: Find which chunk contains the table
    int tableChunkIndex = -1;
    QString tableStart = "| Album Title | Year |";
    
    for (int i = 0; i < chunks.size(); ++i) {
        if (chunks[i].contains(tableStart)) {
            tableChunkIndex = i;
            break;
        }
    }
    
    if (tableChunkIndex >= 0) {
        qDebug() << "\n[CHECK 1: TABLE INTEGRITY]";
        qDebug() << "Table found in chunk" << tableChunkIndex;
        
        // Verify all table rows are in the same chunk
        QStringList tableRows = {
            "*The Rise and Fall of Ziggy Stardust...*",
            "*Young Americans*",
            "*Low*",
            "*Let's Dance*",
            "*Blackstar*"
        };
        
        bool allRowsInChunk = true;
        for (const QString& row : tableRows) {
            if (!chunks[tableChunkIndex].contains(row)) {
                qDebug() << "  ERROR: Missing row:" << row;
                allRowsInChunk = false;
            }
        }
        
        if (allRowsInChunk) {
            qDebug() << "  ✓ PASS: All table rows are in the same chunk";
        } else {
            qDebug() << "  ✗ FAIL: Table is split across chunks";
        }
    } else {
        qDebug() << "\n[CHECK 1: TABLE INTEGRITY]";
        qDebug() << "  ✗ FAIL: Table not found in any chunk";
    }
    
    // Check 2: Verify no spurious ``` characters
    qDebug() << "\n[CHECK 2: GHOST FENCE DETECTION]";
    int inputBacktickCount = text.count("```");
    int totalOutputBackticks = 0;
    
    for (const QString& chunk : chunks) {
        totalOutputBackticks += chunk.count("```");
    }
    
    qDebug() << "  Input ``` count:" << inputBacktickCount;
    qDebug() << "  Output ``` count:" << totalOutputBackticks;
    
    if (totalOutputBackticks == inputBacktickCount) {
        qDebug() << "  ✓ PASS: No spurious ``` characters added";
    } else {
        qDebug() << "  ✗ FAIL: Ghost fences detected!";
    }
    
    // Check 3: Verify header markers are preserved
    qDebug() << "\n[CHECK 3: HEADER PRESERVATION]";
    QStringList headers = {"# 🎸 The Chameleonic Legacy", "## 🌟 Section 1:", "## 🎧 Section 2:", "## 📈 Key Albums"};
    bool allHeadersPreserved = true;
    
    for (const QString& header : headers) {
        bool found = false;
        for (const QString& chunk : chunks) {
            if (chunk.contains(header)) {
                found = true;
                break;
            }
        }
        if (!found) {
            qDebug() << "  ERROR: Missing header:" << header;
            allHeadersPreserved = false;
        }
    }
    
    if (allHeadersPreserved) {
        qDebug() << "  ✓ PASS: All header markers preserved";
    } else {
        qDebug() << "  ✗ FAIL: Some headers lost their markers";
    }
    
    // Print chunks for inspection
    qDebug() << "\n=== CHUNK DETAILS ===";
    for (int i = 0; i < chunks.size(); ++i) {
        qDebug() << "\n[CHUNK" << i << "] Length:" << chunks[i].length();
        qDebug().noquote() << "---START---";
        qDebug().noquote() << chunks[i].left(200) + (chunks[i].length() > 200 ? "..." : "");
        qDebug().noquote() << "---END---";
    }
    
    return 0;
}
