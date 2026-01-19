//
// Reproduction test for ConditionalRouterNode execution routing.
//
// Verifies that the ExecutionEngine enforces "pin-gated" execution so that
// only the branch whose output pin received data from ConditionalRouterNode
// is actually scheduled and executed downstream.
//

#include <gtest/gtest.h>

#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <QtTest/QTest>

#include <QtNodes/internal/Definitions.hpp>

#include "test_app.h"
#include "NodeGraphModel.h"
#include "ExecutionEngine.h"
#include "ToolNodeDelegate.h"
#include "TextInputNode.h"
#include "TextOutputNode.h"
#include "ConditionalRouterNode.h"

using namespace QtNodes;

namespace {

static QApplication* ensureApp_RouterExecution()
{
    return sharedTestApp();
}

// Helper: run engine and wait up to timeout
static bool runEngineAndWait(ExecutionEngine& engine, int timeoutMs = 5000)
{
    bool finished = false;
    QObject::connect(&engine, &ExecutionEngine::pipelineFinished, &engine, [&](const DataPacket&) {
        finished = true;
    });
    engine.Run();

    QEventLoop loop;
    QTimer timeout; timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&engine, &ExecutionEngine::pipelineFinished, &loop, &QEventLoop::quit);
    timeout.start(timeoutMs);
    loop.exec();

    return finished;
}

} // namespace

