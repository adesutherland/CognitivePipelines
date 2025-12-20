#include <gtest/gtest.h>

#include <QApplication>
#include <QEventLoop>
#include <QTimer>

#include "test_app.h"
#include "NodeGraphModel.h"
#include "ExecutionEngine.h"
#include "ToolNodeDelegate.h"
#include "TextInputNode.h"
#include "UniversalLLMNode.h"
#include "PromptBuilderNode.h"
#include "core/LLMProviderRegistry.h"

using namespace QtNodes;

static QApplication* ensureApp()
{
    return sharedTestApp();
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

TEST(UniversalLLMInvalidModelTest, ProducesErrorAndPreventsStaleOutput)
{
    ensureApp();

    // Skip if no credentials: use the same resolver as the app
    QString apiKey = qEnvironmentVariable("OPENAI_API_KEY");
    if (apiKey.isEmpty()) {
        apiKey = LLMProviderRegistry::instance().getCredential(QStringLiteral("openai"));
    }
    if (apiKey.isEmpty()) {
        GTEST_SKIP() << "OPENAI_API_KEY not set and no accounts.json (standard app config dir) found; skipping invalid-model test.";
    }

    NodeGraphModel model;

    // Build pipeline: TextInput -> UniversalLLM
    const NodeId textNodeId = model.addNode(QStringLiteral("text-input"));
    const NodeId llmNodeId = model.addNode(QStringLiteral("universal-llm"));
    ASSERT_NE(textNodeId, InvalidNodeId);
    ASSERT_NE(llmNodeId, InvalidNodeId);

    model.addConnection(ConnectionId{ textNodeId, 0u, llmNodeId, 1u }); // TextInput output -> LLM prompt input (index 1 -> "prompt", pins sorted alphabetically: image=0, prompt=1, system=2)

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

    // Access UniversalLLMNode
    UniversalLLMNode* llm = nullptr;
    {
        auto* del = model.delegateModel<ToolNodeDelegate>(llmNodeId);
        ASSERT_NE(del, nullptr);
        auto c = del->connector();
        ASSERT_TRUE(c);
        llm = dynamic_cast<UniversalLLMNode*>(c.get());
        ASSERT_NE(llm, nullptr);
    }

    ExecutionEngine engine(&model);

    // First run with a valid model to establish a baseline (may still be error due to network)
    {
        QJsonObject state;
        state.insert(QStringLiteral("provider"), QStringLiteral("openai"));
        state.insert(QStringLiteral("model"), QStringLiteral("gpt-5-mini"));
        state.insert(QStringLiteral("temperature"), 1.0);
        state.insert(QStringLiteral("maxTokens"), 100);
        llm->loadState(state);
    }

    DataPacket out1;
    ASSERT_TRUE(runEngineAndWait(engine, out1, 60000)) << "Engine did not finish for valid model run";
    const QString resp1 = out1.value(QString::fromLatin1(UniversalLLMNode::kOutputResponseId)).toString();

    // Second run with invalid maxTokens (0); should not repeat resp1 and should surface an error message
    {
        QJsonObject state;
        state.insert(QStringLiteral("provider"), QStringLiteral("openai"));
        state.insert(QStringLiteral("model"), QStringLiteral("gpt-5-mini"));
        state.insert(QStringLiteral("temperature"), 1.0);
        state.insert(QStringLiteral("maxTokens"), 0); // Invalid: 0 tokens should trigger API error
        llm->loadState(state);
    }

    DataPacket out2;
    ASSERT_TRUE(runEngineAndWait(engine, out2, 60000)) << "Engine did not finish for invalid model run";
    ASSERT_TRUE(out2.contains(QString::fromLatin1(UniversalLLMNode::kOutputResponseId)));
    const QString resp2 = out2.value(QString::fromLatin1(UniversalLLMNode::kOutputResponseId)).toString();

    // Ensure not stale: response must differ across runs
    EXPECT_NE(resp1, resp2) << "UniversalLLMNode emitted stale output when model was invalid";

    // Check for expected error indicators
    const QString lc = resp2.toLower();
    bool looksLikeApiError = lc.contains("does not exist") || lc.contains("model_not_found") || lc.contains("http 404") || lc.contains("invalid") || lc.contains("error");
    EXPECT_TRUE(looksLikeApiError) << "Unexpected response for invalid model: " << resp2.toStdString();
}

TEST(UniversalLLMInvalidModelTest, StopsPipelineOnError)
{
    ensureApp();

    QString apiKey = qEnvironmentVariable("OPENAI_API_KEY");
    if (apiKey.isEmpty()) {
        apiKey = LLMProviderRegistry::instance().getCredential(QStringLiteral("openai"));
    }
    if (apiKey.isEmpty()) {
        GTEST_SKIP() << "OPENAI_API_KEY not set and no accounts.json (standard app config dir) found; skipping stop-on-error test.";
    }

    NodeGraphModel model;

    // Build pipeline: TextInput -> UniversalLLM -> PromptBuilder
    const NodeId textNodeId = model.addNode(QStringLiteral("text-input"));
    const NodeId llmNodeId = model.addNode(QStringLiteral("universal-llm"));
    const NodeId promptNodeId = model.addNode(QStringLiteral("prompt-builder"));
    ASSERT_NE(textNodeId, InvalidNodeId);
    ASSERT_NE(llmNodeId, InvalidNodeId);
    ASSERT_NE(promptNodeId, InvalidNodeId);

    model.addConnection(ConnectionId{ textNodeId, 0u, llmNodeId, 1u });      // text -> llm (prompt, index 1 after alphabetical sorting)
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
    UniversalLLMNode* llm = nullptr;
    {
        auto* del = model.delegateModel<ToolNodeDelegate>(llmNodeId);
        ASSERT_NE(del, nullptr);
        auto c = del->connector();
        ASSERT_TRUE(c);
        llm = dynamic_cast<UniversalLLMNode*>(c.get());
        ASSERT_NE(llm, nullptr);
        // Configure with valid model but invalid maxTokens (0) to trigger API error
        QJsonObject state;
        state.insert(QStringLiteral("provider"), QStringLiteral("openai"));
        state.insert(QStringLiteral("model"), QStringLiteral("gpt-5-mini"));
        state.insert(QStringLiteral("temperature"), 1.0);
        state.insert(QStringLiteral("maxTokens"), 0); // Invalid: 0 tokens should trigger API error
        llm->loadState(state);
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

    // We expect only Text Input and Universal AI to execute; Prompt Builder should not run
    ASSERT_GE(execMsgs.size(), 2);
    const QString all = execMsgs.join("\n");
    EXPECT_NE(all.indexOf("Text Input"), -1);
    EXPECT_NE(all.indexOf("Universal AI"), -1);
    EXPECT_EQ(all.indexOf("Prompt Builder"), -1) << all.toStdString();

    // Final output should carry the error flag
    const QString err = out.value(QStringLiteral("__error")).toString();
    ASSERT_FALSE(err.trimmed().isEmpty()) << "Pipeline did not propagate error flag";
}
