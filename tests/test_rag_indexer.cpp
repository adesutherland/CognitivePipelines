//
// Cognitive Pipeline Application - RagIndexerNode Unit Test
//
// Copyright (c) 2025 Adrian Sutherland
//
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDebug>

#include "RagIndexerNode.h"
#include "core/DocumentLoader.h"
#include "core/TextChunker.h"
#include "core/RagUtils.h"

/**
 * @brief Test suite for RagIndexerNode
 *
 * This test creates a temporary directory with sample files,
 * runs the indexer, and verifies the database contents.
 */
class RagIndexerNodeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure QCoreApplication exists for Qt features
        if (!QCoreApplication::instance()) {
            int argc = 0;
            char** argv = nullptr;
            app = new QCoreApplication(argc, argv);
        }
    }

    void TearDown() override {
        // Clean up database connections
        QSqlDatabase::removeDatabase(QStringLiteral("test_rag_db"));
    }

    QCoreApplication* app {nullptr};
};

/**
 * @brief Test basic indexing flow with a small text file
 */
TEST_F(RagIndexerNodeTest, IndexesSingleTextFile) {
    // Create temporary directory
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    // Create a sample text file
    QString testFilePath = tempDir.path() + QStringLiteral("/sample.txt");
    QFile testFile(testFilePath);
    ASSERT_TRUE(testFile.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&testFile);
    out << "This is a test document for RAG indexing.\n";
    out << "It contains multiple sentences to test chunking.\n";
    out << "The indexer should split this into manageable chunks.\n";
    testFile.close();

    // Create temporary database file
    QTemporaryDir dbDir;
    ASSERT_TRUE(dbDir.isValid());
    QString dbPath = dbDir.path() + QStringLiteral("/test_rag.db");

    // Create RagIndexerNode
    RagIndexerNode indexer;
    indexer.setDirectoryPath(tempDir.path());
    indexer.setDatabasePath(dbPath);
    indexer.setIndexMetadata(QStringLiteral("{\"status\": \"test\"}"));
    indexer.setChunkSize(100);  // Small chunk size for testing
    indexer.setChunkOverlap(20);

    // Set provider and model - credentials will be automatically resolved via LLMProviderRegistry
    indexer.setProviderId(QStringLiteral("openai"));
    indexer.setModelId(QStringLiteral("text-embedding-3-small"));

    // Execute the indexer via V3 token API
    DataPacket inputs;
    inputs.insert(QString::fromLatin1(RagIndexerNode::kInputDirectoryPath), tempDir.path());
    inputs.insert(QString::fromLatin1(RagIndexerNode::kInputDatabasePath), dbPath);
    inputs.insert(QString::fromLatin1(RagIndexerNode::kInputMetadata), QStringLiteral("{\"status\": \"test\"}"));

    ExecutionToken token;
    token.data = inputs;
    TokenList tokens;
    tokens.push_back(std::move(token));

    const TokenList outTokens = indexer.execute(tokens);
    ASSERT_FALSE(outTokens.empty());
    DataPacket output = outTokens.front().data;

    // Verify outputs
    ASSERT_TRUE(output.contains(QString::fromLatin1(RagIndexerNode::kOutputDatabasePath)));
    ASSERT_TRUE(output.contains(QString::fromLatin1(RagIndexerNode::kOutputCount)));

    QString outputDbPath = output[QString::fromLatin1(RagIndexerNode::kOutputDatabasePath)].toString();
    int chunkCount = output[QString::fromLatin1(RagIndexerNode::kOutputCount)].toInt();

    EXPECT_EQ(outputDbPath, dbPath);
    
    // If chunkCount is 0, it means credentials were not available via LLMProviderRegistry
    // (either OPENAI_API_KEY env var or accounts.json in platform-specific app data directory)
    if (chunkCount == 0) {
        GTEST_SKIP() << "No API credentials available. Set OPENAI_API_KEY environment variable "
                     << "or create accounts.json in the platform-specific application data directory "
                     << "(e.g., ~/Library/Application Support/CognitivePipelines/accounts.json on macOS) "
                     << "to run this test.";
    }
    
    EXPECT_GT(chunkCount, 0) << "Should have indexed at least one chunk";

    // Verify database contents
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("test_rag_db"));
    db.setDatabaseName(dbPath);
    ASSERT_TRUE(db.open()) << "Failed to open database: " << db.lastError().text().toStdString();

    // Query fragments table
    QSqlQuery query(db);
    ASSERT_TRUE(query.exec(QStringLiteral("SELECT COUNT(*) FROM fragments")));
    ASSERT_TRUE(query.next());
    int dbChunkCount = query.value(0).toInt();
    EXPECT_EQ(dbChunkCount, chunkCount);

    // Verify source_files table has the file with provider and model
    ASSERT_TRUE(query.exec(QStringLiteral("SELECT file_path, provider, model, metadata FROM source_files LIMIT 1")));
    ASSERT_TRUE(query.next());
    
    QString filePath = query.value(0).toString();
    QString provider = query.value(1).toString();
    QString model = query.value(2).toString();
    QString metadata = query.value(3).toString();
    
    EXPECT_FALSE(filePath.isEmpty());
    EXPECT_EQ(provider, QStringLiteral("openai"));
    EXPECT_EQ(model, QStringLiteral("text-embedding-3-small"));
    EXPECT_EQ(metadata, QStringLiteral("{\"status\": \"test\"}"));

    // Verify a sample fragment has all required fields (using JOIN to get file_path)
    ASSERT_TRUE(query.exec(QStringLiteral(
        "SELECT f.file_id, f.chunk_index, f.content, f.embedding, sf.file_path "
        "FROM fragments f "
        "JOIN source_files sf ON f.file_id = sf.id "
        "LIMIT 1"
    )));
    ASSERT_TRUE(query.next());

    qint64 fileId = query.value(0).toLongLong();
    int chunkIndex = query.value(1).toInt();
    QString content = query.value(2).toString();
    QByteArray embedding = query.value(3).toByteArray();
    QString fragmentFilePath = query.value(4).toString();

    EXPECT_GT(fileId, 0) << "file_id should be a valid foreign key";
    EXPECT_GE(chunkIndex, 0);
    EXPECT_FALSE(content.isEmpty());
    EXPECT_FALSE(embedding.isEmpty()) << "Embedding should not be empty";
    EXPECT_FALSE(fragmentFilePath.isEmpty());

    // Verify embedding is a valid float array
    EXPECT_EQ(embedding.size() % sizeof(float), 0) << "Embedding size should be multiple of sizeof(float)";
    int vectorSize = embedding.size() / sizeof(float);
    EXPECT_GT(vectorSize, 0) << "Embedding vector should have elements";

    db.close();
}

