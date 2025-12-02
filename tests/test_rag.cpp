//
// Cognitive Pipeline Application - RagUtils Unit Tests
//

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryDir>

#include "core/RagUtils.h"

namespace {

// Ensure a QCoreApplication instance exists for Qt SQL.
//
// We intentionally allocate the application on the heap and never delete it,
// matching the pattern used in other tests (e.g., test_rag_foundation.cpp).
// This avoids potential shutdown-order issues between Qt's global singletons
// and static object destructors when the test process exits.
QCoreApplication* ensureCoreApp()
{
    static QCoreApplication* app = nullptr;
    if (app) {
        return app;
    }

    static int argc = 1;
    static char appName[] = "unit_tests";
    static char* argv[] = { appName, nullptr };
    app = new QCoreApplication(argc, argv);
    return app;
}

void createBasicRagSchema(QSqlDatabase& db)
{
    QSqlQuery query(db);
    ASSERT_TRUE(query.exec(QString::fromUtf8(kRagSchemaPragma)))
        << "Failed to enable foreign keys: " << query.lastError().text().toStdString();

    ASSERT_TRUE(query.exec(QString::fromUtf8(kRagSchemaSourceFiles)))
        << "Failed to create source_files table: " << query.lastError().text().toStdString();

    ASSERT_TRUE(query.exec(QString::fromUtf8(kRagSchemaFragments)))
        << "Failed to create fragments table: " << query.lastError().text().toStdString();
}

} // namespace

TEST(RagUtilsTest, GetIndexConfigSingleModel)
{
    ensureCoreApp();

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString dbPath = dir.path() + QStringLiteral("/rag_single.db");

    const QString connectionName = QStringLiteral("rag_utils_test_single");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(dbPath);
        ASSERT_TRUE(db.open()) << "Failed to open temp db: " << db.lastError().text().toStdString();

        createBasicRagSchema(db);

        QSqlQuery insert(db);
        ASSERT_TRUE(insert.exec(QStringLiteral(
                        "INSERT INTO source_files (file_path, provider, model) "
                        "VALUES ('a.txt', 'openai', 'text-embedding-3-small');")))
            << "Failed to insert row: " << insert.lastError().text().toStdString();
        ASSERT_TRUE(insert.exec(QStringLiteral(
                        "INSERT INTO source_files (file_path, provider, model) "
                        "VALUES ('b.txt', 'openai', 'text-embedding-3-small');")))
            << "Failed to insert row: " << insert.lastError().text().toStdString();

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    RagUtils::IndexConfig cfg = RagUtils::getIndexConfig(dbPath);
    EXPECT_EQ(cfg.providerId, QStringLiteral("openai"));
    EXPECT_EQ(cfg.modelId, QStringLiteral("text-embedding-3-small"));
}

TEST(RagUtilsTest, GetIndexConfigMixedModelsThrows)
{
    ensureCoreApp();

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString dbPath = dir.path() + QStringLiteral("/rag_mixed.db");

    const QString connectionName = QStringLiteral("rag_utils_test_mixed");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(dbPath);
        ASSERT_TRUE(db.open()) << "Failed to open temp db: " << db.lastError().text().toStdString();

        createBasicRagSchema(db);

        QSqlQuery insert(db);
        ASSERT_TRUE(insert.exec(QStringLiteral(
                        "INSERT INTO source_files (file_path, provider, model) "
                        "VALUES ('a.txt', 'openai', 'text-embedding-3-small');")))
            << "Failed to insert row: " << insert.lastError().text().toStdString();
        ASSERT_TRUE(insert.exec(QStringLiteral(
                        "INSERT INTO source_files (file_path, provider, model) "
                        "VALUES ('b.txt', 'google', 'text-embedding-004');")))
            << "Failed to insert row: " << insert.lastError().text().toStdString();

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    EXPECT_THROW({ RagUtils::getIndexConfig(dbPath); }, std::runtime_error);
}

