//
// Cognitive Pipeline Application - TDAD Phase 2
//
// Failing integration tests to reproduce:
// 1) Model Auto-Recovery (selected model should pass through unchanged)
// 2) Parameter Enforcement (temperature must be omitted for models that don't support it)
//

#include <gtest/gtest.h>

#include <QApplication>
#include <QJsonObject>
#include <QJsonArray>
#include <QFuture>
#include <QtConcurrent>
#include <QByteArray>
#include <QDir>
#include <QEventLoop>
#include <QTimer>

#include "UniversalLLMNode.h"
#include "NodeGraphModel.h"
#include "ExecutionEngine.h"
#include "ToolNodeDelegate.h"
#include "TextInputNode.h"
#include "ai/registry/LLMProviderRegistry.h"
#include "ai/backends/ILLMBackend.h"
#include "ModelCapsRegistry.h"
#include "ModelCaps.h"

using namespace QtNodes;

// Ensure a minimal QApplication exists (mirrors tests/test_universal_llm.cpp style)
static QApplication* ensureApp()
{
    static int argc = 0;
    static char* argv[] = { nullptr };
    static QApplication* app = nullptr;
    if (!app) {
        app = new QApplication(argc, argv);
    }
    return app;
}

// A capturing backend that replaces the OpenAI backend in the registry for tests.
// It intentionally omits the selected model (e.g., "gpt-5.2") from availableModels()
// to trigger the current auto-recovery behavior in UniversalLLMNode, so the test can
// assert the desired behavior and FAIL under the current implementation.
class CapturingBackend final : public ILLMBackend {
public:
    QString id() const override { return QStringLiteral("openai"); }
    QString name() const override { return QStringLiteral("OpenAI (Capturing Test Backend)"); }

    QStringList availableModels() const override {
        // Intentionally omit "gpt-5.2" to provoke fallback; include a common default first.
        return { QStringLiteral("gpt-5.1"), QStringLiteral("gpt-5-pro"), QStringLiteral("gpt-5-mini") };
    }

    QStringList availableEmbeddingModels() const override {
        return { QStringLiteral("text-embedding-3-small") };
    }

    QFuture<QStringList> fetchModelList() override {
        // Return a deterministic list quickly (no network)
        return QtConcurrent::run([list = availableModels()]() { return list; });
    }

    LLMResult sendPrompt(
        const QString& /*apiKey*/,
        const QString& modelName,
        double temperature,
        int maxTokens,
        const QString& systemPrompt,
        const QString& userPrompt,
        const LLMMessage& message = {}
    ) override {
        captured_model = modelName;
        captured_system_prompt = systemPrompt;
        captured_user_prompt = userPrompt;
        captured_attachment_count = message.attachments.size();

        // Build an OpenAI-like payload and capture it, mirroring capability-aware filtering
        const auto capsOpt = ModelCapsRegistry::instance().resolve(modelName, QStringLiteral("openai"));
        const bool isReasoning = capsOpt.has_value() && capsOpt->hasCapability(ModelCapsTypes::Capability::Reasoning);
        const bool temperatureSupported = capsOpt.has_value() && capsOpt->constraints.temperature.has_value() && !isReasoning;

        QJsonObject sysMsg; sysMsg.insert(QStringLiteral("role"), QStringLiteral("system")); sysMsg.insert(QStringLiteral("content"), systemPrompt);
        QJsonObject userMsg; userMsg.insert(QStringLiteral("role"), QStringLiteral("user")); userMsg.insert(QStringLiteral("content"), userPrompt);
        QJsonArray messages; messages.append(sysMsg); messages.append(userMsg);

        QJsonObject root;
        root.insert(QStringLiteral("model"), modelName);
        root.insert(QStringLiteral("messages"), messages);
        if (temperatureSupported) {
            root.insert(QStringLiteral("temperature"), temperature);
        }
        root.insert(QStringLiteral("max_tokens"), maxTokens);
        if (!message.attachments.isEmpty()) {
            root.insert(QStringLiteral("_attachmentCount"), message.attachments.size());
        }
        captured_payload = root;

        // Capture the final URL used based on resolved endpoint mode to mirror routing behavior
        ModelCapsTypes::EndpointMode endpointMode = ModelCapsTypes::EndpointMode::Chat;
        if (capsOpt.has_value()) {
            endpointMode = capsOpt->endpointMode;
        }
        if (endpointMode == ModelCapsTypes::EndpointMode::Completion) {
            captured_url = QStringLiteral("https://api.openai.com/v1/completions");
        } else if (endpointMode == ModelCapsTypes::EndpointMode::Assistant) {
            captured_url = QStringLiteral("https://api.openai.com/v1/assistants");
        } else {
            captured_url = QStringLiteral("https://api.openai.com/v1/chat/completions");
        }

        LLMResult out;
        out.content = QStringLiteral("ok");
        out.usage.inputTokens = 5;
        out.usage.outputTokens = 5;
        out.usage.totalTokens = 10;
        out.rawResponse = QStringLiteral("{\"ok\":true}");
        out.hasError = false;
        return out;
    }

