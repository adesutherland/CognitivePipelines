#include <QtTest/QtTest>
#include <QApplication>
#include <QSignalSpy>
#include "NodeGraphModel.h"
#include "ExecutionEngine.h"
#include "RetryLoopNode.h"
#include "TextInputNode.h"
#include "ToolNodeDelegate.h"
#include "IToolConnector.h"

using namespace QtNodes;

// Mock worker that fails twice and succeeds on the third attempt
class MockWorkerNode : public QObject, public IToolConnector {
    Q_OBJECT
    Q_INTERFACES(IToolConnector)

public:
    int executionCount = 0;

    NodeDescriptor getDescriptor() const override {
        NodeDescriptor desc;
        desc.id = QStringLiteral("mock-worker");
        desc.name = QStringLiteral("Mock Worker");
        
        PinDefinition in;
        in.direction = PinDirection::Input;
        in.id = QStringLiteral("in");
        in.name = QStringLiteral("In");
        in.type = QStringLiteral("text");
        desc.inputPins.insert(in.id, in);

        PinDefinition out;
        out.direction = PinDirection::Output;
        out.id = QStringLiteral("out");
        out.name = QStringLiteral("Out");
        out.type = QStringLiteral("text");
        desc.outputPins.insert(out.id, out);

        return desc;
    }

    QWidget* createConfigurationWidget(QWidget*) override { return nullptr; }

    TokenList execute(const TokenList& incomingTokens) override {
        // We increment execution count only when we actually process an input
        if (incomingTokens.empty()) return {};

        executionCount++;
        
        QString result = (executionCount < 3) ? QStringLiteral("FAIL") : QStringLiteral("SUCCESS");
        
        TokenList outputs;
        ExecutionToken t;
        // The output pin id must match what's in the descriptor: "out"
        t.data[QStringLiteral("out")] = result;
        t.data[QStringLiteral("text")] = result;
        outputs.push_back(t);
        return outputs;
    }

    QJsonObject saveState() const override { return {}; }
    void loadState(const QJsonObject&) override {}
};

class RetryLoopIntegrationTest : public QObject {
    Q_OBJECT

private slots:
    void test_RetryLoopForcesReexecution() {
        NodeGraphModel model;
        ExecutionEngine engine(&model);

        // Create MockWorkerNode and its delegate
        auto mockWorker = std::make_shared<MockWorkerNode>();
        
        // Register it. ToolNodeDelegate will use mockWorker->getDescriptor().id as the name.
        model.dataModelRegistry()->registerModel([mockWorker]() {
            return std::make_unique<ToolNodeDelegate>(mockWorker);
        }, QStringLiteral("Mocks"));

        // Build Graph
        // TextInput -> RetryLoop (task_in)
        // RetryLoop (worker_instruction) -> MockWorker (in)
        // MockWorker (out) -> RetryLoop (worker_feedback)
        
        NodeId textId = model.addNode(QStringLiteral("text-input"));
        NodeId retryId = model.addNode(QStringLiteral("retry-loop"));
        NodeId mockId = model.addNode(QStringLiteral("mock-worker"));

        QVERIFY(textId != InvalidNodeId);
        QVERIFY(retryId != InvalidNodeId);
        QVERIFY(mockId != InvalidNodeId);

        // Connections:
        // Text output (0) -> Retry task_in (0)
        model.addConnection(ConnectionId{ textId, 0u, retryId, 0u });
        
        // Retry worker_instruction (1) -> Mock in (0)
        model.addConnection(ConnectionId{ retryId, 1u, mockId, 0u });
        
        // Mock out (0) -> Retry worker_feedback (1)
        model.addConnection(ConnectionId{ mockId, 0u, retryId, 1u });

        // Configuration
        {
            auto* del = model.delegateModel<ToolNodeDelegate>(textId);
            auto node = std::dynamic_pointer_cast<TextInputNode>(del->connector());
            node->setText(QStringLiteral("TEST_PAYLOAD"));
        }
        
        {
            auto* retryDel = model.delegateModel<ToolNodeDelegate>(retryId);
            auto retryNode = std::dynamic_pointer_cast<RetryLoopNode>(retryDel->connector());
            retryNode->setFailureString(QStringLiteral("FAIL"));
            retryNode->setMaxRetries(5);
        }

        QSignalSpy spy(&engine, &ExecutionEngine::executionFinished);

        // Start execution
        engine.Run();
        
        // Wait for completion (generous timeout for multiple loops)
        QVERIFY(spy.wait(10000));

        // Assertions
        QCOMPARE(mockWorker->executionCount, 3);
        
        // Verify final output of Retry Node
        DataPacket output = engine.nodeOutput(retryId);
        QCOMPARE(output.value(QString::fromLatin1(RetryLoopNode::kOutputVerifiedResultId)).toString(), QStringLiteral("SUCCESS"));
    }
};

int runRetryLoopIntegrationTest(int argc, char** argv)
{
    RetryLoopIntegrationTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_retry_loop_integration.moc"