TEST(RouterExecutionTest, testTrueBranch)
{
    ensureApp_RouterExecution();

    NodeGraphModel model;

    // Build pipeline:
    //   Data TextInput  -> ConditionalRouter.in
    //   Cond TextInput  -> ConditionalRouter.condition
    //   ConditionalRouter.true/false -> two TextOutputs
    const NodeId dataNodeId = model.addNode(QStringLiteral("text-input"));
    const NodeId condNodeId = model.addNode(QStringLiteral("text-input"));
    const NodeId routerNodeId = model.addNode(QStringLiteral("conditional-router"));
    const NodeId trueOutNodeId = model.addNode(QStringLiteral("text-output"));
    const NodeId falseOutNodeId = model.addNode(QStringLiteral("text-output"));

    ASSERT_NE(dataNodeId, InvalidNodeId);
    ASSERT_NE(condNodeId, InvalidNodeId);
    ASSERT_NE(routerNodeId, InvalidNodeId);
    ASSERT_NE(trueOutNodeId, InvalidNodeId);
    ASSERT_NE(falseOutNodeId, InvalidNodeId);

    // Resolve which inbound/outbound port indices correspond to the logical
    // "in", "condition", "true" and "false" pins so the test remains
    // robust to any internal ordering in ToolNodeDelegate.
    auto* routerDel = model.delegateModel<ToolNodeDelegate>(routerNodeId);
    ASSERT_NE(routerDel, nullptr);

    int inPortIndex = -1;
    int condPortIndex = -1;
    const unsigned int inCount = routerDel->nPorts(QtNodes::PortType::In);
    for (unsigned int i = 0; i < inCount; ++i) {
        const QString pinId = routerDel->pinIdForIndex(QtNodes::PortType::In, i);
        if (pinId == QString::fromLatin1(ConditionalRouterNode::kInputDataId)) {
            inPortIndex = static_cast<int>(i);
        } else if (pinId == QString::fromLatin1(ConditionalRouterNode::kInputConditionId)) {
            condPortIndex = static_cast<int>(i);
        }
    }

    int trueOutPortIndex = -1;
    int falseOutPortIndex = -1;
    const unsigned int outCount = routerDel->nPorts(QtNodes::PortType::Out);
    for (unsigned int i = 0; i < outCount; ++i) {
        const QString pinId = routerDel->pinIdForIndex(QtNodes::PortType::Out, i);
        if (pinId == QString::fromLatin1(ConditionalRouterNode::kOutputTrueId)) {
            trueOutPortIndex = static_cast<int>(i);
        } else if (pinId == QString::fromLatin1(ConditionalRouterNode::kOutputFalseId)) {
            falseOutPortIndex = static_cast<int>(i);
        }
    }

    ASSERT_GE(inPortIndex, 0);
    ASSERT_GE(condPortIndex, 0);
    ASSERT_GE(trueOutPortIndex, 0);
    ASSERT_GE(falseOutPortIndex, 0);

    // Wire data: data.output(0) -> router.in
    model.addConnection(ConnectionId{ dataNodeId,
                                      0u,
                                      routerNodeId,
                                      static_cast<QtNodes::PortIndex>(inPortIndex) });

    // Wire condition: cond.output(0) -> router.condition
    model.addConnection(ConnectionId{ condNodeId,
                                      0u,
                                      routerNodeId,
                                      static_cast<QtNodes::PortIndex>(condPortIndex) });

    // Wire router true/false outputs to the appropriate TextOutput nodes
    model.addConnection(ConnectionId{ routerNodeId,
                                      static_cast<QtNodes::PortIndex>(trueOutPortIndex),
                                      trueOutNodeId,
                                      0u });
    model.addConnection(ConnectionId{ routerNodeId,
                                      static_cast<QtNodes::PortIndex>(falseOutPortIndex),
                                      falseOutNodeId,
                                      0u });

    // Configure TextInputs
    auto* dataDel = model.delegateModel<ToolNodeDelegate>(dataNodeId);
    ASSERT_NE(dataDel, nullptr);
    auto dataConn = dataDel->connector();
    ASSERT_TRUE(dataConn);
    auto* dataTool = dynamic_cast<TextInputNode*>(dataConn.get());
    ASSERT_NE(dataTool, nullptr);
    dataTool->setText(QStringLiteral("payload"));

    auto* condDel = model.delegateModel<ToolNodeDelegate>(condNodeId);
    ASSERT_NE(condDel, nullptr);
    auto condConn = condDel->connector();
    ASSERT_TRUE(condConn);
    auto* condTool = dynamic_cast<TextInputNode*>(condConn.get());
    ASSERT_NE(condTool, nullptr);
    condTool->setText(QStringLiteral("true"));

    // Router uses the explicit condition pin; no need to touch defaultCondition
    auto routerConn = routerDel->connector();
    ASSERT_TRUE(routerConn);
    auto* routerTool = dynamic_cast<ConditionalRouterNode*>(routerConn.get());
    ASSERT_NE(routerTool, nullptr);

    // Track execution counts for downstream TextOutput nodes using nodeOutputChanged
    int trueExecCount = 0;
    int falseExecCount = 0;

    ExecutionEngine engine(&model);
    QObject::connect(&engine, &ExecutionEngine::nodeOutputChanged,
                     &engine, [&](NodeId nid) {
        if (nid == trueOutNodeId) {
            ++trueExecCount;
        } else if (nid == falseOutNodeId) {
            ++falseExecCount;
        }
    });

    ASSERT_TRUE(runEngineAndWait(engine));

    // With condition true: only the true branch should execute
    EXPECT_EQ(trueExecCount, 1);
    EXPECT_EQ(falseExecCount, 0);
}

