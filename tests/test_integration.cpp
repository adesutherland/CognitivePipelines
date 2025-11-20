#include <QtTest/QtTest>
#include <QApplication>
#include <QTemporaryFile>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QTimer>
#include <QtNodes/internal/Definitions.hpp>

#include <stdio.h> // For fprintf, stderr/stdout redirection
#include <iostream>
#if defined(_WIN32)
 #include <io.h> // _dup2, _fileno
#else
 #include <unistd.h> // dup2, fileno
#endif

#include "mainwindow.h"
#include "NodeGraphModel.h"
#include "ExecutionEngine.h"
#include "ToolNodeDelegate.h"
#include "TextInputNode.h"
#include "PromptBuilderNode.h"
#include "UniversalLLMNode.h"
#include "core/LLMProviderRegistry.h"

using namespace QtNodes;

class IntegrationTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void test_SaveLoad();
    void test_FullPipelineExecution();

private:
    std::unique_ptr<MainWindow> mainWindow_;
    NodeGraphModel* model_ {nullptr};
    ExecutionEngine* engine_ {nullptr};
};

void IntegrationTests::initTestCase()
{
    // Instantiate the entire application stack (headless)
    mainWindow_ = std::make_unique<MainWindow>();
    model_ = mainWindow_->graphModel();
    engine_ = mainWindow_->executionEngine();

    QVERIFY(model_ != nullptr);
    QVERIFY(engine_ != nullptr);
}

void IntegrationTests::test_SaveLoad()
{
    QVERIFY(model_ != nullptr);

    // Build: add a TextInput node and set its state
    const NodeId textNodeId = model_->addNode(QStringLiteral("text-input"));
    QVERIFY(textNodeId != InvalidNodeId);

    // Configure via loadState on the underlying connector
    auto* textDel = model_->delegateModel<ToolNodeDelegate>(textNodeId);
    QVERIFY(textDel != nullptr);
    auto conn = textDel->connector();
    QVERIFY(static_cast<bool>(conn));
    auto* textTool = dynamic_cast<TextInputNode*>(conn.get());
    QVERIFY(textTool != nullptr);

    const QString kValue = QStringLiteral("Hello SaveLoad");
    {
        QJsonObject state;
        state.insert(QStringLiteral("text"), kValue);
        textTool->loadState(state);
    }

    // Save model to a temporary file
    QTemporaryFile tmpFile;
    QVERIFY(tmpFile.open());
    const QJsonObject json = model_->save();
    const QByteArray bytes = QJsonDocument(json).toJson();
    QVERIFY(tmpFile.write(bytes) == bytes.size());
    tmpFile.flush();

    // Clear model and load back
    model_->clear();
    QVERIFY(model_->allNodeIds().empty());

    tmpFile.seek(0);
    const QByteArray reBytes = tmpFile.readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(reBytes);
    QVERIFY(doc.isObject());
    model_->load(doc.object());

    // Verify restored nodes and properties
    const auto ids = model_->allNodeIds();
    QCOMPARE(static_cast<int>(ids.size()), 1);
    NodeId restoredId = *ids.begin();

    auto* restoredDel = model_->delegateModel<ToolNodeDelegate>(restoredId);
    QVERIFY(restoredDel != nullptr);
    auto restoredConn = restoredDel->connector();
    QVERIFY(static_cast<bool>(restoredConn));
    auto* restoredText = dynamic_cast<TextInputNode*>(restoredConn.get());
    QVERIFY(restoredText != nullptr);
    QCOMPARE(restoredText->text(), kValue);
}