    EmbeddingResult getEmbedding(
        const QString& /*apiKey*/,
        const QString& /*modelName*/,
        const QString& /*text*/
    ) override {
        EmbeddingResult r; r.vector = {0.f, 1.f}; r.usage.totalTokens = 2; return r;
    }

    QFuture<QString> generateImage(
        const QString& /*prompt*/,
        const QString& /*model*/,
        const QString& /*size*/,
        const QString& /*quality*/,
        const QString& /*style*/,
        const QString& /*targetDir*/ = QString()
    ) override {
        return QtConcurrent::run([](){ return QDir::tempPath() + QStringLiteral("/dummy.png"); });
    }

    QString captured_model;
    QString captured_system_prompt;
    QString captured_user_prompt;
    qsizetype captured_attachment_count = 0;
    QJsonObject captured_payload;
    QString captured_url;
};

// Helper to install the capturing backend into the registry (it replaces the default OpenAI backend)
static std::shared_ptr<CapturingBackend> installCapturingOpenAI()
{
    auto backend = std::make_shared<CapturingBackend>();
    LLMProviderRegistry::instance().registerBackend(backend); // replaces id "openai"
    return backend;
}

// Helper to ensure credentials are present (node checks registry for API key)
static void ensureDummyOpenAIKey()
{
    qputenv("OPENAI_API_KEY", QByteArray("DUMMY_KEY_FOR_TESTS"));
}

static bool runPipelineAndWait(ExecutionEngine& engine, DataPacket& finalOut, int timeoutMs = 5000)
{
    bool finished = false;
    QObject::connect(&engine, &ExecutionEngine::pipelineFinished, &engine, [&](const DataPacket& out) {
        finished = true;
        finalOut = out;
    });

    engine.Run();

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&engine, &ExecutionEngine::pipelineFinished, &loop, &QEventLoop::quit);
    timeout.start(timeoutMs);
    loop.exec();

    return finished;
}

TEST(UniversalLLMPipelineWiringTest, TextInputFirstPortArrivesAsUserPrompt)
{
    ensureApp();
    ensureDummyOpenAIKey();

    auto capturing = installCapturingOpenAI();

    NodeGraphModel model;
    const NodeId textNodeId = model.addNode(QStringLiteral("text-input"));
    const NodeId llmNodeId = model.addNode(QStringLiteral("universal-llm"));
    ASSERT_NE(textNodeId, InvalidNodeId);
    ASSERT_NE(llmNodeId, InvalidNodeId);

    auto* llmDel = model.delegateModel<ToolNodeDelegate>(llmNodeId);
    ASSERT_NE(llmDel, nullptr);
    ASSERT_EQ(llmDel->pinIdForIndex(PortType::In, 0u),
              QString::fromLatin1(UniversalLLMNode::kInputPromptId));

    model.addConnection(ConnectionId{ textNodeId, 0u, llmNodeId, 0u });

    const QString prompt = QStringLiteral("Text input should become the LLM prompt");
    {
        auto* textDel = model.delegateModel<ToolNodeDelegate>(textNodeId);
        ASSERT_NE(textDel, nullptr);
        auto textNode = textDel->node();
        ASSERT_TRUE(textNode);
        auto* textInput = dynamic_cast<TextInputNode*>(textNode.get());
        ASSERT_NE(textInput, nullptr);
        textInput->setText(prompt);
    }
    {
        auto llmNode = llmDel->node();
        ASSERT_TRUE(llmNode);
        auto* llm = dynamic_cast<UniversalLLMNode*>(llmNode.get());
        ASSERT_NE(llm, nullptr);
        llm->onProviderChanged(QStringLiteral("openai"));
        llm->onModelChanged(QStringLiteral("gpt-4o-mini"));
    }

    ExecutionEngine engine(&model);
    DataPacket out;
    ASSERT_TRUE(runPipelineAndWait(engine, out));

    EXPECT_EQ(capturing->captured_user_prompt, prompt);
    EXPECT_TRUE(capturing->captured_system_prompt.trimmed().isEmpty());
    EXPECT_EQ(capturing->captured_attachment_count, 0);
    EXPECT_EQ(out.value(QString::fromLatin1(UniversalLLMNode::kOutputResponseId)).toString(),
              QStringLiteral("ok"));
}

