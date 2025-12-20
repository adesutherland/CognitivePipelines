//
// Integration-style unit test to verify LoopNode fan-out executes all iterations
// and downstream nodes receive tokens in order (A, B, C).
//

#include <gtest/gtest.h>

#include <QApplication>
#include <QEventLoop>
#include <QTimer>

#include "test_app.h"
#include "NodeGraphModel.h"
#include "ExecutionEngine.h"
#include "ToolNodeDelegate.h"
#include "TextInputNode.h"
#include "LoopNode.h"
#include "PromptBuilderNode.h"

using namespace QtNodes;

static QApplication* ensureApp_loop_integration()
{
    return sharedTestApp();
}

TEST(LoopIntegrationTest, DownstreamExecutesThreeTimesInOrder)
{
    ensureApp_loop_integration();

    NodeGraphModel model;

    // Build: TextInput -> Loop(body) -> PromptBuilder
    NodeId textId = model.addNode(QStringLiteral("text-input"));
    NodeId loopId = model.addNode(QStringLiteral("loop-foreach"));
    NodeId promptId = model.addNode(QStringLiteral("prompt-builder"));

    ASSERT_NE(textId, InvalidNodeId);
    ASSERT_NE(loopId, InvalidNodeId);
    ASSERT_NE(promptId, InvalidNodeId);

    // text.output(0) -> loop.input(0)
    model.addConnection(ConnectionId{ textId, 0u, loopId, 0u });
    // loop.body(output index 0) -> prompt.input(0)
    model.addConnection(ConnectionId{ loopId, 0u, promptId, 0u });

    // Configure TextInput content and PromptBuilder template
    auto* textDel = model.delegateModel<ToolNodeDelegate>(textId);
    ASSERT_NE(textDel, nullptr);
    auto textConn = textDel->connector();
    ASSERT_TRUE(textConn);
    auto* textTool = dynamic_cast<TextInputNode*>(textConn.get());
    ASSERT_NE(textTool, nullptr);
    textTool->setText(QStringLiteral("[\"A\",\"B\",\"C\"]"));

    auto* promptDel = model.delegateModel<ToolNodeDelegate>(promptId);
    ASSERT_NE(promptDel, nullptr);
    auto promptConn = promptDel->connector();
    ASSERT_TRUE(promptConn);
    auto* promptTool = dynamic_cast<PromptBuilderNode*>(promptConn.get());
    ASSERT_NE(promptTool, nullptr);
    promptTool->setTemplateText(QStringLiteral("{input}"));

    ExecutionEngine engine(&model);

    // Capture PromptBuilder outputs in execution order by listening to nodeOutputChanged
    QStringList seen;
    QObject::connect(&engine, &ExecutionEngine::nodeOutputChanged, &engine,
                     [&](NodeId nodeId){
        if (nodeId != promptId) return;
        const DataPacket pkt = engine.nodeOutput(nodeId);
        const QString s = pkt.value(QStringLiteral("prompt")).toString();
        if (!s.isEmpty()) seen << s;
    });

    bool finished = false;
    QObject::connect(&engine, &ExecutionEngine::pipelineFinished, &engine, [&](const DataPacket&){ finished = true; });

    engine.run();

    QEventLoop loop;
    QTimer timer; timer.setSingleShot(true);
    QObject::connect(&engine, &ExecutionEngine::pipelineFinished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(5000);
    loop.exec();

    ASSERT_TRUE(finished) << "Engine did not finish";
    ASSERT_EQ(seen.size(), 3);
    EXPECT_EQ(seen.at(0), QStringLiteral("A"));
    EXPECT_EQ(seen.at(1), QStringLiteral("B"));
    EXPECT_EQ(seen.at(2), QStringLiteral("C"));
}

TEST(LoopIntegrationTest, ConcurrencyTwoSourcesFinishInParallel)
{
    ensureApp_loop_integration();

    NodeGraphModel model;

    // Two independent source nodes (TextInput) should execute concurrently
    NodeId textA = model.addNode(QStringLiteral("text-input"));
    NodeId textB = model.addNode(QStringLiteral("text-input"));

    ASSERT_NE(textA, InvalidNodeId);
    ASSERT_NE(textB, InvalidNodeId);

    // Configure both sources with simple payloads
    auto* delA = model.delegateModel<ToolNodeDelegate>(textA);
    auto* delB = model.delegateModel<ToolNodeDelegate>(textB);
    ASSERT_NE(delA, nullptr);
    ASSERT_NE(delB, nullptr);
    auto connA = delA->connector();
    auto connB = delB->connector();
    ASSERT_TRUE(connA);
    ASSERT_TRUE(connB);
    auto* toolA = dynamic_cast<TextInputNode*>(connA.get());
    auto* toolB = dynamic_cast<TextInputNode*>(connB.get());
    ASSERT_NE(toolA, nullptr);
    ASSERT_NE(toolB, nullptr);
    toolA->setText(QStringLiteral("A"));
    toolB->setText(QStringLiteral("B"));

    ExecutionEngine engine(&model);
    // Simulate slow nodes via engine slow-motion delay; tasks should overlap
    engine.setExecutionDelay(500);

    bool finished = false;
    QObject::connect(&engine, &ExecutionEngine::pipelineFinished, &engine, [&](const DataPacket&){ finished = true; });

    QElapsedTimer t; t.start();
    engine.run();

    QEventLoop loop;
    QTimer timer; timer.setSingleShot(true);
    QObject::connect(&engine, &ExecutionEngine::pipelineFinished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(5000);
    loop.exec();

    const qint64 elapsed = t.elapsed();
    ASSERT_TRUE(finished) << "Engine did not finish";
    // Expect roughly one delay interval, not two (tolerance for CI variance)
    EXPECT_GE(elapsed, 350);
    EXPECT_LT(elapsed, 900);
}
