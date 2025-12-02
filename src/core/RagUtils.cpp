//
// Cognitive Pipeline Application
//
// RAG utility functions for index inspection and similarity search
//

#include "RagUtils.h"

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QtCore/QUuid>
#include <QtCore/QVariant>

#include <algorithm>
#include <cmath>
#include <stdexcept>

using namespace std;

namespace {

vector<float> blobToVectorFloat(const QByteArray& blob)
{
    vector<float> result;
    if (blob.isEmpty()) {
        return result;
    }

    // Ensure size is a multiple of sizeof(float)
    if (blob.size() % static_cast<int>(sizeof(float)) != 0) {
        return result; // treat malformed blobs as empty to keep search robust
    }

    const int count = blob.size() / static_cast<int>(sizeof(float));
    result.resize(count);
    memcpy(result.data(), blob.constData(), blob.size());
    return result;
}

} // namespace

RagUtils::IndexConfig RagUtils::getIndexConfig(const QString& dbPath)
{
    const QString connectionName = QStringLiteral("rag_utils_index_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    QVector<QPair<QString, QString>> pairs;
    QString errorMessage;
    bool hadError = false;

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(dbPath);
        if (!db.open()) {
            errorMessage = QStringLiteral("Failed to open RAG database '%1': %2")
                               .arg(dbPath, db.lastError().text());
            hadError = true;
        } else {
            QSqlQuery query(db);
            if (!query.exec(QStringLiteral("SELECT DISTINCT provider, model FROM source_files"))) {
                errorMessage = QStringLiteral("Failed to query source_files for index configuration: %1")
                                   .arg(query.lastError().text());
                hadError = true;
            } else {
                while (query.next()) {
                    const QString provider = query.value(0).toString();
                    const QString model = query.value(1).toString();
                    pairs.append(qMakePair(provider, model));
                }
            }

            db.close();
        }
        // db and query go out of scope here, before removeDatabase is called.
    }

    // Now that all QSqlDatabase and QSqlQuery instances using this connection
    // have gone out of scope, it is safe to remove the connection.
    QSqlDatabase::removeDatabase(connectionName);

    if (hadError) {
        throw std::runtime_error(errorMessage.toStdString());
    }

    if (pairs.isEmpty()) {
        throw std::runtime_error("RAG index is empty; no source_files rows found");
    }

    if (pairs.size() > 1) {
        throw std::runtime_error("Mixed-model RAG is not supported: multiple provider/model pairs found in source_files");
    }

    IndexConfig cfg;
    cfg.providerId = pairs.first().first;
    cfg.modelId = pairs.first().second;
    return cfg;
}

double RagUtils::cosineSimilarity(const vector<float>& a, const vector<float>& b)
{
    if (a.empty() || b.empty() || a.size() != b.size()) {
        return 0.0;
    }

    double dot = 0.0;
    double normA = 0.0;
    double normB = 0.0;

    for (size_t i = 0; i < a.size(); ++i) {
        const double va = a[i];
        const double vb = b[i];
        dot += va * vb;
        normA += va * va;
        normB += vb * vb;
    }

    if (normA == 0.0 || normB == 0.0) {
        return 0.0;
    }

    const double denom = std::sqrt(normA) * std::sqrt(normB);
    if (denom == 0.0) {
        return 0.0;
    }

    return dot / denom;
}

std::vector<RagUtils::SearchResult> RagUtils::findMostRelevantChunks(
    const QString& dbPath,
    const std::vector<float>& queryEmbedding,
    int limit,
    double minRelevance)
{
    std::vector<SearchResult> results;

    if (limit <= 0) {
        return results;
    }

    if (queryEmbedding.empty()) {
        return results;
    }

    const QString connectionName = QStringLiteral("rag_utils_search_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    QString errorMessage;
    bool hadError = false;

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(dbPath);
        if (!db.open()) {
            errorMessage = QStringLiteral("Failed to open RAG database '%1': %2")
                               .arg(dbPath, db.lastError().text());
            hadError = true;
        } else {
            QSqlQuery query(db);
            if (!query.exec(QStringLiteral("SELECT id, file_id, chunk_index, content, embedding FROM fragments"))) {
                errorMessage = QStringLiteral("Failed to query fragments for similarity search: %1")
                                   .arg(query.lastError().text());
                hadError = true;
            } else {
                while (query.next()) {
                    const qint64 fragmentId = query.value(0).toLongLong();
                    const qint64 fileId = query.value(1).toLongLong();
                    const int chunkIndex = query.value(2).toInt();
                    const QString content = query.value(3).toString();
                    const QByteArray embeddingBlob = query.value(4).toByteArray();

                    const vector<float> embeddingVec = blobToVectorFloat(embeddingBlob);
                    if (embeddingVec.empty() || embeddingVec.size() != queryEmbedding.size()) {
                        continue; // skip malformed or incompatible embeddings
                    }

                    const double score = cosineSimilarity(queryEmbedding, embeddingVec);
                    if (score < minRelevance) {
                        continue;
                    }

                    SearchResult sr;
                    sr.fragmentId = fragmentId;
                    sr.fileId = fileId;
                    sr.chunkIndex = chunkIndex;
                    sr.content = content;
                    sr.score = score;
                    results.push_back(std::move(sr));
                }
            }

            db.close();
        }
        // db and query go out of scope here, before removeDatabase is called.
    }

    // Now that all QSqlDatabase and QSqlQuery instances using this connection
    // have gone out of scope, it is safe to remove the connection.
    QSqlDatabase::removeDatabase(connectionName);

    if (hadError) {
        throw std::runtime_error(errorMessage.toStdString());
    }

    std::sort(results.begin(), results.end(), [](const SearchResult& lhs, const SearchResult& rhs) {
        if (lhs.score == rhs.score) {
            return lhs.fragmentId < rhs.fragmentId; // stable deterministic ordering
        }
        return lhs.score > rhs.score;
    });

    if (static_cast<int>(results.size()) > limit) {
        results.resize(limit);
    }

    return results;
}
