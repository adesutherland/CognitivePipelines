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

#include <gtest/gtest.h>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QFile>
#include "ScriptDatabaseBridge.h"

TEST(ScriptDatabaseBridgeTest, FullWorkflow) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QString dbPath = dir.path() + "/test.db";

    ScriptDatabaseBridge bridge(dbPath);

    // 1. Create Table
    QJsonValue createResult = bridge.exec("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)");
    ASSERT_TRUE(createResult.isObject());
    EXPECT_FALSE(createResult.toObject().contains("error")) << createResult.toObject()["error"].toString().toStdString();
    
    // 2. Insert Data
    QJsonValue insertResult = bridge.exec("INSERT INTO users (name, age) VALUES ('Alice', 30)");
    ASSERT_TRUE(insertResult.isObject());
    EXPECT_EQ(insertResult.toObject()["rowsAffected"].toInt(), 1);
    EXPECT_TRUE(insertResult.toObject().contains("lastInsertId"));

    bridge.exec("INSERT INTO users (name, age) VALUES ('Bob', 25)");

    // 3. Select Data
    QJsonValue selectResult = bridge.exec("SELECT * FROM users ORDER BY age ASC");
    ASSERT_TRUE(selectResult.isArray());
    QJsonArray rows = selectResult.toArray();
    ASSERT_EQ(rows.size(), 2);
    
    EXPECT_EQ(rows[0].toObject()["name"].toString(), "Bob");
    EXPECT_EQ(rows[0].toObject()["age"].toInt(), 25);
    EXPECT_EQ(rows[1].toObject()["name"].toString(), "Alice");
    EXPECT_EQ(rows[1].toObject()["age"].toInt(), 30);

    // 4. Invalid SQL
    QJsonValue errorResult = bridge.exec("SELECT * FROM non_existent_table");
    ASSERT_TRUE(errorResult.isObject());
    EXPECT_TRUE(errorResult.toObject().contains("error"));
}

TEST(ScriptDatabaseBridgeTest, TransactionRollback) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QString dbPath = dir.path() + "/test_rollback.db";

    ScriptDatabaseBridge bridge(dbPath);
    bridge.exec("CREATE TABLE items (name TEXT UNIQUE)");
    bridge.exec("INSERT INTO items (name) VALUES ('item1')");

    // This should fail because 'item1' already exists, and it should rollback (though it's a single statement)
    QJsonValue failResult = bridge.exec("INSERT INTO items (name) VALUES ('item1')");
    ASSERT_TRUE(failResult.isObject());
    EXPECT_TRUE(failResult.toObject().contains("error"));

    QJsonValue selectResult = bridge.exec("SELECT count(*) as count FROM items");
    EXPECT_EQ(selectResult.toArray()[0].toObject()["count"].toInt(), 1);
}