TEST(RagUtilsTest, CosineSimilarityBasicCases)
{
    const std::vector<float> v1{1.0f, 0.0f};
    const std::vector<float> v2{0.0f, 1.0f};
    const std::vector<float> v3{1.0f, 0.0f};
    const std::vector<float> empty;

    const double identical = RagUtils::cosineSimilarity(v1, v3);
    const double orthogonal = RagUtils::cosineSimilarity(v1, v2);
    const double withEmpty = RagUtils::cosineSimilarity(v1, empty);

    EXPECT_NEAR(identical, 1.0, 1e-6);
    EXPECT_NEAR(orthogonal, 0.0, 1e-6);
    EXPECT_NEAR(withEmpty, 0.0, 1e-6);
}

TEST(RagUtilsTest, FindMostRelevantChunksSimple)
{
    ensureCoreApp();

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString dbPath = dir.path() + QStringLiteral("/rag_search.db");

    const QString connectionName = QStringLiteral("rag_utils_test_search");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(dbPath);
        ASSERT_TRUE(db.open()) << "Failed to open temp db: " << db.lastError().text().toStdString();

        createBasicRagSchema(db);

        QSqlQuery insertFile(db);
        ASSERT_TRUE(insertFile.exec(QStringLiteral(
                        "INSERT INTO source_files (file_path, provider, model) "
                        "VALUES ('doc.txt', 'openai', 'text-embedding-3-small');")))
            << "Failed to insert source file: " << insertFile.lastError().text().toStdString();

        QSqlQuery selectId(db);
        ASSERT_TRUE(selectId.exec(QStringLiteral("SELECT id FROM source_files WHERE file_path='doc.txt';")))
            << "Failed to select source file id: " << selectId.lastError().text().toStdString();
        ASSERT_TRUE(selectId.next());
        const qint64 fileId = selectId.value(0).toLongLong();

        // Two 2D embeddings: [1,0] and [0,1]
        const std::vector<float> embA{1.0f, 0.0f};
        const std::vector<float> embB{0.0f, 1.0f};
        const QByteArray blobA(reinterpret_cast<const char*>(embA.data()), static_cast<int>(embA.size() * sizeof(float)));
        const QByteArray blobB(reinterpret_cast<const char*>(embB.data()), static_cast<int>(embB.size() * sizeof(float)));

        QSqlQuery insertFrag(db);
        insertFrag.prepare(QStringLiteral(
            "INSERT INTO fragments (file_id, chunk_index, content, embedding) "
            "VALUES (:file_id, :chunk_index, :content, :embedding)"));

        insertFrag.bindValue(QStringLiteral(":file_id"), fileId);
        insertFrag.bindValue(QStringLiteral(":chunk_index"), 0);
        insertFrag.bindValue(QStringLiteral(":content"), QStringLiteral("chunk A"));
        insertFrag.bindValue(QStringLiteral(":embedding"), blobA);
        ASSERT_TRUE(insertFrag.exec())
            << "Failed to insert fragment A: " << insertFrag.lastError().text().toStdString();

        insertFrag.bindValue(QStringLiteral(":file_id"), fileId);
        insertFrag.bindValue(QStringLiteral(":chunk_index"), 1);
        insertFrag.bindValue(QStringLiteral(":content"), QStringLiteral("chunk B"));
        insertFrag.bindValue(QStringLiteral(":embedding"), blobB);
        ASSERT_TRUE(insertFrag.exec())
            << "Failed to insert fragment B: " << insertFrag.lastError().text().toStdString();

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    const std::vector<float> query{1.0f, 0.0f};
    const auto results = RagUtils::findMostRelevantChunks(dbPath, query, /*limit*/ 5, /*minRelevance*/ 0.0);

    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results.front().chunkIndex, 0);
    EXPECT_GE(results.front().score, results.back().score);
}
