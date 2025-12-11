#include <gtest/gtest.h>

#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <QSet>

#include "NodeGraphModel.h"
#include "ExecutionEngine.h"
#include "ToolNodeDelegate.h"
#include "TextInputNode.h"
#include "ExecutionIdUtils.h"
#include "ExecutionState.h"

using namespace QtNodes;

static QApplication* ensureApp2()
{
    static QApplication* app = nullptr;
    if (!app) {
        int argc = 0;
        char* argv[] = { nullptr };
        app = new QApplication(argc, argv);
    }
    return app;
}

TEST(ExecutionControlTest, SelectiveEntryExecution)
{
    ensureApp2();

    NodeGraphModel model;

    // Two disconnected source nodes A and B
    NodeId aId = model.addNode(QStringLiteral("text-input"));
    NodeId bId = model.addNode(QStringLiteral("text-input"));
    ASSERT_NE(aId, InvalidNodeId);
    ASSERT_NE(bId, InvalidNodeId);

    // Assign different texts to distinguish runs (optional)
    if (auto* delA = model.delegateModel<ToolNodeDelegate>(aId)) {
        if (auto conn = delA->connector()) {
            if (auto* ti = dynamic_cast<TextInputNode*>(conn.get())) {
                ti->setText(QStringLiteral("Alpha"));
            }
        }
    }
    if (auto* delB = model.delegateModel<ToolNodeDelegate>(bId)) {
        if (auto conn = delB->connector()) {
            if (auto* ti = dynamic_cast<TextInputNode*>(conn.get())) {
                ti->setText(QStringLiteral("Beta"));
            }
        }
    }

    ExecutionEngine engine(&model);

    const QUuid aUuid = ExecIds::nodeUuid(aId);
    const QUuid bUuid = ExecIds::nodeUuid(bId);

    // Capture which nodes transitioned to Running
    QSet<QUuid> ran;
    QObject::connect(&engine, &ExecutionEngine::nodeStatusChanged, &engine,
                     [&ran](const QUuid& nodeUuid, int state){
        if (state == static_cast<int>(ExecutionState::Running)) ran.insert(nodeUuid);
    });

    // Run only A
    ran.clear();
    engine.runPipeline({ aUuid });

    QEventLoop loop1;
    QTimer t1; t1.setSingleShot(true);
    QObject::connect(&engine, &ExecutionEngine::pipelineFinished, &loop1, &QEventLoop::quit);
    QObject::connect(&t1, &QTimer::timeout, &loop1, &QEventLoop::quit);
    t1.start(5000);
    loop1.exec();

    EXPECT_TRUE(ran.contains(aUuid));
    EXPECT_FALSE(ran.contains(bUuid));

    // Run only B
    ran.clear();
    engine.runPipeline({ bUuid });

    QEventLoop loop2;
    QTimer t2; t2.setSingleShot(true);
    QObject::connect(&engine, &ExecutionEngine::pipelineFinished, &loop2, &QEventLoop::quit);
    QObject::connect(&t2, &QTimer::timeout, &loop2, &QEventLoop::quit);
    t2.start(5000);
    loop2.exec();

    EXPECT_TRUE(ran.contains(bUuid));
    EXPECT_FALSE(ran.contains(aUuid));
}
