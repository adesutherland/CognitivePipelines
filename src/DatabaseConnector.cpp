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
#include <QDebug>
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

NodeDescriptor DatabaseConnector::GetDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("database-connector");
    desc.name = QStringLiteral("Database Connector");
    desc.category = QStringLiteral("Data");

    // Input pin: sql (text)
    PinDefinition inSql;
    inSql.direction = PinDirection::Input;
    inSql.id = QStringLiteral("sql");
    inSql.name = QStringLiteral("SQL");
    inSql.type = QStringLiteral("text");
    desc.inputPins.insert(inSql.id, inSql);

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
        // connect UI -> node
        QObject::connect(propertiesWidget, &DatabaseConnectorPropertiesWidget::databasePathChanged,
                         this, &DatabaseConnector::onDatabasePathChanged);
    }
    return propertiesWidget;
}

QFuture<DataPacket> DatabaseConnector::Execute(const DataPacket& inputs)
{
    const QString sql = inputs.value(QStringLiteral("sql")).toString();
    const QString dbPath = m_databasePath;

    return QtConcurrent::run([sql, dbPath]() -> DataPacket {
        DataPacket packet;
        const QString outKey = QStringLiteral("stdout");
        const QString errKey = QStringLiteral("stderr");

        if (dbPath.trimmed().isEmpty()) {
            const QString msg = QStringLiteral("ERROR: Database path is empty.");
            packet.insert(outKey, QString());
            packet.insert(errKey, msg);
            qWarning() << "DatabaseConnector:" << msg;
            return packet;
        }
        if (sql.trimmed().isEmpty()) {
            const QString msg = QStringLiteral("ERROR: SQL is empty.");
            packet.insert(outKey, QString());
            packet.insert(errKey, msg);
            qWarning() << "DatabaseConnector:" << msg;
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
                qWarning() << "DatabaseConnector: failed to open DB" << dbPath << ":" << stderrText;
                // Ensure close before leaving scope
                if (db.isOpen()) db.close();
            } else {
                QSqlQuery query(db);
                if (!query.exec(sql)) {
                    stderrText = query.lastError().text();
                    qWarning() << "DatabaseConnector: query exec failed:" << stderrText;
                    db.close();
                } else {
                    // Success: format results. If not a SELECT, show rows affected.
                    if (!query.isSelect()) {
                        const qint64 rows = query.numRowsAffected();
                        stdoutText = QStringLiteral("Rows affected: ") + QString::number(rows);
                    } else {
                        // Build CSV with headers
                        auto csvEscape = [](const QString& in) -> QString {
                            QString s = in;
                            s.replace('"', "\"\"");
                            return QStringLiteral("\"") + s + QStringLiteral("\"");
                        };

                        QSqlRecord rec = query.record();
                        const int colCount = rec.count();
                        QStringList lines;

                        // Header line
                        {
                            QStringList headers;
                            headers.reserve(colCount);
                            for (int i = 0; i < colCount; ++i) {
                                headers << csvEscape(rec.fieldName(i));
                            }
                            lines << headers.join(QLatin1Char(','));
                        }

                        // Rows
                        while (query.next()) {
                            QStringList row;
                            row.reserve(colCount);
                            for (int i = 0; i < colCount; ++i) {
                                const QVariant v = query.value(i);
                                if (v.isNull()) {
                                    row << QStringLiteral("NULL");
                                } else {
                                    row << csvEscape(v.toString());
                                }
                            }
                            lines << row.join(QLatin1Char(','));
                        }

                        stdoutText = lines.join(QLatin1Char('\n'));
                    }
                    db.close();
                }
            }
        }
        // After db and query are out of scope, remove the connection
        QSqlDatabase::removeDatabase(connectionName);

        packet.insert(QStringLiteral("stdout"), stdoutText);
        packet.insert(QStringLiteral("stderr"), stderrText);
        return packet;
    });
}

QJsonObject DatabaseConnector::saveState() const
{
    QJsonObject state;
    state.insert(QStringLiteral("databasePath"), m_databasePath);
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
}

void DatabaseConnector::onDatabasePathChanged(const QString& path)
{
    if (m_databasePath == path) return;
    m_databasePath = path;
}