TEST(RouterExecutionTest, testFalseBranch)
{
    ensureApp_RouterExecution();

    NodeGraphModel model;

    const NodeId dataNodeId = model.addNode(QStringLiteral("text-input"));
    const NodeId condNodeId = model.addNode(QStringLiteral("text-input"));
    const NodeId routerNodeId = model.addNode(QStringLiteral("conditional-router"));
    const NodeId trueOutNodeId = model.addNode(QStringLiteral("text-output"));
    const NodeId falseOutNodeId = model.addNode(QStringLiteral("text-output"));

    ASSERT_NE(dataNodeId, InvalidNodeId);
    ASSERT_NE(condNodeId, InvalidNodeId);
    ASSERT_NE(routerNodeId, InvalidNodeId);
    ASSERT_NE(trueOutNodeId, InvalidNodeId);
    ASSERT_NE(falseOutNodeId, InvalidNodeId);

    // Resolve inbound/outbound port indices for logical pins
    auto* routerDel = model.delegateModel<ToolNodeDelegate>(routerNodeId);
    ASSERT_NE(routerDel, nullptr);

    int inPortIndex = -1;
    int condPortIndex = -1;
    const unsigned int inCount = routerDel->nPorts(QtNodes::PortType::In);
    for (unsigned int i = 0; i < inCount; ++i) {
        const QString pinId = routerDel->pinIdForIndex(QtNodes::PortType::In, i);
        if (pinId == QString::fromLatin1(ConditionalRouterNode::kInputDataId)) {
            inPortIndex = static_cast<int>(i);
        } else if (pinId == QString::fromLatin1(ConditionalRouterNode::kInputConditionId)) {
            condPortIndex = static_cast<int>(i);
        }
    }

    int trueOutPortIndex = -1;
    int falseOutPortIndex = -1;
    const unsigned int outCount = routerDel->nPorts(QtNodes::PortType::Out);
    for (unsigned int i = 0; i < outCount; ++i) {
        const QString pinId = routerDel->pinIdForIndex(QtNodes::PortType::Out, i);
        if (pinId == QString::fromLatin1(ConditionalRouterNode::kOutputTrueId)) {
            trueOutPortIndex = static_cast<int>(i);
        } else if (pinId == QString::fromLatin1(ConditionalRouterNode::kOutputFalseId)) {
            falseOutPortIndex = static_cast<int>(i);
        }
    }

    ASSERT_GE(inPortIndex, 0);
    ASSERT_GE(condPortIndex, 0);
    ASSERT_GE(trueOutPortIndex, 0);
    ASSERT_GE(falseOutPortIndex, 0);

    // Wire data: data.output(0) -> router.in
    model.addConnection(ConnectionId{ dataNodeId,
                                      0u,
                                      routerNodeId,
                                      static_cast<QtNodes::PortIndex>(inPortIndex) });

    // Wire condition: cond.output(0) -> router.condition
    model.addConnection(ConnectionId{ condNodeId,
                                      0u,
                                      routerNodeId,
                                      static_cast<QtNodes::PortIndex>(condPortIndex) });

    // Wire router true/false outputs to the appropriate TextOutput nodes
    model.addConnection(ConnectionId{ routerNodeId,
                                      static_cast<QtNodes::PortIndex>(trueOutPortIndex),
                                      trueOutNodeId,
                                      0u });
    model.addConnection(ConnectionId{ routerNodeId,
                                      static_cast<QtNodes::PortIndex>(falseOutPortIndex),
                                      falseOutNodeId,
                                      0u });

    // Configure TextInputs
    auto* dataDel = model.delegateModel<ToolNodeDelegate>(dataNodeId);
    ASSERT_NE(dataDel, nullptr);
    auto dataConn = dataDel->connector();
    ASSERT_TRUE(dataConn);
    auto* dataTool = dynamic_cast<TextInputNode*>(dataConn.get());
    ASSERT_NE(dataTool, nullptr);
    dataTool->setText(QStringLiteral("payload"));

    auto* condDel = model.delegateModel<ToolNodeDelegate>(condNodeId);
    ASSERT_NE(condDel, nullptr);
    auto condConn = condDel->connector();
    ASSERT_TRUE(condConn);
    auto* condTool = dynamic_cast<TextInputNode*>(condConn.get());
    ASSERT_NE(condTool, nullptr);
    condTool->setText(QStringLiteral("false"));

    // Router uses the explicit condition pin; defaultCondition is irrelevant
    auto routerConn = routerDel->connector();
    ASSERT_TRUE(routerConn);
    auto* routerTool = dynamic_cast<ConditionalRouterNode*>(routerConn.get());
    ASSERT_NE(routerTool, nullptr);
    Q_UNUSED(routerTool);

    int trueExecCount = 0;
    int falseExecCount = 0;

    ExecutionEngine engine(&model);
    QObject::connect(&engine, &ExecutionEngine::nodeOutputChanged,
                     &engine, [&](NodeId nid) {
        if (nid == trueOutNodeId) {
            ++trueExecCount;
        } else if (nid == falseOutNodeId) {
            ++falseExecCount;
        }
    });

    ASSERT_TRUE(runEngineAndWait(engine));

    // With condition false: only the false branch should execute
    EXPECT_EQ(trueExecCount, 0);
    EXPECT_EQ(falseExecCount, 1);
}