/**
 * @brief Test that indexer handles empty directory gracefully
 */
TEST_F(RagIndexerNodeTest, HandlesEmptyDirectory) {
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    QTemporaryDir dbDir;
    ASSERT_TRUE(dbDir.isValid());
    QString dbPath = dbDir.path() + QStringLiteral("/test_rag_empty.db");

    RagIndexerNode indexer;
    indexer.setDirectoryPath(tempDir.path());
    indexer.setDatabasePath(dbPath);
    indexer.setProviderId(QStringLiteral("openai"));  // Won't be used since directory is empty

    DataPacket inputs;
    ExecutionToken token;
    token.data = inputs;
    TokenList tokens;
    tokens.push_back(std::move(token));

    const TokenList outTokens = indexer.execute(tokens);
    ASSERT_FALSE(outTokens.empty());
    DataPacket output = outTokens.front().data;

    ASSERT_TRUE(output.contains(QString::fromLatin1(RagIndexerNode::kOutputCount)));
    int chunkCount = output[QString::fromLatin1(RagIndexerNode::kOutputCount)].toInt();
    EXPECT_EQ(chunkCount, 0) << "Empty directory should produce 0 chunks";
}

/**
 * @brief Test saveState and loadState for persistence
 */
TEST_F(RagIndexerNodeTest, SavesAndLoadsState) {
    RagIndexerNode node;
    node.setDirectoryPath(QStringLiteral("/test/dir"));
    node.setDatabasePath(QStringLiteral("/test/db.sqlite"));
    node.setIndexMetadata(QStringLiteral("{\"tag\": \"baseline\"}"));
    node.setProviderId(QStringLiteral("openai"));
    node.setModelId(QStringLiteral("text-embedding-3-large"));
    node.setChunkSize(2000);
    node.setChunkOverlap(300);

    QJsonObject state = node.saveState();

    RagIndexerNode node2;
    node2.loadState(state);

    EXPECT_EQ(node2.directoryPath(), QStringLiteral("/test/dir"));
    EXPECT_EQ(node2.databasePath(), QStringLiteral("/test/db.sqlite"));
    EXPECT_EQ(node2.indexMetadata(), QStringLiteral("{\"tag\": \"baseline\"}"));
    EXPECT_EQ(node2.providerId(), QStringLiteral("openai"));
    EXPECT_EQ(node2.modelId(), QStringLiteral("text-embedding-3-large"));
    EXPECT_EQ(node2.chunkSize(), 2000);
    EXPECT_EQ(node2.chunkOverlap(), 300);
}

