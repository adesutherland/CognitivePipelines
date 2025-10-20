#include <gtest/gtest.h>

#include <QApplication>
#include <QEventLoop>
#include <QTimer>

#include <QtNodes/internal/Definitions.hpp>

#include "NodeGraphModel.h"
#include "ExecutionEngine.h"
#include "ToolNodeDelegate.h"
#include "TextInputNode.h"
#include "PromptBuilderNode.h"

using namespace QtNodes;

// Ensure a QApplication exists for any queued connections and potential widgets
static QApplication* ensureApp()
{
    static QApplication* app = nullptr;
    if (!app) {
        int argc = 0;
        char* argv[] = { nullptr };
        app = new QApplication(argc, argv);
    }
    return app;
}

TEST(ExecutionEngineTest, LinearTwoNodes_DataFlowsAndOrderIsCorrect)
{
    ensureApp();

    NodeGraphModel model;

    // Build a simple pipeline: TextInput -> PromptBuilder
    NodeId textNodeId = model.addNode(QStringLiteral("text-input"));
    NodeId promptNodeId = model.addNode(QStringLiteral("prompt-builder"));

    ASSERT_NE(textNodeId, InvalidNodeId);
    ASSERT_NE(promptNodeId, InvalidNodeId);

    // Connect text.output(0) -> prompt.input(0)
    ConnectionId conn{ textNodeId, 0u, promptNodeId, 0u };
    model.addConnection(conn);

    // Configure nodes via their connectors directly for simplicity
    auto* textDel = model.delegateModel<ToolNodeDelegate>(textNodeId);
    ASSERT_NE(textDel, nullptr);
    auto textConn = textDel->connector();
    ASSERT_TRUE(textConn);
    auto* textTool = dynamic_cast<TextInputNode*>(textConn.get());
    ASSERT_NE(textTool, nullptr);
    textTool->setText(QStringLiteral("Bob"));

    auto* promptDel = model.delegateModel<ToolNodeDelegate>(promptNodeId);
    ASSERT_NE(promptDel, nullptr);
    auto promptConn = promptDel->connector();
    ASSERT_TRUE(promptConn);
    auto* promptTool = dynamic_cast<PromptBuilderNode*>(promptConn.get());
    ASSERT_NE(promptTool, nullptr);
    promptTool->setTemplateText(QStringLiteral("Hello {input}!"));

    // Run the engine and capture signals
    ExecutionEngine engine(&model);

    QStringList execOrder; // capture node names in execution order
    QObject::connect(&engine, &ExecutionEngine::nodeLog, &engine, [&execOrder](const QString& msg) {
        // Expect messages like: "Executing Node: <id> <name> with INPUT: {...}"
        if (msg.startsWith(QLatin1String("Executing Node:"))) {
            execOrder << msg;
        }
    });

    bool finished = false;
    DataPacket finalOut;

    QObject::connect(&engine, &ExecutionEngine::pipelineFinished, &engine, [&](const DataPacket& out) {
        finished = true;
        finalOut = out;
    });

    engine.run();

    // Pump event loop until finished or timeout
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&engine, &ExecutionEngine::pipelineFinished, &loop, &QEventLoop::quit);
    timeout.start(5000);
    loop.exec();

    ASSERT_TRUE(finished) << "Engine did not finish within timeout";

    // Verify the final output contains the prompt
    ASSERT_TRUE(finalOut.contains(QStringLiteral("prompt")));
    EXPECT_EQ(finalOut.value(QStringLiteral("prompt")).toString(), QStringLiteral("Hello Bob!"));

    // Verify order: Text Input should execute before Prompt Builder
    // We expect two "Executing Node:" log entries in order
    ASSERT_GE(execOrder.size(), 2);
    const QString first = execOrder.at(0);
    const QString second = execOrder.at(1);

    EXPECT_NE(first.indexOf(QLatin1String("Text Input")), -1);
    EXPECT_NE(second.indexOf(QLatin1String("Prompt Builder")), -1);
}
