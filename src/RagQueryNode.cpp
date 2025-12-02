//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "RagQueryNode.h"

#include "RagQueryPropertiesWidget.h"
#include "core/RagUtils.h"
#include "core/LLMProviderRegistry.h"
#include "backends/ILLMBackend.h"

#include <QtConcurrent>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFileInfo>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>

RagQueryNode::RagQueryNode(QObject* parent)
    : QObject(parent)
{
}

NodeDescriptor RagQueryNode::GetDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("rag-query");
    desc.name = QStringLiteral("RAG Query");
    desc.category = QStringLiteral("Retrieval");

    // Inputs
    desc.inputPins.insert(QString::fromLatin1(kInputQuery),
        PinDefinition{PinDirection::Input, QString::fromLatin1(kInputQuery),
                      QStringLiteral("Query"), QStringLiteral("text")});
    desc.inputPins.insert(QString::fromLatin1(kInputDbPath),
        PinDefinition{PinDirection::Input, QString::fromLatin1(kInputDbPath),
                      QStringLiteral("DB Path"), QStringLiteral("text")});

    // Outputs
    desc.outputPins.insert(QString::fromLatin1(kOutputContext),
        PinDefinition{PinDirection::Output, QString::fromLatin1(kOutputContext),
                      QStringLiteral("Context"), QStringLiteral("text")});
    desc.outputPins.insert(QString::fromLatin1(kOutputResults),
        PinDefinition{PinDirection::Output, QString::fromLatin1(kOutputResults),
                      QStringLiteral("Results"), QStringLiteral("json")});

    return desc;
}

QWidget* RagQueryNode::createConfigurationWidget(QWidget* parent)
{
    auto* widget = new RagQueryPropertiesWidget(parent);
    widget->setMaxResults(m_maxResults);
    widget->setMinRelevance(m_minRelevance);

    QObject::connect(widget, &RagQueryPropertiesWidget::maxResultsChanged,
                     this, &RagQueryNode::setMaxResults);
    QObject::connect(widget, &RagQueryPropertiesWidget::minRelevanceChanged,
                     this, &RagQueryNode::setMinRelevance);
    return widget;
}