// --- Test Case 1: Integrity of Selection ---
// Selected model must be passed through unchanged. This test encodes the desired behavior
// and is expected to FAIL under the current implementation (auto-recovery occurs).
TEST(ModelSelectionIntegrationTest, IntegrityOfSelection_ShouldNotAutoRecover)
{
    ensureApp();
    ensureDummyOpenAIKey();

    auto capturing = installCapturingOpenAI();

    UniversalLLMNode node;
    // Configure provider/model via slots (simulating PropertiesWidget signals)
    node.onProviderChanged(QStringLiteral("openai"));
    const QString selectedModel = QStringLiteral("gpt-5.2"); // not in availableModels() above
    node.onModelChanged(selectedModel);

    // Provide a minimal prompt so execute() proceeds
    DataPacket inputs; inputs.insert(QString::fromLatin1(UniversalLLMNode::kInputPromptId), QStringLiteral("ping"));
    ExecutionToken t; t.data = inputs; TokenList in; in.push_back(t);

    const TokenList out = node.execute(in);
    ASSERT_FALSE(out.empty());

    // Desired behavior: backend receives exactly the selected model.
    // Current behavior: node auto-recovers to availableModels().first() -> EXPECTED TO FAIL
    EXPECT_EQ(capturing->captured_model, selectedModel) <<
        "Node should pass the selected model through unchanged (no auto-recovery).";
}

// --- Test Case 2: Parameter Filtering ---
// For models that do not support temperature, the request must omit the temperature key.
// This test encodes the desired behavior and is expected to FAIL because the backend
// currently includes temperature unconditionally.
TEST(ModelSelectionIntegrationTest, ParameterFiltering_TemperatureMustBeOmittedWhenUnsupported)
{
    ensureApp();
    ensureDummyOpenAIKey();

    auto capturing = installCapturingOpenAI();

    UniversalLLMNode node;
    node.onProviderChanged(QStringLiteral("openai"));
    // Use a model (e.g., GPT-5 series) that our caps typically mark as no-temperature or fixed value
    const QString tempUnsupportedModel = QStringLiteral("gpt-5.2");
    node.onModelChanged(tempUnsupportedModel);

    // Set a non-default temperature on the node to surface the bug if it's not filtered
    node.onTemperatureChanged(0.42);

    // Provide a minimal prompt
    DataPacket inputs; inputs.insert(QString::fromLatin1(UniversalLLMNode::kInputPromptId), QStringLiteral("ping"));
    ExecutionToken t; t.data = inputs; TokenList in; in.push_back(t);

    const TokenList out = node.execute(in);
    ASSERT_FALSE(out.empty());

    // Desired behavior: temperature key should be ABSENT for temp-unsupported models.
    // Current behavior: always present -> EXPECTED TO FAIL
    EXPECT_FALSE(capturing->captured_payload.contains(QStringLiteral("temperature"))) <<
        "Backend request must omit 'temperature' for models without temperature support.";
}

// --- Test Case 3: Backend URL Routing (Failing) ---
// When the registry resolves a model to EndpointMode::Completion, the backend must route to
// /v1/completions. This test encodes that contract and is expected to FAIL until routing is wired.
static bool writeRulesToTempFile(QTemporaryFile& file, const QJsonArray& rules)
{
    if (!file.open()) return false;
    const QJsonObject root { { QStringLiteral("rules"), rules } };
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
    const auto written = file.write(payload);
    file.flush();
    file.close();
    return written == payload.size();
}

TEST(ModelSelectionIntegrationTest, BackendURLRouting_CompletionEndpointShouldUseCompletionsPath)
{
    ensureApp();
    ensureDummyOpenAIKey();

    // Install capturing backend and load a rule that declares endpoint="completion"
    auto capturing = installCapturingOpenAI();

    QJsonArray rules;
    rules.append(QJsonObject{
        { QStringLiteral("id"), QStringLiteral("openai-legacy-completion") },
        { QStringLiteral("pattern"), QStringLiteral("^gpt-5\\.2-pro$") },
        { QStringLiteral("backend"), QStringLiteral("openai") },
        { QStringLiteral("endpoint"), QStringLiteral("completion") },
        { QStringLiteral("priority"), 100 }
    });

    QTemporaryFile file;
    ASSERT_TRUE(writeRulesToTempFile(file, rules));
    ASSERT_TRUE(ModelCapsRegistry::instance().loadFromFile(file.fileName()));

    UniversalLLMNode node;
    node.onProviderChanged(QStringLiteral("openai"));
    const QString model = QStringLiteral("gpt-5.2-pro");
    node.onModelChanged(model);

    DataPacket inputs; inputs.insert(QString::fromLatin1(UniversalLLMNode::kInputPromptId), QStringLiteral("ping"));
    ExecutionToken t; t.data = inputs; TokenList in; in.push_back(t);

    const TokenList out = node.execute(in);
    ASSERT_FALSE(out.empty());

    // Desired: /v1/completions. Current captured_url mirrors chat/completions -> EXPECTED TO FAIL
    ASSERT_FALSE(capturing->captured_url.isEmpty());
    EXPECT_NE(capturing->captured_url.indexOf(QStringLiteral("/v1/completions")), -1)
        << "Routing should use /v1/completions for endpoint=completion, but backend used: "
        << capturing->captured_url.toStdString();
}
