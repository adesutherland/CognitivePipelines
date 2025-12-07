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
#pragma once

#include <QObject>
#include <QWidget>
#include <QFuture>
#include <QString>

#include "IToolConnector.h"
#include "CommonDataTypes.h"

/**
 * @brief RagIndexerNode orchestrates the RAG ingestion pipeline.
 * 
 * This node scans a directory for files, chunks the text content,
 * generates embeddings via the LLM API, and stores everything in a
 * local SQLite database for retrieval-augmented generation.
 * 
 * Features:
 * - Recursive directory scanning
 * - Code-aware text chunking
 * - Embedding generation via OpenAI/compatible backends
 * - Metadata tagging (e.g., "baseline", "wip")
 * - Transactional SQLite storage
 */
class RagIndexerNode : public QObject, public IToolConnector {
    Q_OBJECT
    Q_INTERFACES(IToolConnector)
public:
    explicit RagIndexerNode(QObject* parent = nullptr);
    ~RagIndexerNode() override = default;

    // IToolConnector interface (V3 tokens API)
    NodeDescriptor getDescriptor() const override;
    QWidget* createConfigurationWidget(QWidget* parent) override;
    TokenList execute(const TokenList& incomingTokens) override;
    QJsonObject saveState() const override;
    void loadState(const QJsonObject& data) override;

    // Port IDs
    static constexpr const char* kInputDirectoryPath = "directory_path";
    static constexpr const char* kInputDatabasePath = "database_path";
    static constexpr const char* kInputMetadata = "index_metadata";
    static constexpr const char* kOutputDatabasePath = "database_path";
    static constexpr const char* kOutputCount = "count";

    // Property accessors
    QString directoryPath() const { return m_directoryPath; }
    QString databasePath() const { return m_databasePath; }
    QString indexMetadata() const { return m_indexMetadata; }
    QString providerId() const { return m_providerId; }
    QString modelId() const { return m_modelId; }
    int chunkSize() const { return m_chunkSize; }
    int chunkOverlap() const { return m_chunkOverlap; }
    QString fileFilter() const { return m_fileFilter; }
    QString chunkingStrategy() const { return m_chunkingStrategy; }
    bool clearDatabase() const { return m_clearDatabase; }

public slots:
    void setDirectoryPath(const QString& path);
    void setDatabasePath(const QString& path);
    void setIndexMetadata(const QString& metadata);
    void setProviderId(const QString& id);
    void setModelId(const QString& id);
    void setChunkSize(int size);
    void setChunkOverlap(int overlap);
    void setFileFilter(const QString& filter);
    void setChunkingStrategy(const QString& strategy);
    void setClearDatabase(bool clear);

signals:
    void directoryPathChanged(const QString& path);
    void databasePathChanged(const QString& path);
    void indexMetadataChanged(const QString& metadata);
    void providerChanged(const QString& id);
    void modelChanged(const QString& id);
    void chunkSizeChanged(int size);
    void chunkOverlapChanged(int overlap);
    void fileFilterChanged(const QString& filter);
    void chunkingStrategyChanged(const QString& strategy);
    void clearDatabaseChanged(bool clear);

    // Emitted periodically while indexing is running to report progress
    // (e.g., current file/chunk and totals). The ExecutionEngine listens
    // for this signal to surface live status in the Stage Output panel.
    void progressUpdated(const DataPacket& packet);

private:
    // Legacy async helper preserved to reuse the existing QtConcurrent-based
    // implementation. The public execute(TokenList&) wrapper calls this and
    // adapts the result to the V3 token API.
    QFuture<DataPacket> Execute(const DataPacket& inputs);

    // Configuration properties
    QString m_directoryPath;
    QString m_databasePath;
    QString m_indexMetadata { QStringLiteral("{\"source\": \"user\"}") };
    QString m_providerId { QStringLiteral("openai") };
    QString m_modelId { QStringLiteral("text-embedding-3-small") };
    int m_chunkSize { 1000 };
    int m_chunkOverlap { 200 };
    QString m_fileFilter;
    QString m_chunkingStrategy { QStringLiteral("Auto") };
    bool m_clearDatabase { false };
};
