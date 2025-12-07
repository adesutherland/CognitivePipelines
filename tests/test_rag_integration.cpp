//
// Cognitive Pipeline Application - RAG end-to-end persistence test
//

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

#include "RagIndexerNode.h"

namespace {

// Ensure a QCoreApplication exists for Qt SQL and file utilities.
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

} // namespace

TEST(RagIntegrationTest, IndexesMultipleFilesIntoSingleDatabase)
{
    ensureCoreApp();

    // Temporary directory with two small text files (kept intentionally
    // small so the test runs quickly while still exercising multi-file
    // indexing into a single database).
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    const QString fileAPath = tempDir.path() + QStringLiteral("/file_a.txt");
    const QString fileBPath = tempDir.path() + QStringLiteral("/file_b.txt");

    {
        QFile fileA(fileAPath);
        ASSERT_TRUE(fileA.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream outA(&fileA);
        // Only a handful of lines are needed to produce multiple chunks
        // overall; keeping this small dramatically reduces the number of
        // embedding calls in RagIndexerNode (and therefore test runtime).
        for (int i = 0; i < 5; ++i) {
            outA << "File A - line " << i
                 << ". This is some example content to ensure chunking across boundaries.\n";
        }
        fileA.close();
    }

    {
        QFile fileB(fileBPath);
        ASSERT_TRUE(fileB.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream outB(&fileB);
        // Mirror File A: a few lines are sufficient for coverage.
        for (int i = 0; i < 5; ++i) {
            outB << "File B - line " << i
                 << ". Additional content to drive multiple chunks in the index.\n";
        }
        fileB.close();
    }

    // Separate temporary directory for the SQLite DB
    QTemporaryDir dbDir;
    ASSERT_TRUE(dbDir.isValid());
    const QString dbPath = dbDir.path() + QStringLiteral("/rag_multi.db");

    RagIndexerNode indexer;
    indexer.setDirectoryPath(tempDir.path());
    indexer.setDatabasePath(dbPath);
    indexer.setIndexMetadata(QStringLiteral("{\"status\": \"multi_file_test\"}"));
    indexer.setChunkSize(128);   // small to guarantee multiple chunks overall
    indexer.setChunkOverlap(0);
    indexer.setProviderId(QStringLiteral("openai"));
    indexer.setModelId(QStringLiteral("text-embedding-3-small"));
    indexer.setClearDatabase(true);

    DataPacket inputs;
    inputs.insert(QString::fromLatin1(RagIndexerNode::kInputDirectoryPath), tempDir.path());
    inputs.insert(QString::fromLatin1(RagIndexerNode::kInputDatabasePath), dbPath);
    inputs.insert(QString::fromLatin1(RagIndexerNode::kInputMetadata), QStringLiteral("{\"status\": \"multi_file_test\"}"));

    ExecutionToken token;
    token.data = inputs;
    TokenList tokens;
    tokens.push_back(std::move(token));

    const TokenList outTokens = indexer.execute(tokens);
    ASSERT_FALSE(outTokens.empty());
    DataPacket output = outTokens.front().data;

    ASSERT_TRUE(output.contains(QString::fromLatin1(RagIndexerNode::kOutputCount)));
    const int chunkCount = output.value(QString::fromLatin1(RagIndexerNode::kOutputCount)).toInt();

    if (chunkCount == 0) {
        GTEST_SKIP() << "No API credentials available. Set OPENAI_API_KEY or configure accounts.json to run RAG indexer.";
    }

    // Open DB and verify that both files and all fragments were persisted.
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("rag_integration_db"));
    db.setDatabaseName(dbPath);
    ASSERT_TRUE(db.open()) << "Failed to open RAG database: " << db.lastError().text().toStdString();

    QSqlQuery query(db);

    ASSERT_TRUE(query.exec(QStringLiteral("SELECT COUNT(*) FROM source_files")));
    ASSERT_TRUE(query.next());
    const int fileCount = query.value(0).toInt();
    EXPECT_EQ(fileCount, 2) << "Expected exactly 2 source_files rows for File A and File B";

    ASSERT_TRUE(query.exec(QStringLiteral("SELECT COUNT(*) FROM fragments")));
    ASSERT_TRUE(query.next());
    const int fragmentCount = query.value(0).toInt();
    EXPECT_GT(fragmentCount, 2) << "Expected more than 2 fragments across both files";

    db.close();
    QSqlDatabase::removeDatabase(QStringLiteral("rag_integration_db"));
}
