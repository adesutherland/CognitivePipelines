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
#include "DatabaseConnector.h"
#include "CommonDataTypes.h"
#include "DatabaseConnectorPropertiesWidget.h"

#include <QtConcurrent/QtConcurrent>
#include "Logger.h"
#include <QUuid>

// QtSql
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>

DatabaseConnector::DatabaseConnector(QObject* parent)
    : QObject(parent)
{
}

NodeDescriptor DatabaseConnector::getDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("database-connector");
    desc.name = QStringLiteral("Database Connector");
    desc.category = QStringLiteral("Data");

    // Input pin: database (text)
    PinDefinition inDatabase;
    inDatabase.direction = PinDirection::Input;
    inDatabase.id = QStringLiteral("database");
    inDatabase.name = QStringLiteral("Database");
    inDatabase.type = QStringLiteral("text");
    desc.inputPins.insert(inDatabase.id, inDatabase);

    // Input pin: sql (text)
    PinDefinition inSql;
    inSql.direction = PinDirection::Input;
    inSql.id = QStringLiteral("sql");
    inSql.name = QStringLiteral("SQL");
    inSql.type = QStringLiteral("text");
    desc.inputPins.insert(inSql.id, inSql);

    // Output pin: database (text)
    PinDefinition outDatabase;
    outDatabase.direction = PinDirection::Output;
    outDatabase.id = QStringLiteral("database");
    outDatabase.name = QStringLiteral("Database");
    outDatabase.type = QStringLiteral("text");
    desc.outputPins.insert(outDatabase.id, outDatabase);

    // Output pin: stdout (text)
    PinDefinition outStdout;
    outStdout.direction = PinDirection::Output;
    outStdout.id = QStringLiteral("stdout");
    outStdout.name = QStringLiteral("stdout");
    outStdout.type = QStringLiteral("text");
    desc.outputPins.insert(outStdout.id, outStdout);

    // Output pin: stderr (text)
    PinDefinition outStderr;
    outStderr.direction = PinDirection::Output;
    outStderr.id = QStringLiteral("stderr");
    outStderr.name = QStringLiteral("stderr");
    outStderr.type = QStringLiteral("text");
    desc.outputPins.insert(outStderr.id, outStderr);

    return desc;
}

QWidget* DatabaseConnector::createConfigurationWidget(QWidget* parent)
{
    if (!propertiesWidget) {
        propertiesWidget = new DatabaseConnectorPropertiesWidget(parent);
        // initialize UI from current state
        propertiesWidget->setDatabasePath(m_databasePath);
        propertiesWidget->setSqlQuery(m_sqlQuery);
        // connect UI -> node
        QObject::connect(propertiesWidget, &DatabaseConnectorPropertiesWidget::databasePathChanged,
                         this, &DatabaseConnector::onDatabasePathChanged);
        QObject::connect(propertiesWidget, &DatabaseConnectorPropertiesWidget::sqlQueryChanged,
                         this, &DatabaseConnector::onSqlQueryChanged);
    }
    return propertiesWidget;
}

