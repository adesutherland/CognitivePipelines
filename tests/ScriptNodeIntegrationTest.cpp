#include <gtest/gtest.h>
#include <QApplication>
#include <QTemporaryFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariantMap>
#include <QVariantList>

#include "UniversalScriptConnector.h"
#include "ExecutionToken.h"
#include "QuickJSRuntime.h"

TEST(ScriptNodeIntegrationTest, SqliteIntegration)
{
    // Need QApplication for some components
    if (!qApp) {
        static int argc = 1;
        static char* argv[] = {(char*)"test"};
        new QApplication(argc, argv);
    }

    // Register quickjs engine (normally done in NodeGraphModel)
    ScriptEngineRegistry::instance().registerEngine(QStringLiteral("quickjs"), []() {
        return std::make_unique<QuickJSRuntime>();
    });

    // 1. Setup temporary database
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    QString dbPath = tempFile.fileName();
    tempFile.close();

    // Set environment variable for QuickJSRuntime to use this temp file
    qputenv("CP_QUICKJS_DB_PATH", dbPath.toUtf8());

    // 2. Instantiate UniversalScriptConnector
    UniversalScriptConnector node;

    // 3. Set the script
    // The script creates a table, inserts data and returns it.
    // Our QuickJSRuntime now supports top-level return by wrapping non-module scripts in an IIFE,
    // and it automatically captures the return value into the "output" field.
    QString script = 
        "sqlite.exec(\"CREATE TABLE test_table (id INTEGER PRIMARY KEY, name TEXT)\");\n"
        "sqlite.exec(\"INSERT INTO test_table (name) VALUES ('integration_check')\");\n"
        "return sqlite.exec(\"SELECT * FROM test_table\");";

    QJsonObject state;
    state.insert(QStringLiteral("scriptCode"), script);
    state.insert(QStringLiteral("engineId"), QStringLiteral("quickjs"));
    node.loadState(state);

    // 4. Execution
    TokenList incomingTokens; // Dummy input
    TokenList outTokens = node.execute(incomingTokens);

    // 5. Assertions
    ASSERT_FALSE(outTokens.empty());
    DataPacket outData = outTokens.front().data;

    // The return value should be in "output" field
    ASSERT_TRUE(outData.contains(QStringLiteral("output"))) << "Output packet should contain 'output' field from return value";
    QVariant outputVar = outData.value(QStringLiteral("output"));

    // Expected data: [{"id": 1, "name": "integration_check"}]
    QVariantList list = outputVar.toList();
    ASSERT_EQ(list.size(), 1);

    QVariantMap row = list.at(0).toMap();
    EXPECT_EQ(row.value(QStringLiteral("id")).toInt(), 1);
    EXPECT_EQ(row.value(QStringLiteral("name")).toString(), QStringLiteral("integration_check"));

    // Cleanup env var
    qunsetenv("CP_QUICKJS_DB_PATH");
}
