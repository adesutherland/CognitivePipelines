#include <gtest/gtest.h>

#include <QApplication>
#include <QEventLoop>
#include <QTimer>

#include "NodeGraphModel.h"
#include "ExecutionEngine.h"
#include "ToolNodeDelegate.h"
#include "TextInputNode.h"
#include "LLMConnector.h"
#include "PromptBuilderNode.h"

using namespace QtNodes;

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

static bool runEngineAndWait(ExecutionEngine& engine, DataPacket& finalOut, int timeoutMs = 60000)
{
    bool finished = false;
    QObject::connect(&engine, &ExecutionEngine::pipelineFinished, &engine, [&](const DataPacket& out){
        finished = true;
        finalOut = out;
    });

    engine.run();

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&engine, &ExecutionEngine::pipelineFinished, &loop, &QEventLoop::quit);
    timeout.start(timeoutMs);
    loop.exec();

    return finished;
}

TEST(LLMConnectorInvalidModelTest, ProducesErrorAndPreventsStaleOutput)
{
    ensureApp();

    // Skip if no credentials: use the same resolver as the app
    if (LLMConnector::getApiKey().isEmpty()) {
        GTEST_SKIP() << "OPENAI_API_KEY not set and no accounts.json (standard app config dir) found; skipping invalid-model test.";
    }

    NodeGraphModel model;

    // Build pipeline: TextInput -> LLMConnector
    const NodeId textNodeId = model.addNode(QStringLiteral("text-input"));
    const NodeId llmNodeId = model.addNode(QStringLiteral("llm-connector"));
    ASSERT_NE(textNodeId, InvalidNodeId);
    ASSERT_NE(llmNodeId, InvalidNodeId);

    model.addConnection(ConnectionId{ textNodeId, 0u, llmNodeId, 0u }); // TextInput output -> LLM prompt input (index 0 -> "prompt")

    // Configure TextInput
    {
        auto* del = model.delegateModel<ToolNodeDelegate>(textNodeId);
        ASSERT_NE(del, nullptr);
        auto c = del->connector();
        ASSERT_TRUE(c);
        auto* tool = dynamic_cast<TextInputNode*>(c.get());
        ASSERT_NE(tool, nullptr);
        tool->setText(QStringLiteral("Say hello."));
    }

    // Access LLMConnector
    LLMConnector* llm = nullptr;
    {
        auto* del = model.delegateModel<ToolNodeDelegate>(llmNodeId);
        ASSERT_NE(del, nullptr);
        auto c = del->connector();
        ASSERT_TRUE(c);
        llm = dynamic_cast<LLMConnector*>(c.get());
        ASSERT_NE(llm, nullptr);
    }

    ExecutionEngine engine(&model);

    // First run with a valid model to establish a baseline (may still be error due to network)
    llm->onModelNameChanged(QStringLiteral("gpt-4o-mini"));

    DataPacket out1;
    ASSERT_TRUE(runEngineAndWait(engine, out1, 60000)) << "Engine did not finish for valid model run";
    const QString resp1 = out1.value(QString::fromLatin1(LLMConnector::kOutputResponseId)).toString();

    // Second run with an invalid model; should not repeat resp1 and should surface an error message
    llm->onModelNameChanged(QStringLiteral("gpt-3.5-turboxxx"));

    DataPacket out2;
    ASSERT_TRUE(runEngineAndWait(engine, out2, 60000)) << "Engine did not finish for invalid model run";
    ASSERT_TRUE(out2.contains(QString::fromLatin1(LLMConnector::kOutputResponseId)));
    const QString resp2 = out2.value(QString::fromLatin1(LLMConnector::kOutputResponseId)).toString();

    // Ensure not stale: response must differ across runs
    EXPECT_NE(resp1, resp2) << "LLMConnector emitted stale output when model was invalid";

    // Check for expected error indicators
    const QString lc = resp2.toLower();
    bool looksLikeApiError = lc.contains("does not exist") || lc.contains("model_not_found") || lc.contains("http 404") || lc.contains("invalid") || lc.contains("error");
    EXPECT_TRUE(looksLikeApiError) << "Unexpected response for invalid model: " << resp2.toStdString();
}

TEST(LLMConnectorInvalidModelTest, StopsPipelineOnError)
{
    ensureApp();

    if (LLMConnector::getApiKey().isEmpty()) {
        GTEST_SKIP() << "OPENAI_API_KEY not set and no accounts.json (standard app config dir) found; skipping stop-on-error test.";
    }

    NodeGraphModel model;

    // Build pipeline: TextInput -> LLMConnector -> PromptBuilder
    const NodeId textNodeId = model.addNode(QStringLiteral("text-input"));
    const NodeId llmNodeId = model.addNode(QStringLiteral("llm-connector"));
    const NodeId promptNodeId = model.addNode(QStringLiteral("prompt-builder"));
    ASSERT_NE(textNodeId, InvalidNodeId);
    ASSERT_NE(llmNodeId, InvalidNodeId);
    ASSERT_NE(promptNodeId, InvalidNodeId);

    model.addConnection(ConnectionId{ textNodeId, 0u, llmNodeId, 0u });      // text -> llm (prompt)
    model.addConnection(ConnectionId{ llmNodeId, 0u, promptNodeId, 0u });    // llm.response -> prompt.input

    // Configure nodes
    {
        auto* del = model.delegateModel<ToolNodeDelegate>(textNodeId);
        ASSERT_NE(del, nullptr);
        auto c = del->connector();
        ASSERT_TRUE(c);
        auto* tool = dynamic_cast<TextInputNode*>(c.get());
        ASSERT_NE(tool, nullptr);
        tool->setText(QStringLiteral("Hello"));
    }
    LLMConnector* llm = nullptr;
    {
        auto* del = model.delegateModel<ToolNodeDelegate>(llmNodeId);
        ASSERT_NE(del, nullptr);
        auto c = del->connector();
        ASSERT_TRUE(c);
        llm = dynamic_cast<LLMConnector*>(c.get());
        ASSERT_NE(llm, nullptr);
        llm->onModelNameChanged(QStringLiteral("gpt-3.5-turboxxx")); // invalid
    }
    {
        auto* del = model.delegateModel<ToolNodeDelegate>(promptNodeId);
        ASSERT_NE(del, nullptr);
        auto c = del->connector();
        ASSERT_TRUE(c);
        auto* tool = dynamic_cast<PromptBuilderNode*>(c.get());
        ASSERT_NE(tool, nullptr);
        tool->setTemplateText(QStringLiteral("AFTER: {input}"));
    }

    ExecutionEngine engine(&model);

    // Capture execution order
    QStringList execMsgs;
    QObject::connect(&engine, &ExecutionEngine::nodeLog, &engine, [&](const QString& msg){
        if (msg.startsWith(QLatin1String("Executing Node:"))) execMsgs << msg;
    });

    DataPacket out;
    ASSERT_TRUE(runEngineAndWait(engine, out, 60000));

    // We expect only Text Input and LLM Connector to execute; Prompt Builder should not run
    ASSERT_GE(execMsgs.size(), 2);
    const QString all = execMsgs.join("\n");
    EXPECT_NE(all.indexOf("Text Input"), -1);
    EXPECT_NE(all.indexOf("LLM Connector"), -1);
    EXPECT_EQ(all.indexOf("Prompt Builder"), -1) << all.toStdString();

    // Final output should carry the error flag
    const QString err = out.value(QStringLiteral("__error")).toString();
    ASSERT_FALSE(err.trimmed().isEmpty()) << "Pipeline did not propagate error flag";
}
