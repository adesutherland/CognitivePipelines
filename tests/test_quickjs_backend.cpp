#include <gtest/gtest.h>
#include <QString>
#include <QVariant>
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

TEST(QuickJSBackendTest, SyntaxError) {
    QuickJSRuntime runtime;
    MockScriptHost host;
    QString script = "var x = ;";
    
    bool success = runtime.execute(script, &host);
    
    EXPECT_FALSE(success);
    EXPECT_FALSE(host.errors.empty());
}
