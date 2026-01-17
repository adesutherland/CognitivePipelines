#include <gtest/gtest.h>
#include <QString>
#include <QVariant>
#include <QDateTime>
#include <QDir>
#include <map>
#include <vector>
#include "IScriptHost.h"

// Note: This file will intentionally not compile until QuickJSRuntime is implemented.
// We include it here as it's the expected location for the class definition.
#include "QuickJSRuntime.h" 

class MockScriptHost : public IScriptHost {
public:
    void log(const QString& message) override {
        logs.push_back(message);
    }

    QVariant getInput(const QString& key) override {
        return inputs[key];
    }

    void setOutput(const QString& key, const QVariant& value) override {
        outputs[key] = value;
    }

    void setError(const QString& message) override {
        errors.push_back(message);
    }

    QString getTempDir() const override {
        return QDir::tempPath();
    }

    std::vector<QString> logs;
    std::vector<QString> errors;
    std::map<QString, QVariant> inputs;
    std::map<QString, QVariant> outputs;
};

TEST(QuickJSBackendTest, Identity) {
    // This will fail to compile because QuickJSRuntime is an incomplete type
    QuickJSRuntime runtime;
    EXPECT_EQ(runtime.getEngineId(), "quickjs");
}

TEST(QuickJSBackendTest, BasicExecutionAndLogging) {
    QuickJSRuntime runtime;
    MockScriptHost host;
    QString script = "console.log(\"Hello from JS\");";
    
    bool success = runtime.execute(script, &host);
    
    EXPECT_TRUE(success);
    ASSERT_FALSE(host.logs.empty());
    EXPECT_EQ(host.logs[0], "Hello from JS");
}

TEST(QuickJSBackendTest, DataExchange) {
    QuickJSRuntime runtime;
    MockScriptHost host;
    host.inputs["in_key"] = "Hello";
    
    QString script = 
        "var val = pipeline.getInput(\"in_key\");\n"
        "pipeline.setOutput(\"out_key\", val + \" world\");";
    
    bool success = runtime.execute(script, &host);
    
    EXPECT_TRUE(success);
    EXPECT_EQ(host.outputs["out_key"].toString(), "Hello world");
}

TEST(QuickJSBackendTest, ArrayOutput) {
    QuickJSRuntime runtime;
    MockScriptHost host;
    
    QString script = "pipeline.setOutput(\"out_key\", [\"a\", \"b\"]);";
    
    bool success = runtime.execute(script, &host);
    
    EXPECT_TRUE(success);
    QVariant val = host.outputs["out_key"];
    EXPECT_EQ(val.typeId(), QMetaType::QVariantList);
    QVariantList list = val.toList();
    ASSERT_EQ(list.size(), 2);
    EXPECT_EQ(list[0].toString(), "a");
    EXPECT_EQ(list[1].toString(), "b");
}

TEST(QuickJSBackendTest, ObjectOutput) {
    QuickJSRuntime runtime;
    MockScriptHost host;
    
    QString script = "pipeline.setOutput(\"out_key\", { \"x\": 1, \"y\": \"two\" });";
    
    bool success = runtime.execute(script, &host);
    
    EXPECT_TRUE(success);
    QVariant val = host.outputs["out_key"];
    EXPECT_EQ(val.typeId(), QMetaType::QVariantMap);
    QVariantMap map = val.toMap();
    EXPECT_EQ(map["x"].toInt(), 1);
    EXPECT_EQ(map["y"].toString(), "two");
}

TEST(QuickJSBackendTest, SyntaxError) {
    QuickJSRuntime runtime;
    MockScriptHost host;
    QString script = "var x = ;";
    
    bool success = runtime.execute(script, &host);
    
    EXPECT_FALSE(success);
    EXPECT_FALSE(host.errors.empty());
}

TEST(QuickJSBackendTest, StandardModulesImport) {
    QuickJSRuntime runtime;
    MockScriptHost host;
    QString script = "import * as std from 'std';\n"
                     "import * as os from 'os';\n"
                     "if (typeof std.gc === 'function' && typeof os.open === 'function') {\n"
                     "  console.log('Modules loaded correctly');\n"
                     "} else {\n"
                     "  console.log('Module content missing');\n"
                     "}";
    
    bool success = runtime.execute(script, &host);
    
    EXPECT_TRUE(success);
    ASSERT_FALSE(host.logs.empty());
    EXPECT_EQ(host.logs[0], "Modules loaded correctly");
}

TEST(QuickJSBackendTest, SqliteIntegration) {
    QuickJSRuntime runtime;
    MockScriptHost host;
    
    // Test basic SELECT without table
    QString script = 
        "var dbPath = pipeline.getTempDir() + '/test_integration.db';\n"
        "sqlite.connect(dbPath);\n"
        "var res = sqlite.exec(\"SELECT 'test' as col\");\n"
        "if (Array.isArray(res) && res.length > 0 && res[0].col === 'test') {\n"
        "  pipeline.setOutput(\"result\", \"success\");\n"
        "} else {\n"
        "  pipeline.setOutput(\"result\", \"failure\");\n"
        "  console.log(\"Actual result: \" + JSON.stringify(res));\n"
        "}";
    
    bool success = runtime.execute(script, &host);
    
    EXPECT_TRUE(success);
    EXPECT_EQ(host.outputs["result"].toString(), "success");
}

TEST(QuickJSBackendTest, SqliteFullWorkflow) {
    QuickJSRuntime runtime;
    MockScriptHost host;
    
    // Use a unique table name to avoid conflicts between test runs if scripts.db persists
    QString tableName = "js_test_" + QString::number(QDateTime::currentMSecsSinceEpoch());

    QString script = 
        "var dbPath = pipeline.getTempDir() + '/test_workflow.db';\n"
        "sqlite.connect(dbPath);\n"
        "sqlite.exec(\"CREATE TABLE " + tableName + " (val TEXT, num INTEGER)\");\n"
        "sqlite.exec(\"INSERT INTO " + tableName + " (val, num) VALUES (?, ?)\", ['hello', 42]);\n"
        "var res = sqlite.exec(\"SELECT val, num FROM " + tableName + " WHERE num = ?\", [42]);\n"
        "pipeline.setOutput(\"val\", res[0].val);\n"
        "pipeline.setOutput(\"num\", res[0].num);";
    
    bool success = runtime.execute(script, &host);
    
    EXPECT_TRUE(success);
    EXPECT_EQ(host.outputs["val"].toString(), "hello");
    EXPECT_EQ(host.outputs["num"].toInt(), 42);
}
