//
// Cognitive Pipeline Application
//
// RAG Foundation tests
//

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>

#include "core/RagUtils.h"
#include "backends/OpenAIBackend.h"
#include "core/LLMProviderRegistry.h"
#include "test_app.h"

// Minimal app helper to ensure Qt application context exists
static QCoreApplication* ensureCoreApp()
{
    static QCoreApplication* app = nullptr;
    if (!app) {
        static int argc = 1;
        static char appName[] = "unit_tests";
        static char* argv[] = { appName, nullptr };
        app = new QCoreApplication(argc, argv);
    }
    return app;
}

/**
 * @brief Test that the RAG database schema creates both tables successfully.
 *
 * This test creates an in-memory SQLite database, executes the RAG schema statements,
 * and verifies that both source_files and fragments tables were created with all required columns.
 */
TEST(RagFoundationTest, SchemaCreatesFragmentsTable)
{
    ensureCoreApp();
    
    // Create an in-memory SQLite database
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("test_rag_schema"));
    db.setDatabaseName(QStringLiteral(":memory:"));
    
    ASSERT_TRUE(db.open()) << "Failed to open in-memory database: " 
                           << db.lastError().text().toStdString();
    
    // Execute the RAG schema (must execute statements separately due to SQLite limitation)
    QSqlQuery query(db);
    
    // Enable foreign keys
    ASSERT_TRUE(query.exec(QString::fromUtf8(kRagSchemaPragma))) 
        << "Failed to enable foreign keys: " << query.lastError().text().toStdString();
    
    // Create source_files table
    ASSERT_TRUE(query.exec(QString::fromUtf8(kRagSchemaSourceFiles))) 
        << "Failed to create source_files table: " << query.lastError().text().toStdString();
    
    // Create fragments table
    ASSERT_TRUE(query.exec(QString::fromUtf8(kRagSchemaFragments))) 
        << "Failed to create fragments table: " << query.lastError().text().toStdString();
    
    // Verify that the source_files table exists
    QSqlQuery sourceFilesCheckQuery(db);
    ASSERT_TRUE(sourceFilesCheckQuery.exec(
        QStringLiteral("SELECT name FROM sqlite_master WHERE type='table' AND name='source_files';")
    )) << "Failed to query sqlite_master: " << sourceFilesCheckQuery.lastError().text().toStdString();
    
    ASSERT_TRUE(sourceFilesCheckQuery.next()) << "source_files table was not created";
    EXPECT_EQ(sourceFilesCheckQuery.value(0).toString(), QStringLiteral("source_files"));
    
    // Verify source_files table structure
    QSqlQuery sourceFilesColumnQuery(db);
    ASSERT_TRUE(sourceFilesColumnQuery.exec(QStringLiteral("PRAGMA table_info(source_files);")))
        << "Failed to query source_files table_info: " << sourceFilesColumnQuery.lastError().text().toStdString();
    
    QStringList sourceFilesColumns;
    while (sourceFilesColumnQuery.next()) {
        sourceFilesColumns.append(sourceFilesColumnQuery.value(1).toString());
    }
    
    EXPECT_TRUE(sourceFilesColumns.contains(QStringLiteral("id"))) << "Missing 'id' column in source_files";
    EXPECT_TRUE(sourceFilesColumns.contains(QStringLiteral("file_path"))) << "Missing 'file_path' column in source_files";
    EXPECT_TRUE(sourceFilesColumns.contains(QStringLiteral("provider"))) << "Missing 'provider' column in source_files";
    EXPECT_TRUE(sourceFilesColumns.contains(QStringLiteral("model"))) << "Missing 'model' column in source_files";
    EXPECT_TRUE(sourceFilesColumns.contains(QStringLiteral("last_modified"))) << "Missing 'last_modified' column in source_files";
    EXPECT_TRUE(sourceFilesColumns.contains(QStringLiteral("metadata"))) << "Missing 'metadata' column in source_files";
    
    // Verify that the fragments table exists
    QSqlQuery fragmentsCheckQuery(db);
    ASSERT_TRUE(fragmentsCheckQuery.exec(
        QStringLiteral("SELECT name FROM sqlite_master WHERE type='table' AND name='fragments';")
    )) << "Failed to query sqlite_master: " << fragmentsCheckQuery.lastError().text().toStdString();
    
    ASSERT_TRUE(fragmentsCheckQuery.next()) << "fragments table was not created";
    EXPECT_EQ(fragmentsCheckQuery.value(0).toString(), QStringLiteral("fragments"));
    
    // Verify fragments table structure
    QSqlQuery fragmentsColumnQuery(db);
    ASSERT_TRUE(fragmentsColumnQuery.exec(QStringLiteral("PRAGMA table_info(fragments);")))
        << "Failed to query fragments table_info: " << fragmentsColumnQuery.lastError().text().toStdString();
    
    QStringList fragmentsColumns;
    while (fragmentsColumnQuery.next()) {
        fragmentsColumns.append(fragmentsColumnQuery.value(1).toString());
    }
    
    // Verify expected columns exist in fragments (note: file_id instead of file_path)
    EXPECT_TRUE(fragmentsColumns.contains(QStringLiteral("id"))) << "Missing 'id' column in fragments";
    EXPECT_TRUE(fragmentsColumns.contains(QStringLiteral("file_id"))) << "Missing 'file_id' column in fragments";
    EXPECT_TRUE(fragmentsColumns.contains(QStringLiteral("chunk_index"))) << "Missing 'chunk_index' column in fragments";
    EXPECT_TRUE(fragmentsColumns.contains(QStringLiteral("content"))) << "Missing 'content' column in fragments";
    EXPECT_TRUE(fragmentsColumns.contains(QStringLiteral("embedding"))) << "Missing 'embedding' column in fragments";
    
    // Clean up
    db.close();
    QSqlDatabase::removeDatabase(QStringLiteral("test_rag_schema"));
}