TokenList DatabaseConnector::execute(const TokenList& incomingTokens)
{
    // Merge incoming tokens into a single DataPacket, preserving the
    // last-writer-wins semantics used elsewhere in the engine.
    DataPacket inputs;
    for (const auto& token : incomingTokens) {
        for (auto it = token.data.cbegin(); it != token.data.cend(); ++it) {
            inputs.insert(it.key(), it.value());
        }
    }

    // Check if SQL is provided via input pin, otherwise use internal property
    QString sql = inputs.value(QStringLiteral("sql")).toString();
    if (sql.trimmed().isEmpty()) {
        sql = m_sqlQuery;
    }

    // Check if database path is provided via input pin, otherwise use internal property
    QString dbPath = inputs.value(QStringLiteral("database")).toString();
    if (dbPath.trimmed().isEmpty()) {
        dbPath = m_databasePath;
    }

    auto work = [sql, dbPath]() -> DataPacket {
        DataPacket packet;
        const QString outKey = QStringLiteral("stdout");
        const QString errKey = QStringLiteral("stderr");

        if (dbPath.trimmed().isEmpty()) {
            const QString msg = QStringLiteral("ERROR: Database path is empty.");
            packet.insert(outKey, QString());
            packet.insert(errKey, msg);
            packet.insert(QStringLiteral("database"), dbPath);
            CP_WARN << "DatabaseConnector:" << msg;
            return packet;
        }
        if (sql.trimmed().isEmpty()) {
            const QString msg = QStringLiteral("ERROR: SQL is empty.");
            packet.insert(outKey, QString());
            packet.insert(errKey, msg);
            packet.insert(QStringLiteral("database"), dbPath);
            CP_WARN << "DatabaseConnector:" << msg;
            return packet;
        }

        const QString connectionName = QUuid::createUuid().toString(QUuid::WithoutBraces);

        QString stdoutText;
        QString stderrText;
        {
            // Scope to ensure db and query are destroyed before removeDatabase
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
            db.setDatabaseName(dbPath);

            if (!db.open()) {
                stderrText = db.lastError().text();
                CP_WARN << "DatabaseConnector: failed to open DB" << dbPath << ":" << stderrText;
                // Ensure close before leaving scope
                if (db.isOpen()) db.close();
            } else {
                // Split SQL by semicolon to handle multi-statement scripts
                QStringList statements = sql.split(QLatin1Char(';'), Qt::SkipEmptyParts);
                
                // Start transaction for multi-statement execution
                if (!db.transaction()) {
                    stderrText = QStringLiteral("Failed to start transaction: ") + db.lastError().text();
                    CP_WARN << "DatabaseConnector:" << stderrText;
                    db.close();
                } else {
                    QSqlQuery query(db);
                    bool allSuccess = true;
                    qint64 totalRowsAffected = 0;
                    
                    // Execute each statement
                    for (const QString& stmt : statements) {
                        QString trimmedStmt = stmt.trimmed();
                        if (trimmedStmt.isEmpty()) {
                            continue;
                        }
                        
                        if (!query.exec(trimmedStmt)) {
                            stderrText = QStringLiteral("Statement failed: ") + query.lastError().text();
                            CP_WARN << "DatabaseConnector: statement exec failed:" << stderrText;
                            allSuccess = false;
                            break;
                        }
                        
                        if (!query.isSelect()) {
                            totalRowsAffected += query.numRowsAffected();
                        }
                    }
                    
                    if (!allSuccess) {
                        // Rollback on failure
                        if (!db.rollback()) {
                            stderrText += QStringLiteral(" (Rollback also failed: ") + db.lastError().text() + QStringLiteral(")");
                            CP_WARN << "DatabaseConnector: rollback failed:" << db.lastError().text();
                        }
                        db.close();
                    } else {
                        // Commit on success
                        if (!db.commit()) {
                            stderrText = QStringLiteral("Failed to commit transaction: ") + db.lastError().text();
                            CP_WARN << "DatabaseConnector: commit failed:" << stderrText;
                            db.close();
                        } else {
                            // Success: format results. If last query was a SELECT, show table
                            if (query.isSelect()) {
                                // Build Markdown table with headers
                                auto sanitizeCell = [](const QString& in) -> QString {
                                    // First escape HTML-sensitive characters so Markdown renderers
                                    // display them literally (e.g. "<vector>" -> "&lt;vector&gt;").
                                    QString s = in.toHtmlEscaped();
                                    // Replace newlines and carriage returns with spaces to prevent breaking table structure
                                    s.replace(QLatin1Char('\n'), QStringLiteral(" "));
                                    s.replace(QLatin1Char('\r'), QStringLiteral(" "));
                                    // Collapse consecutive whitespace into single space
                                    s = s.simplified();
                                    // Escape pipe characters to avoid confusing Markdown parser
                                    s.replace(QLatin1Char('|'), QStringLiteral("\\|"));
                                    return s;
                                };

                                QSqlRecord rec = query.record();
                                const int colCount = rec.count();
                                QStringList lines;

                                // Header row
                                {
                                    QStringList headers;
                                    headers.reserve(colCount);
                                    for (int i = 0; i < colCount; ++i) {
                                        headers << sanitizeCell(rec.fieldName(i));
                                    }
                                    lines << QStringLiteral("| ") + headers.join(QStringLiteral(" | ")) + QStringLiteral(" |");
                                }

                                // Separator row
                                {
                                    QStringList separators;
                                    separators.reserve(colCount);
                                    for (int i = 0; i < colCount; ++i) {
                                        separators << QStringLiteral("---");
                                    }
                                    lines << QStringLiteral("|") + separators.join(QStringLiteral("|")) + QStringLiteral("|");
                                }

                                // Data rows
                                while (query.next()) {
                                    QStringList row;
                                    row.reserve(colCount);
                                    for (int i = 0; i < colCount; ++i) {
                                        const QVariant v = query.value(i);
                                        if (v.isNull()) {
                                            row << QStringLiteral("NULL");
                                        } else {
                                            row << sanitizeCell(v.toString());
                                        }
                                    }
                                    lines << QStringLiteral("| ") + row.join(QStringLiteral(" | ")) + QStringLiteral(" |");
                                }

                                stdoutText = lines.join(QLatin1Char('\n'));
                            } else {
                                // Non-SELECT queries: show total rows affected
                                stdoutText = QStringLiteral("Rows affected: ") + QString::number(totalRowsAffected);
                            }
                            db.close();
                        }
                    }
                }
            }
        }
        // After db and query are out of scope, remove the connection
        QSqlDatabase::removeDatabase(connectionName);

        packet.insert(QStringLiteral("stdout"), stdoutText);
        packet.insert(QStringLiteral("stderr"), stderrText);
        packet.insert(QStringLiteral("database"), dbPath);
        return packet;
    };

    const DataPacket packet = work();

    ExecutionToken token;
    token.data = packet;

    TokenList result;
    result.push_back(token);
    return result;
}

QJsonObject DatabaseConnector::saveState() const
{
    QJsonObject state;
    state.insert(QStringLiteral("databasePath"), m_databasePath);
    state.insert(QStringLiteral("sqlQuery"), m_sqlQuery);
    return state;
}

void DatabaseConnector::loadState(const QJsonObject& data)
{
    if (data.contains(QStringLiteral("databasePath"))) {
        m_databasePath = data.value(QStringLiteral("databasePath")).toString();
        if (propertiesWidget) {
            propertiesWidget->setDatabasePath(m_databasePath);
        }
    }
    if (data.contains(QStringLiteral("sqlQuery"))) {
        m_sqlQuery = data.value(QStringLiteral("sqlQuery")).toString();
        if (propertiesWidget) {
            propertiesWidget->setSqlQuery(m_sqlQuery);
        }
    }
}

void DatabaseConnector::onDatabasePathChanged(const QString& path)
{
    if (m_databasePath == path) return;
    m_databasePath = path;
}

void DatabaseConnector::onSqlQueryChanged(const QString& query)
{
    if (m_sqlQuery == query) return;
    m_sqlQuery = query;
}