void IntegrationTests::test_FullPipelineExecution()
{
    QString apiKey = qEnvironmentVariable("OPENAI_API_KEY");
    if (apiKey.isEmpty()) {
        apiKey = LLMProviderRegistry::instance().getCredential(QStringLiteral("openai"));
    }
    if (apiKey.isEmpty()) {
        QSKIP("Neither OPENAI_API_KEY is set nor accounts.json (standard app config dir) found; skipping live integration test.");
    }

    QVERIFY(model_ != nullptr);
    QVERIFY(engine_ != nullptr);

    // Ensure a clean model
    model_->clear();

    // Build pipeline: TextInput -> PromptBuilder -> UniversalLLM
    const NodeId textNodeId = model_->addNode(QStringLiteral("text-input"));
    const NodeId promptNodeId = model_->addNode(QStringLiteral("prompt-builder"));
    const NodeId llmNodeId = model_->addNode(QStringLiteral("universal-llm"));

    QVERIFY(textNodeId != InvalidNodeId);
    QVERIFY(promptNodeId != InvalidNodeId);
    QVERIFY(llmNodeId != InvalidNodeId);

    // Connect ports: output(0) -> input(0)
    model_->addConnection(ConnectionId{ textNodeId, 0u, promptNodeId, 0u });
    model_->addConnection(ConnectionId{ promptNodeId, 0u, llmNodeId, 0u });

    // Configure nodes
    {
        auto* del = model_->delegateModel<ToolNodeDelegate>(textNodeId);
        QVERIFY(del != nullptr);
        auto c = del->connector();
        auto* tool = dynamic_cast<TextInputNode*>(c.get());
        QVERIFY(tool != nullptr);
        tool->setText(QStringLiteral("Say hello to Alice."));
    }
    {
        auto* del = model_->delegateModel<ToolNodeDelegate>(promptNodeId);
        QVERIFY(del != nullptr);
        auto c = del->connector();
        auto* tool = dynamic_cast<PromptBuilderNode*>(c.get());
        QVERIFY(tool != nullptr);
        tool->setTemplateText(QStringLiteral("Instruction: {input}"));
    }
    {
        auto* del = model_->delegateModel<ToolNodeDelegate>(llmNodeId);
        QVERIFY(del != nullptr);
        auto c = del->connector();
        auto* tool = dynamic_cast<UniversalLLMNode*>(c.get());
        QVERIFY(tool != nullptr);
        // Configure via loadState: set provider to "openai" and model to "gpt-5-mini"
        QJsonObject state;
        state.insert(QStringLiteral("provider"), QStringLiteral("openai"));
        state.insert(QStringLiteral("model"), QStringLiteral("gpt-5-mini"));
        tool->loadState(state);
    }

    // Run and wait for completion
    bool finished = false;
    DataPacket finalOut;

    QObject::connect(engine_, &ExecutionEngine::pipelineFinished, this, [&](const DataPacket& out){
        finished = true;
        finalOut = out;
    });

    engine_->run();

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(engine_, &ExecutionEngine::pipelineFinished, &loop, &QEventLoop::quit);
    timeout.start(60000); // up to 60s
    loop.exec();

    QVERIFY(finished);
    QVERIFY(finalOut.contains(QString::fromLatin1(UniversalLLMNode::kOutputResponseId)));
    const QString response = finalOut.value(QString::fromLatin1(UniversalLLMNode::kOutputResponseId)).toString();
    QVERIFY(!response.trimmed().isEmpty());
}

// Force everything written to stdout to be redirected to stderr (so CI captures all test output)
static void redirectStdoutToStderr()
{
#if defined(_WIN32)
    _dup2(_fileno(stderr), _fileno(stdout));
#else
    dup2(fileno(stderr), fileno(stdout));
#endif
    setvbuf(stdout, nullptr, _IONBF, 0);
}

// Custom handler to force all Qt output to stderr
void ciMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(type);
    Q_UNUSED(context);
    // Force output to stderr, which CI runners will capture
    fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
    fflush(stderr); // Ensure it's written immediately
}

// Custom main to ensure QApplication is created and logging is captured in CI
int main(int argc, char** argv)
{
    redirectStdoutToStderr(); // Route QTest/stdout to stderr
    qInstallMessageHandler(ciMessageHandler); // Install handler before QApplication

    QApplication app(argc, argv);
    IntegrationTests tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_integration.moc"
