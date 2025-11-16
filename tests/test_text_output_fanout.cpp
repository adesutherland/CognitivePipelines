//
// Cognitive Pipeline Application - Unit Test for TextOutput fan-out
//
// Verifies that when a single TextInput feeds two TextOutput nodes, both
// receive and display the value on the first run. Also checks behavior when
// the second widget is created after execution (cached value applied).
//

#include <gtest/gtest.h>

#include <QApplication>
#include <QEventLoop>
#include <QTextEdit>
#include <QTimer>

#include "NodeGraphModel.h"
#include "ExecutionEngine.h"
#include "ToolNodeDelegate.h"
#include "TextInputNode.h"
#include "TextOutputNode.h"
#include "TextOutputPropertiesWidget.h"

using namespace QtNodes;

static QApplication* ensureApp_TextOutputFanout()
{
    static QApplication* app = nullptr;
    if (!app) {
        int argc = 0;
        char* argv[] = { nullptr };
        app = new QApplication(argc, argv);
    }
    return app;
}

// Helper: run engine and wait up to timeout
static bool runEngineAndWait(ExecutionEngine& engine, int timeoutMs = 5000)
{
    bool finished = false;
    QObject::connect(&engine, &ExecutionEngine::pipelineFinished, &engine, [&](const DataPacket&) {
        finished = true;
    });
    engine.run();
    QEventLoop loop;
    QTimer timeout; timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&engine, &ExecutionEngine::pipelineFinished, &loop, &QEventLoop::quit);
    timeout.start(timeoutMs);
    loop.exec();
    return finished;
}

TEST(TextOutputFanoutTest, BothOutputsUpdateOnFirstRun)
{
    ensureApp_TextOutputFanout();

    NodeGraphModel model;

    // Nodes: one Text Input, two Text Outputs
    NodeId inputId = model.addNode(QStringLiteral("text-input"));
    NodeId outAId = model.addNode(QStringLiteral("text-output"));
    NodeId outBId = model.addNode(QStringLiteral("text-output"));

    ASSERT_NE(inputId, InvalidNodeId);
    ASSERT_NE(outAId, InvalidNodeId);
    ASSERT_NE(outBId, InvalidNodeId);

    // Connect: input -> outA, input -> outB (fan-out)
    model.addConnection(ConnectionId{ inputId, 0u, outAId, 0u });
    model.addConnection(ConnectionId{ inputId, 0u, outBId, 0u });

    // Configure input text
    auto* inDel = model.delegateModel<ToolNodeDelegate>(inputId);
    ASSERT_NE(inDel, nullptr);
    auto inConn = inDel->connector();
    ASSERT_TRUE(inConn);
    auto* inputTool = dynamic_cast<TextInputNode*>(inConn.get());
    ASSERT_NE(inputTool, nullptr);
    const QString kText = QStringLiteral("Hello fan-out");
    inputTool->setText(kText);

    // Create properties widgets for both outputs BEFORE run
    auto* outADel = model.delegateModel<ToolNodeDelegate>(outAId);
    auto outAConn = outADel->connector();
    auto* outATool = dynamic_cast<TextOutputNode*>(outAConn.get());
    ASSERT_NE(outATool, nullptr);
    QWidget* wA = outATool->createConfigurationWidget(nullptr);
    ASSERT_NE(wA, nullptr);

    auto* outBDel = model.delegateModel<ToolNodeDelegate>(outBId);
    auto outBConn = outBDel->connector();
    auto* outBTool = dynamic_cast<TextOutputNode*>(outBConn.get());
    ASSERT_NE(outBTool, nullptr);
    QWidget* wB = outBTool->createConfigurationWidget(nullptr);
    ASSERT_NE(wB, nullptr);

    // Run
    ExecutionEngine engine(&model);
    ASSERT_TRUE(runEngineAndWait(engine));

    // Verify both widgets show the text
    auto* editA = wA->findChild<QTextEdit*>();
    auto* editB = wB->findChild<QTextEdit*>();
    ASSERT_NE(editA, nullptr);
    ASSERT_NE(editB, nullptr);
    EXPECT_EQ(editA->toPlainText(), kText);
    EXPECT_EQ(editB->toPlainText(), kText);

    delete wA;
    delete wB;
}

TEST(TextOutputFanoutTest, SecondWidgetCreatedAfterRunShowsCachedText)
{
    ensureApp_TextOutputFanout();

    NodeGraphModel model;

    NodeId inputId = model.addNode(QStringLiteral("text-input"));
    NodeId outAId = model.addNode(QStringLiteral("text-output"));
    NodeId outBId = model.addNode(QStringLiteral("text-output"));

    model.addConnection(ConnectionId{ inputId, 0u, outAId, 0u });
    model.addConnection(ConnectionId{ inputId, 0u, outBId, 0u });

    auto* inDel = model.delegateModel<ToolNodeDelegate>(inputId);
    auto inConn = inDel->connector();
    auto* inputTool = dynamic_cast<TextInputNode*>(inConn.get());
    const QString kText = QStringLiteral("First run value");
    inputTool->setText(kText);

    // Create only the first output's widget before run
    auto* outADel = model.delegateModel<ToolNodeDelegate>(outAId);
    auto outAConn = outADel->connector();
    auto* outATool = dynamic_cast<TextOutputNode*>(outAConn.get());
    QWidget* wA = outATool->createConfigurationWidget(nullptr);
    ASSERT_NE(wA, nullptr);

    // Do not create widget B yet
    auto* outBDel = model.delegateModel<ToolNodeDelegate>(outBId);
    auto outBConn = outBDel->connector();
    auto* outBTool = dynamic_cast<TextOutputNode*>(outBConn.get());
    ASSERT_NE(outBTool, nullptr);

    // Run
    ExecutionEngine engine(&model);
    ASSERT_TRUE(runEngineAndWait(engine));

    // Verify A shows text
    auto* editA = wA->findChild<QTextEdit*>();
    ASSERT_NE(editA, nullptr);
    EXPECT_EQ(editA->toPlainText(), kText);

    // Now create widget B AFTER run; it should adopt the cached last text
    QWidget* wB = outBTool->createConfigurationWidget(nullptr);
    ASSERT_NE(wB, nullptr);
    auto* editB = wB->findChild<QTextEdit*>();
    ASSERT_NE(editB, nullptr);
    EXPECT_EQ(editB->toPlainText(), kText);

    delete wA;
    delete wB;
}
