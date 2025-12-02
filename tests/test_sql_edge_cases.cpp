//
// Cognitive Pipeline Application - SQL Edge Case Tests
//

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryDir>
#include <QVariant>

#include "core/RagUtils.h"

namespace {

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

/**
 * @brief Ensure that inserting text chunks containing leading '#' does not
 *        corrupt QSqlQuery state or result in empty/NULL rows.
 *
 * This test uses a prepared INSERT statement with bound parameters, mirroring
 * the production insertion path used by RagIndexerNode.
 */
TEST(SqlEdgeCasesTest, HashPrefixedContentPersistsCorrectly)
{
    ensureCoreApp();

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString dbPath = dir.path() + QStringLiteral("/rag_sql_edge.db");

    const QString connectionName = QStringLiteral("rag_sql_edge_test");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(dbPath);
        ASSERT_TRUE(db.open()) << "Failed to open temp db: " << db.lastError().text().toStdString();

        createBasicRagSchema(db);

        // Insert a single source_files row so fragments have a valid foreign key
        QSqlQuery insertFile(db);
        ASSERT_TRUE(insertFile.exec(QStringLiteral(
                        "INSERT INTO source_files (file_path, provider, model) "
                        "VALUES ('edge.cpp', 'openai', 'text-embedding-3-small');")))
            << "Failed to insert source file: " << insertFile.lastError().text().toStdString();

        // Look up the file_id
        QSqlQuery selectId(db);
        ASSERT_TRUE(selectId.exec(QStringLiteral("SELECT id FROM source_files WHERE file_path='edge.cpp';")))
            << "Failed to select source file id: " << selectId.lastError().text().toStdString();
        ASSERT_TRUE(selectId.next());
        const qint64 fileId = selectId.value(0).toLongLong();

        const QStringList chunks{
            QStringLiteral("Normal"),
            QStringLiteral("#include <vector>"),
            QStringLiteral("After hash")
        };

        QSqlQuery insertFrag(db);
        ASSERT_TRUE(insertFrag.prepare(QStringLiteral(
                        "INSERT INTO fragments (file_id, chunk_index, content, embedding) "
                        "VALUES (:file_id, :chunk_index, :content, :embedding)")))
            << "Failed to prepare fragment insert: " << insertFrag.lastError().text().toStdString();

        for (int i = 0; i < chunks.size(); ++i) {
            const QString& chunk = chunks[i];

            // This test focuses on SQL/text handling, so we omit a real embedding
            const QByteArray emptyEmbedding;

            insertFrag.bindValue(QStringLiteral(":file_id"), fileId);
            insertFrag.bindValue(QStringLiteral(":chunk_index"), i);
            insertFrag.bindValue(QStringLiteral(":content"), chunk);
            insertFrag.bindValue(QStringLiteral(":embedding"), emptyEmbedding);

            if (!insertFrag.exec()) {
                // Log the error so real-world failures are visible in test output
                ADD_FAILURE() << "Fragment insert failed for chunk index " << i
                              << ": " << insertFrag.lastError().text().toStdString();
            }
        }

        // Verify that all three rows exist and content is non-empty
        QSqlQuery verify(db);
        ASSERT_TRUE(verify.exec(QStringLiteral(
                        "SELECT chunk_index, content FROM fragments ORDER BY chunk_index ASC;")))
            << "Failed to query fragments: " << verify.lastError().text().toStdString();

        QStringList stored;
        while (verify.next()) {
            stored << verify.value(1).toString();
        }

        ASSERT_EQ(stored.size(), chunks.size());
        for (int i = 0; i < chunks.size(); ++i) {
            EXPECT_FALSE(stored[i].isEmpty()) << "Row " << i << " has empty content";
            EXPECT_EQ(stored[i], chunks[i]) << "Row " << i << " content mismatch";
        }

        db.close();
    }

    QSqlDatabase::removeDatabase(connectionName);
}
