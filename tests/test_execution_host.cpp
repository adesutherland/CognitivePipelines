#include <QtTest>
#include <QObject>
#include <QList>
#include <QString>
#include <QVariant>

#include "IScriptHost.h"
#include "CommonDataTypes.h"
#include "ExecutionScriptHost.h"

#include <gtest/gtest.h>

TEST(ExecutionScriptHostTest, InputRetrieval) {
    // Create input data
    DataPacket in;
    in["input_key"] = "input_value";
    
    DataPacket out;
    QList<QString> logs;

    ExecutionScriptHost host(in, out, logs);

    // Assert correct input retrieval
    EXPECT_EQ(host.getInput("input_key").toString(), QString("input_value"));
    
    // Assert missing key returns invalid/null QVariant
    EXPECT_FALSE(host.getInput("missing_key").isValid());
}

TEST(ExecutionScriptHostTest, OutputSetting) {
    DataPacket in;
    DataPacket out;
    QList<QString> logs;

    ExecutionScriptHost host(in, out, logs);

    // Set output
    host.setOutput("result_key", 12345);

    // Assert output was set in the referenced DataPacket
    EXPECT_EQ(out["result_key"].toInt(), 12345);
}

TEST(ExecutionScriptHostTest, Logging) {
    DataPacket in;
    DataPacket out;
    QList<QString> logs;

    ExecutionScriptHost host(in, out, logs);

    // Log a message
    host.log("Test Log");

    // Assert log was captured in the referenced list
    EXPECT_EQ(logs.size(), 1);
    EXPECT_EQ(logs.first(), QString("Test Log"));
}

TEST(ExecutionScriptHostTest, ErrorSetting) {
    DataPacket in;
    DataPacket out;
    QList<QString> logs;

    ExecutionScriptHost host(in, out, logs);
    
    host.setError("Something went wrong");
    
    EXPECT_TRUE(logs.contains("Error: Something went wrong") || logs.size() > 0);
}
