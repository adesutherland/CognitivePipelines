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

class ScriptNodeIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
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
    }
};

TEST_F(ScriptNodeIntegrationTest, SqliteIntegration)
{
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
    // The script connects to the database, creates a table, inserts data and returns it.
    // Our QuickJSRuntime now supports top-level return by wrapping non-module scripts in an IIFE,
    // and it automatically captures the return value into the "output" field.
    QString script = 
        "sqlite.connect(\"" + dbPath + "\");\n"
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

TEST_F(ScriptNodeIntegrationTest, ArrayPassThrough)
{
    UniversalScriptConnector node;
    QString script = "pipeline.setOutput(\"out\", [\"A\", \"B\"]);";

    QJsonObject state;
    state.insert(QStringLiteral("scriptCode"), script);
    state.insert(QStringLiteral("engineId"), QStringLiteral("quickjs"));
    state.insert(QStringLiteral("enableFanOut"), false);
    node.loadState(state);

    TokenList outTokens = node.execute({});

    ASSERT_EQ(outTokens.size(), 1);
    QVariant outVal = outTokens.front().data.value(QStringLiteral("out"));
    ASSERT_EQ(outVal.typeId(), QMetaType::QVariantList);
    QVariantList list = outVal.toList();
    ASSERT_EQ(list.size(), 2);
    EXPECT_EQ(list.at(0).toString(), QStringLiteral("A"));
    EXPECT_EQ(list.at(1).toString(), QStringLiteral("B"));
}

TEST_F(ScriptNodeIntegrationTest, ArrayFanOut)
{
    UniversalScriptConnector node;
    QString script = "pipeline.setOutput(\"out\", [\"A\", \"B\"]);";

    QJsonObject state;
    state.insert(QStringLiteral("scriptCode"), script);
    state.insert(QStringLiteral("engineId"), QStringLiteral("quickjs"));
    state.insert(QStringLiteral("enableFanOut"), true);
    node.loadState(state);

    TokenList outTokens = node.execute({});

    ASSERT_EQ(outTokens.size(), 2);
    auto it = outTokens.begin();
    EXPECT_EQ(it->data.value(QStringLiteral("out")).toString(), QStringLiteral("A"));
    ++it;
    EXPECT_EQ(it->data.value(QStringLiteral("out")).toString(), QStringLiteral("B"));
}

TEST_F(ScriptNodeIntegrationTest, MixedTypes)
{
    UniversalScriptConnector node;
    QString script = "pipeline.setOutput(\"out\", \"SingleString\");";

    QJsonObject state;
    state.insert(QStringLiteral("scriptCode"), script);
    state.insert(QStringLiteral("engineId"), QStringLiteral("quickjs"));
    state.insert(QStringLiteral("enableFanOut"), true);
    node.loadState(state);

    TokenList outTokens = node.execute({});

    ASSERT_EQ(outTokens.size(), 1);
    EXPECT_EQ(outTokens.front().data.value(QStringLiteral("out")).toString(), QStringLiteral("SingleString"));
}

TEST_F(ScriptNodeIntegrationTest, UnifiedLoggingAndStatus)
{
    UniversalScriptConnector node;
    
    // Verify Descriptor
    NodeDescriptor desc = node.getDescriptor();
    ASSERT_TRUE(desc.outputPins.contains(QStringLiteral("status")));
    EXPECT_EQ(desc.outputPins.value(QStringLiteral("status")).name, QStringLiteral("Status"));

    // Test print() and console.error()
    QString script = 
        "print('Hello Print');\n"
        "console.error('Hello Error');\n"
        "pipeline.setOutput('status', 'CustomStatus');";
    
    QJsonObject state;
    state.insert(QStringLiteral("scriptCode"), script);
    state.insert(QStringLiteral("engineId"), QStringLiteral("quickjs"));
    node.loadState(state);

    TokenList outTokens = node.execute({});
    ASSERT_EQ(outTokens.size(), 1);
    DataPacket data = outTokens.front().data;
    
    QString logs = data.value(QStringLiteral("logs")).toString();
    EXPECT_TRUE(logs.contains(QStringLiteral("Hello Print")));
    EXPECT_TRUE(logs.contains(QStringLiteral("ERROR: Hello Error")));
    
    // Test custom status
    EXPECT_EQ(data.value(QStringLiteral("status")).toString(), QStringLiteral("CustomStatus"));

    // Test default OK status
    state.insert(QStringLiteral("scriptCode"), QStringLiteral("console.log('done');"));
    node.loadState(state);
    outTokens = node.execute({});
    EXPECT_EQ(outTokens.front().data.value(QStringLiteral("status")).toString(), QStringLiteral("OK"));

    // Test default FAIL status
    state.insert(QStringLiteral("scriptCode"), QStringLiteral("throw new Error('boom');"));
    node.loadState(state);
    outTokens = node.execute({});
    EXPECT_EQ(outTokens.front().data.value(QStringLiteral("status")).toString(), QStringLiteral("FAIL"));
}

TEST_F(ScriptNodeIntegrationTest, FanOutPreservesLogs)
{
    UniversalScriptConnector node;
    QString script = "console.log(\"Log 1\");\n"
                     "pipeline.setOutput(\"out\", [\"A\", \"B\"]);";

    QJsonObject state;
    state.insert(QStringLiteral("scriptCode"), script);
    state.insert(QStringLiteral("engineId"), QStringLiteral("quickjs"));
    state.insert(QStringLiteral("enableFanOut"), true);
    node.loadState(state);

    TokenList outTokens = node.execute({});

    ASSERT_EQ(outTokens.size(), 2);
    for (const auto& token : outTokens) {
        EXPECT_TRUE(token.data.contains(QStringLiteral("logs")));
        EXPECT_TRUE(token.data.value(QStringLiteral("logs")).toString().contains(QStringLiteral("Log 1")));
    }
}

TEST_F(ScriptNodeIntegrationTest, InjectsFanOutSummaryIntoLogs)
{
    UniversalScriptConnector node;
    QString script = "pipeline.setOutput(\"out\", [\"A\", \"B\"]);";

    QJsonObject state;
    state.insert(QStringLiteral("scriptCode"), script);
    state.insert(QStringLiteral("engineId"), QStringLiteral("quickjs"));
    state.insert(QStringLiteral("enableFanOut"), true);
    node.loadState(state);

    TokenList outTokens = node.execute({});

    ASSERT_EQ(outTokens.size(), 2);
    for (const auto& token : outTokens) {
        QString logs = token.data.value(QStringLiteral("logs")).toString();
        EXPECT_TRUE(logs.contains(QStringLiteral("out[1]: A")));
        EXPECT_TRUE(logs.contains(QStringLiteral("out[2]: B")));
        EXPECT_FALSE(logs.contains(QStringLiteral("[0]:")));
    }
}

TEST_F(ScriptNodeIntegrationTest, NoSummaryInSingleMode)
{
    UniversalScriptConnector node;
    QString script = "pipeline.setOutput(\"out\", [\"A\", \"B\"]);";

    QJsonObject state;
    state.insert(QStringLiteral("scriptCode"), script);
    state.insert(QStringLiteral("engineId"), QStringLiteral("quickjs"));
    state.insert(QStringLiteral("enableFanOut"), false);
    node.loadState(state);

    TokenList outTokens = node.execute({});

    ASSERT_EQ(outTokens.size(), 1);
    QString logs = outTokens.front().data.value(QStringLiteral("logs")).toString();
    EXPECT_FALSE(logs.contains(QStringLiteral("--- Output Data ---")));
    EXPECT_FALSE(logs.contains(QStringLiteral("out:")));
}
