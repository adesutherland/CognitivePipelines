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

#include "ScriptDatabaseBridge.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>
#include <QVariant>
#include "Logger.h"

ScriptDatabaseBridge::ScriptDatabaseBridge(const QString& dbPath, QObject* parent)
    : QObject(parent), m_dbPath(dbPath)
{
}

ScriptDatabaseBridge::~ScriptDatabaseBridge()
{
}

QJsonValue ScriptDatabaseBridge::exec(const QString& sql)
{
    if (m_dbPath.isEmpty()) {
        QJsonObject errorObj;
        errorObj.insert(QStringLiteral("error"), QStringLiteral("Database path is empty"));
        return errorObj;
    }

    const QString connectionName = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonValue result;

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(m_dbPath);

        if (!db.open()) {
            QJsonObject errorObj;
            errorObj.insert(QStringLiteral("error"), db.lastError().text());
            result = errorObj;
        } else {
            if (!db.transaction()) {
                QJsonObject errorObj;
                errorObj.insert(QStringLiteral("error"), QStringLiteral("Failed to start transaction: ") + db.lastError().text());
                result = errorObj;
            } else {
                QSqlQuery query(db);
                if (!query.exec(sql)) {
                    QJsonObject errorObj;
                    errorObj.insert(QStringLiteral("error"), query.lastError().text());
                    result = errorObj;
                    db.rollback();
                } else {
                    if (query.isSelect()) {
                        QJsonArray rows;
                        QSqlRecord record = query.record();
                        int columnCount = record.count();

                        while (query.next()) {
                            QJsonObject row;
                            for (int i = 0; i < columnCount; ++i) {
                                row.insert(record.fieldName(i), QJsonValue::fromVariant(query.value(i)));
                            }
                            rows.append(row);
                        }
                        result = rows;
                    } else {
                        QJsonObject meta;
                        meta.insert(QStringLiteral("rowsAffected"), query.numRowsAffected());
                        QVariant lastId = query.lastInsertId();
                        if (lastId.isValid()) {
                            meta.insert(QStringLiteral("lastInsertId"), QJsonValue::fromVariant(lastId));
                        }
                        result = meta;
                    }

                    if (!db.commit()) {
                        QJsonObject errorObj;
                        errorObj.insert(QStringLiteral("error"), QStringLiteral("Failed to commit transaction: ") + db.lastError().text());
                        result = errorObj;
                    }
                }
            }
            db.close();
        }
    }

    QSqlDatabase::removeDatabase(connectionName);
    return result;
}
