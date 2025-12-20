#include <gtest/gtest.h>

#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <QElapsedTimer>
#include <QVariant>
#include <QMutexLocker>

#include "test_app.h"
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
    return sharedTestApp();
}

class ExecutionEngineSignatureFriend {
public:
    static QByteArray compute(ExecutionEngine& engine, const QVariantMap& payload)
    {
        return engine.computeInputSignature(payload);
    }

    static void setLastSignature(ExecutionEngine& engine, const QUuid& nodeUuid, const QByteArray& sig)
    {
        QMutexLocker locker(&engine.m_queueMutex);
        engine.m_lastInputSignature.insert(nodeUuid, sig);
    }

    static QByteArray lastSignature(const ExecutionEngine& engine, const QUuid& nodeUuid)
    {
        QMutexLocker locker(&engine.m_queueMutex);
        return engine.m_lastInputSignature.value(nodeUuid);
    }

    static void setCurrentRunId(ExecutionEngine& engine, const QUuid& runId)
    {
        engine.m_currentRunId = runId;
    }

    static void invokeHandle(ExecutionEngine& engine,
                             QtNodes::NodeId nodeId,
                             const QUuid& nodeUuid,
                             const TokenList& outputs,
                             const QUuid& runId)
    {
        engine.handleTaskCompleted(nodeId, nodeUuid, outputs, runId);
    }
};

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

TEST(ExecutionEngineTest, ClearsDedupSignatureWhenNodeEmitsNoOutput)
{
    ensureApp();

    NodeGraphModel model;
    ExecutionEngine engine(&model);

    const QUuid nodeUuid = QUuid::createUuid();
    const QByteArray sig = QByteArrayLiteral("sig");
    const QUuid runId = QUuid::createUuid();

    ExecutionEngineSignatureFriend::setCurrentRunId(engine, runId);
    ExecutionEngineSignatureFriend::setLastSignature(engine, nodeUuid, sig);

    EXPECT_FALSE(ExecutionEngineSignatureFriend::lastSignature(engine, nodeUuid).isEmpty());

    ExecutionEngineSignatureFriend::invokeHandle(engine, /*nodeId*/1, nodeUuid, TokenList{}, runId);

    EXPECT_TRUE(ExecutionEngineSignatureFriend::lastSignature(engine, nodeUuid).isEmpty());
}

TEST(ExecutionEngineTest, SlowMotionDelaysFirstDownstreamDispatch)
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

    // Configure nodes
    auto* textDel = model.delegateModel<ToolNodeDelegate>(textNodeId);
    ASSERT_NE(textDel, nullptr);
    auto textConn = textDel->connector();
    ASSERT_TRUE(textConn);
    auto* textTool = dynamic_cast<TextInputNode*>(textConn.get());
    ASSERT_NE(textTool, nullptr);
    textTool->setText(QStringLiteral("Hello"));

    auto* promptDel = model.delegateModel<ToolNodeDelegate>(promptNodeId);
    ASSERT_NE(promptDel, nullptr);
    auto promptConn = promptDel->connector();
    ASSERT_TRUE(promptConn);
    auto* promptTool = dynamic_cast<PromptBuilderNode*>(promptConn.get());
    ASSERT_NE(promptTool, nullptr);
    promptTool->setTemplateText(QStringLiteral("{input}"));

    ExecutionEngine engine(&model);
    engine.setExecutionDelay(200); // slow-motion pacing

    QElapsedTimer timer;
    timer.start();
    qint64 observedMs = -1;

    QObject::connect(&engine, &ExecutionEngine::nodeOutputChanged, &engine,
                     [&](NodeId nodeId){
        if (nodeId != promptNodeId) return;
        if (observedMs < 0) observedMs = timer.elapsed();
    });

    bool finished = false;
    QObject::connect(&engine, &ExecutionEngine::pipelineFinished, &engine,
                     [&](const DataPacket&){ finished = true; });

    engine.run();

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&engine, &ExecutionEngine::pipelineFinished, &loop, &QEventLoop::quit);
    timeout.start(5000);
    loop.exec();

    ASSERT_TRUE(finished) << "Engine did not finish within timeout";
    ASSERT_GE(observedMs, 0) << "Prompt node never emitted output";

    // Expect the downstream node to start after at least one delay interval, but well under 1s.
    EXPECT_GE(observedMs, 150);
    EXPECT_LT(observedMs, 1000);
}