/**
 * @brief Test that file filter correctly excludes non-matching files
 */
TEST_F(RagIndexerNodeTest, FileFilterExcludesNonMatchingFiles) {
    // Create temporary directory
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    // Create a .txt file
    QString txtFilePath = tempDir.path() + QStringLiteral("/sample.txt");
    QFile txtFile(txtFilePath);
    ASSERT_TRUE(txtFile.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream txtOut(&txtFile);
    txtOut << "This is a text file that should be indexed.\n";
    txtFile.close();

    // Create a .md file
    QString mdFilePath = tempDir.path() + QStringLiteral("/sample.md");
    QFile mdFile(mdFilePath);
    ASSERT_TRUE(mdFile.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream mdOut(&mdFile);
    mdOut << "# This is a markdown file that should be EXCLUDED.\n";
    mdFile.close();

    // Create temporary database
    QTemporaryDir dbDir;
    ASSERT_TRUE(dbDir.isValid());
    QString dbPath = dbDir.path() + QStringLiteral("/test_rag_filter.db");

    // Create RagIndexerNode with file filter for *.txt only
    RagIndexerNode indexer;
    indexer.setDirectoryPath(tempDir.path());
    indexer.setDatabasePath(dbPath);
    indexer.setIndexMetadata(QStringLiteral("{\"status\": \"filter_test\"}"));
    indexer.setChunkSize(100);
    indexer.setChunkOverlap(20);
    indexer.setFileFilter(QStringLiteral("*.txt"));  // Only .txt files
    indexer.setProviderId(QStringLiteral("openai"));
    indexer.setModelId(QStringLiteral("text-embedding-3-small"));

    // Execute the indexer via V3 token API
    DataPacket inputs;
    ExecutionToken token;
    token.data = inputs;
    TokenList tokens;
    tokens.push_back(std::move(token));

    const TokenList outTokens = indexer.execute(tokens);
    ASSERT_FALSE(outTokens.empty());
    DataPacket output = outTokens.front().data;

    // Verify outputs
    ASSERT_TRUE(output.contains(QString::fromLatin1(RagIndexerNode::kOutputCount)));
    int chunkCount = output[QString::fromLatin1(RagIndexerNode::kOutputCount)].toInt();

    // Skip test if no credentials available
    if (chunkCount == 0) {
        GTEST_SKIP() << "No API credentials available. Set OPENAI_API_KEY environment variable "
                     << "or create accounts.json to run this test.";
    }

    EXPECT_GT(chunkCount, 0) << "Should have indexed at least one chunk from the .txt file";

    // Verify database contents - only the .txt file should be present
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("test_filter_db"));
    db.setDatabaseName(dbPath);
    ASSERT_TRUE(db.open()) << "Failed to open database: " << db.lastError().text().toStdString();

    // Query source_files table - should have exactly 1 entry
    QSqlQuery query(db);
    ASSERT_TRUE(query.exec(QStringLiteral("SELECT COUNT(*) FROM source_files")));
    ASSERT_TRUE(query.next());
    int fileCount = query.value(0).toInt();
    EXPECT_EQ(fileCount, 1) << "Should have indexed exactly 1 file (the .txt file)";

    // Verify that the indexed file is the .txt file, not the .md file
    ASSERT_TRUE(query.exec(QStringLiteral("SELECT file_path FROM source_files")));
    ASSERT_TRUE(query.next());
    QString indexedFilePath = query.value(0).toString();
    EXPECT_TRUE(indexedFilePath.endsWith(QStringLiteral(".txt"))) 
        << "Indexed file should be .txt, got: " << indexedFilePath.toStdString();
    EXPECT_FALSE(indexedFilePath.endsWith(QStringLiteral(".md"))) 
        << "Should not have indexed .md file";

    db.close();
    QSqlDatabase::removeDatabase(QStringLiteral("test_filter_db"));
}
