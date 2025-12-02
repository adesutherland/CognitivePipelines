//
// Cognitive Pipeline Application - Ziggy Repro Test
//
// Targeted reproduction for a data-loss bug where a C++ file that transitions
// from a Doxygen-style comment block directly into an `#include` line appears
// to chunk correctly but is persisted as empty rows in the `fragments` table.
//

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

#include "core/TextChunker.h"
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

// Helper for hex-style debug of stored chunks.
static QString toVisibleDebug(const QString& s)
{
    QString result;
    for (QChar c : s) {
        if (c == '\n') {
            result += QStringLiteral("\\n");
        } else if (c == '\r') {
            result += QStringLiteral("\\r");
        } else if (c == '\t') {
            result += QStringLiteral("\\t");
        } else {
            result += c;
        }
    }
    return result;
}

} // namespace

/**
 * @brief Repro test for ziggy.cpp-style content.
 *
 * Steps (mirrors the user-specified scenario):
 * 1. Hard-code the exact ziggy-style content.
 * 2. Chunk with TextChunker::split using FileType::CodeCpp.
 * 3. Checkpoint 1: ensure chunk index 1 (the one beginning with `#include`) is
 *    non-empty and log it to qDebug().
 * 4. Persist chunks into a temporary SQLite database using the same schema as
 *    the production RAG index.
 * 5. Checkpoint 2: query back chunk_index 1 and assert that `content` is
 *    non-empty.
 */
