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
#include <QString>
#include <QJsonValue>

/**
 * @brief Helper class to bridge JavaScript and SQLite.
 *
 * This class handles database connections and converts SQL results into QJsonValue formats.
 */
class ScriptDatabaseBridge : public QObject {
    Q_OBJECT

public:
    explicit ScriptDatabaseBridge(const QString& dbPath, QObject* parent = nullptr);
    ~ScriptDatabaseBridge() override;

    /**
     * @brief Executes a SQL query and returns the result as a QJsonValue.
     * @param sql The SQL query to execute.
     * @return A QJsonArray for SELECT queries, a QJsonObject for others, or an error object.
     */
    Q_INVOKABLE QJsonValue exec(const QString& sql);

private:
    QString m_dbPath;
};