/**
 * @brief Test that OpenAI embeddings API returns a valid vector for a simple text input.
 *
 * This test calls the OpenAI embeddings endpoint with "Hello World" and verifies
 * that a non-empty embedding vector is returned. If no API key is available, the test
 * is skipped using GTEST_SKIP().
 */
TEST(RagFoundationTest, OpenAIEmbeddingsAPI)
{
    ensureCoreApp();
    
    // Try to get API key from environment or accounts.json
    QString apiKey = qEnvironmentVariable("OPENAI_API_KEY");
    if (apiKey.isEmpty()) {
        apiKey = LLMProviderRegistry::instance().getCredential(QStringLiteral("openai"));
    }
    
    if (apiKey.isEmpty()) {
        GTEST_SKIP() << "No OpenAI API key provided. Set OPENAI_API_KEY environment variable or add to accounts.json.";
    }
    
    // Create OpenAI backend instance
    OpenAIBackend backend;
    
    // Call getEmbedding with a simple test string
    const QString testText = QStringLiteral("Hello World");
    const QString modelName = QStringLiteral("text-embedding-3-small");
    
    EmbeddingResult result = backend.getEmbedding(apiKey, modelName, testText);
    
    // Verify no error occurred
    if (result.hasError) {
        if (isTemporaryError(result.errorMsg)) {
            GTEST_SKIP() << "Temporary LLM error during embedding: " << result.errorMsg.toStdString();
        }
        FAIL() << "Embedding request failed with error: " << result.errorMsg.toStdString();
    }
    
    // Verify that the vector is not empty
    ASSERT_FALSE(result.vector.empty()) << "Embedding vector should not be empty";
    
    // Verify that the vector has a reasonable size (OpenAI text-embedding-3-small returns 1536 dimensions)
    EXPECT_EQ(result.vector.size(), 1536) 
        << "Expected 1536 dimensions for text-embedding-3-small, got " << result.vector.size();
    
    // Verify that usage statistics are populated
    EXPECT_GT(result.usage.totalTokens, 0) << "Total tokens should be greater than 0";
    EXPECT_GT(result.usage.inputTokens, 0) << "Input tokens should be greater than 0";
    
    // Basic sanity check: verify that vector values are in a reasonable range (typically -1 to 1 for normalized embeddings)
    bool hasNonZeroValue = false;
    for (float value : result.vector) {
        EXPECT_GE(value, -10.0f) << "Vector value seems out of range (too negative)";
        EXPECT_LE(value, 10.0f) << "Vector value seems out of range (too positive)";
        if (value != 0.0f) {
            hasNonZeroValue = true;
        }
    }
    EXPECT_TRUE(hasNonZeroValue) << "Embedding vector should contain non-zero values";
}