TEST(ZiggyReproTest, ChunkAndPersistZiggyLikeContent)
{
    ensureCoreApp();

    // --- Step 1: Hard-coded input ---
    const QString ziggyContent = R"(/**
 * @file ziggy.cpp
 * @brief The Rise and Fall of Ziggy Stardust and the Spiders from Mars.
 *
 * This file defines the core logic for the ZiggyStardust class.
 */
#include <iostream>

void playGuitar() {
    // logic
}
)";

    // --- Step 2: Chunking ---
    // Use a relatively small chunk size so that the transition from the
    // leading Doxygen block comment to the first `#include` is very likely
    // to appear in a *separate* chunk. This mirrors the original bug report
    // where the Chunker saw multiple chunks and the DB ended up with empty
    // rows for some of them.
    const int chunkSize = 80;
    const int chunkOverlap = 20;

    QStringList chunks = TextChunker::split(ziggyContent, chunkSize, chunkOverlap, FileType::CodeCpp);

    qDebug() << "ZiggyReproTest: chunk count =" << chunks.size();

    // Locate the chunk that actually contains the #include line. In the
    // original bug report this was at index 1, but depending on chunking
    // parameters and implementation details it may end up at a different
    // index. We keep the test robust by searching instead of hard-coding.
    int includeChunkIndex = -1;
    for (int i = 0; i < chunks.size(); ++i) {
        if (chunks[i].contains(QStringLiteral("#include"))) {
            includeChunkIndex = i;
            break;
        }
    }

    ASSERT_GE(includeChunkIndex, 0) << "Expected at least one chunk containing '#include'";

    // --- Checkpoint 1: Inspect the chunk containing the include ---
    const QString includeChunk = chunks[includeChunkIndex];
    qDebug() << "ZiggyReproTest: include-chunk index =" << includeChunkIndex;
    qDebug() << "ZiggyReproTest: include-chunk length =" << includeChunk.length();
    qDebug() << "ZiggyReproTest: include-chunk content (visible) =" << toVisibleDebug(includeChunk);

    ASSERT_FALSE(includeChunk.isEmpty()) << "Precondition failed: include chunk should not be empty";
    // Explicitly assert that matched '<' and '>' is present so that this test
    // exercises the edge case the user observed in their environment.
    ASSERT_TRUE(includeChunk.contains(QLatin1Char('<')))
        << "Include chunk should contain '<' to reproduce the reported issue";

    // --- Step 3: Persist to a temporary SQLite database ---
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString dbPath = dir.path() + QStringLiteral("/ziggy_repro.db");

    const QString connectionName = QStringLiteral("ziggy_repro_conn");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(dbPath);
        ASSERT_TRUE(db.open()) << "Failed to open temp db: " << db.lastError().text().toStdString();

        createBasicRagSchema(db);

        // Insert a dummy source_files row so fragments have a valid foreign key.
        QSqlQuery insertFile(db);
        ASSERT_TRUE(insertFile.exec(QStringLiteral(
                            "INSERT INTO source_files (file_path, provider, model) "
                            "VALUES ('ziggy.cpp', 'openai', 'text-embedding-3-small');")))
            << "Failed to insert source file: " << insertFile.lastError().text().toStdString();

        // Retrieve file_id
        QSqlQuery selectId(db);
        ASSERT_TRUE(selectId.exec(QStringLiteral("SELECT id FROM source_files WHERE file_path='ziggy.cpp';")))
            << "Failed to select source file id: " << selectId.lastError().text().toStdString();
        ASSERT_TRUE(selectId.next());
        const qint64 fileId = selectId.value(0).toLongLong();

        QSqlQuery insertFrag(db);
        ASSERT_TRUE(insertFrag.prepare(QStringLiteral(
                                "INSERT INTO fragments (file_id, chunk_index, content, embedding) "
                                "VALUES (:file_id, :chunk_index, :content, :embedding)")))
            << "Failed to prepare fragment insert: " << insertFrag.lastError().text().toStdString();

        for (int i = 0; i < chunks.size(); ++i) {
            const QString& chunk = chunks[i];

            qDebug() << "ZiggyReproTest: inserting chunk" << i
                     << "len=" << chunk.length()
                     << "visible=" << toVisibleDebug(chunk);

            const QByteArray emptyEmbedding; // Embeddings are irrelevant for this repro

            insertFrag.bindValue(QStringLiteral(":file_id"), fileId);
            insertFrag.bindValue(QStringLiteral(":chunk_index"), i);
            insertFrag.bindValue(QStringLiteral(":content"), chunk);
            insertFrag.bindValue(QStringLiteral(":embedding"), emptyEmbedding);

            if (!insertFrag.exec()) {
                ADD_FAILURE() << "Fragment insert failed for chunk index " << i
                              << ": " << insertFrag.lastError().text().toStdString();
            }
        }

        db.close();
    }

    QSqlDatabase::removeDatabase(connectionName);

    // --- Step 4: Verification from DB ---
    {
        const QString verifyConnName = QStringLiteral("ziggy_repro_verify");
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), verifyConnName);
            db.setDatabaseName(dbPath);
            ASSERT_TRUE(db.open()) << "Failed to reopen temp db: " << db.lastError().text().toStdString();

            QSqlQuery verify(db);
            const int expectedIndex = includeChunkIndex;
            const QString querySql = QStringLiteral(
                "SELECT chunk_index, content FROM fragments WHERE chunk_index = %1;")
                                          .arg(expectedIndex);
            ASSERT_TRUE(verify.exec(querySql))
                << "Failed to query fragments: " << verify.lastError().text().toStdString();

            bool foundRow = false;
            QString dbContent;
            while (verify.next()) {
                const int idx = verify.value(0).toInt();
                const QString content = verify.value(1).toString();

                qDebug() << "ZiggyReproTest: DB row chunk_index=" << idx
                         << "len=" << content.length()
                         << "visible=" << toVisibleDebug(content);

                if (idx == expectedIndex) {
                    foundRow = true;
                    dbContent = content;
                }
            }

            EXPECT_TRUE(foundRow) << "Expected a row with the include chunk_index in fragments table";
            EXPECT_FALSE(dbContent.isEmpty()) << "DB content for include chunk_index should not be empty";
            // Ensure that the '<' character survives the round-trip into SQLite.
            EXPECT_TRUE(dbContent.contains(QLatin1Char('<')))
                << "DB content for include chunk_index should preserve '<' character";

            db.close();
        }
        // Now that all QSqlDatabase/QSqlQuery instances using this connection
        // are out of scope, it is safe to remove the connection without
        // triggering Qt's "connection ... is still in use" warning.
        QSqlDatabase::removeDatabase(verifyConnName);
    }
}