QFuture<DataPacket> RagQueryNode::Execute(const DataPacket& inputs)
{
    return QtConcurrent::run([this, inputs]() -> DataPacket {
        DataPacket output;

        const QString queryText = inputs.value(QString::fromLatin1(kInputQuery)).toString().trimmed();
        const QString dbPath = inputs.value(QString::fromLatin1(kInputDbPath)).toString().trimmed();

        if (queryText.isEmpty()) {
            qWarning() << "RagQueryNode: Query text is empty";
            return output;
        }

        if (dbPath.isEmpty()) {
            qWarning() << "RagQueryNode: Database path is empty";
            return output;
        }

        QFileInfo fi(dbPath);
        if (!fi.exists() || !fi.isFile()) {
            qWarning() << "RagQueryNode: Database file does not exist:" << dbPath;
            return output;
        }

        RagUtils::IndexConfig indexCfg;
        try {
            indexCfg = RagUtils::getIndexConfig(dbPath);
        } catch (const std::exception& ex) {
            qWarning() << "RagQueryNode: Failed to inspect index config:" << ex.what();
            return output;
        }

        if (indexCfg.providerId.isEmpty() || indexCfg.modelId.isEmpty()) {
            qWarning() << "RagQueryNode: Index configuration returned empty provider/model";
            return output;
        }

        // Resolve credentials and backend via LLMProviderRegistry
        QString apiKey = LLMProviderRegistry::instance().getCredential(indexCfg.providerId);
        if (apiKey.isEmpty()) {
            qWarning() << "RagQueryNode: No API key found for provider:" << indexCfg.providerId;
            return output;
        }

        ILLMBackend* backend = LLMProviderRegistry::instance().getBackend(indexCfg.providerId);
        if (!backend) {
            qWarning() << "RagQueryNode: Backend not found for provider:" << indexCfg.providerId;
            return output;
        }

        qDebug() << "RagQueryNode: Using provider" << indexCfg.providerId
                 << "model" << indexCfg.modelId
                 << "limit" << m_maxResults
                 << "minRelevance" << m_minRelevance;

        // Vectorization
        EmbeddingResult embResult = backend->getEmbedding(apiKey, indexCfg.modelId, queryText);
        if (embResult.hasError) {
            qWarning() << "RagQueryNode: Embedding error:" << embResult.errorMsg;
            return output;
        }

        if (embResult.vector.empty()) {
            qWarning() << "RagQueryNode: Empty embedding vector for query";
            return output;
        }

        // Search
        std::vector<RagUtils::SearchResult> searchResults;
        try {
            searchResults = RagUtils::findMostRelevantChunks(dbPath, embResult.vector, m_maxResults, m_minRelevance);
        } catch (const std::exception& ex) {
            qWarning() << "RagQueryNode: Search error:" << ex.what();
            return output;
        }

        // Optionally resolve file paths for nicer "Source" labels.
        QMap<qint64, QString> filePathById;
        QString connectionName;
        bool dbOpened = false;

        if (!searchResults.empty()) {
            connectionName = QStringLiteral("rag_query_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
            db.setDatabaseName(dbPath);
            if (!db.open()) {
                qWarning() << "RagQueryNode: Failed to open database for source resolution:" << db.lastError().text();
            } else {
                dbOpened = true;
                QSqlQuery query(db);
                for (const auto& r : searchResults) {
                    if (filePathById.contains(r.fileId)) {
                        continue;
                    }
                    query.prepare(QStringLiteral("SELECT file_path FROM source_files WHERE id = ?"));
                    query.addBindValue(r.fileId);
                    if (query.exec() && query.next()) {
                        filePathById.insert(r.fileId, query.value(0).toString());
                    } else if (query.lastError().isValid()) {
                        qWarning() << "RagQueryNode: Failed to resolve file_path for id" << r.fileId
                                   << ":" << query.lastError().text();
                    }
                }

                db.close();
            }

            if (dbOpened) {
                QSqlDatabase::removeDatabase(connectionName);
            }
        }

        // Format context string
        QString contextText;
        contextText.reserve(1024);

        for (const auto& r : searchResults) {
            QString sourceLabel;
            if (filePathById.contains(r.fileId)) {
                sourceLabel = filePathById.value(r.fileId);
            } else {
                sourceLabel = QStringLiteral("file_id=%1").arg(r.fileId);
            }

            contextText += QStringLiteral("[Source: %1 (Score: %2)]\n")
                               .arg(sourceLabel)
                               .arg(r.score, 0, 'f', 4);
            contextText += r.content;
            contextText += QStringLiteral("\n\n");
        }

        // Serialize results to JSON
        QJsonArray resultsArray;
        for (const auto& r : searchResults) {
            QString sourceLabel;
            if (filePathById.contains(r.fileId)) {
                sourceLabel = filePathById.value(r.fileId);
            } else {
                sourceLabel = QStringLiteral("file_id=%1").arg(r.fileId);
            }

            QJsonObject obj;
            obj.insert(QStringLiteral("source"), sourceLabel);
            obj.insert(QStringLiteral("score"), r.score);
            obj.insert(QStringLiteral("text"), r.content);
            resultsArray.append(obj);
        }

        QJsonDocument doc(resultsArray);

        output.insert(QString::fromLatin1(kOutputContext), contextText);
        output.insert(QString::fromLatin1(kOutputResults), QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));

        return output;
    });
}

QJsonObject RagQueryNode::saveState() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("max_results"), m_maxResults);
    obj.insert(QStringLiteral("min_relevance"), m_minRelevance);
    return obj;
}

void RagQueryNode::loadState(const QJsonObject& data)
{
    if (data.contains(QStringLiteral("max_results"))) {
        m_maxResults = data.value(QStringLiteral("max_results")).toInt(m_maxResults);
    }
    if (data.contains(QStringLiteral("min_relevance"))) {
        m_minRelevance = data.value(QStringLiteral("min_relevance")).toDouble(m_minRelevance);
    }
}

void RagQueryNode::setMaxResults(int value)
{
    if (value < 1) {
        value = 1;
    } else if (value > 50) {
        value = 50;
    }
    m_maxResults = value;
}

void RagQueryNode::setMinRelevance(double value)
{
    if (value < 0.0) {
        value = 0.0;
    } else if (value > 1.0) {
        value = 1.0;
    }
    m_minRelevance = value;
}
