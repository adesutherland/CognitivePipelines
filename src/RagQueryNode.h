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
 * @brief Node that performs semantic retrieval from a RAG index.
 *
 * This node accepts a natural-language query and a database path, discovers
 * the embedding model used for the index, generates a query embedding via the
 * appropriate backend, and returns the most relevant chunks.
 */
class RagQueryNode : public QObject, public IToolConnector {
    Q_OBJECT
    Q_INTERFACES(IToolConnector)

public:
    explicit RagQueryNode(QObject* parent = nullptr);
    ~RagQueryNode() override = default;

    // IToolConnector interface
    NodeDescriptor GetDescriptor() const override;
    QWidget* createConfigurationWidget(QWidget* parent) override;
    QFuture<DataPacket> Execute(const DataPacket& inputs) override;
    QJsonObject saveState() const override;
    void loadState(const QJsonObject& data) override;

    // Port IDs
    static constexpr const char* kInputQuery = "query";
    static constexpr const char* kInputDbPath = "database";
    static constexpr const char* kOutputContext = "context";
    static constexpr const char* kOutputResults = "results";

    // Property accessors (used by tests and potential UI bindings)
    QString databasePath() const { return m_databasePath; }
    QString queryText() const { return m_queryText; }

public slots:
    void setMaxResults(int value);
    void setMinRelevance(double value);
    void setDatabasePath(const QString& path);
    void setQueryText(const QString& text);

private:
    int m_maxResults {5};
    double m_minRelevance {0.5};
    QString m_databasePath;
    QString m_queryText;
};
