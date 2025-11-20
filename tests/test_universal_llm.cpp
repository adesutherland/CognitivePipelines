//
// Cognitive Pipeline Application
//
// Universal LLM Node tests
//

#include <gtest/gtest.h>

#include <QApplication>
#include <QString>
#include <QJsonObject>

#include "UniversalLLMNode.h"
#include "core/LLMProviderRegistry.h"

// Minimal app helper in case QObject machinery needs it
static QApplication* ensureApp()
{
    static QApplication* app = nullptr;
    if (!app) {
        static int argc = 1;
        static char appName[] = "unit_tests";
        static char* argv[] = { appName, nullptr };
        app = new QApplication(argc, argv);
    }
    return app;
}

TEST(UniversalLLMNodeTest, OpenAIIntegration)
{
    ensureApp();

    // Try to get API key from environment or accounts.json
    QString apiKey = qEnvironmentVariable("OPENAI_API_KEY");
    if (apiKey.isEmpty()) {
        apiKey = LLMProviderRegistry::instance().getCredential(QStringLiteral("openai"));
    }
    
    if (apiKey.isEmpty()) {
        GTEST_SKIP() << "No OpenAI API key provided. Set OPENAI_API_KEY environment variable or add to accounts.json.";
    }

    auto* node = new UniversalLLMNode();

    // Configure via loadState: provider "openai", model "gpt-5-mini"
    QJsonObject state;
    state.insert(QStringLiteral("provider"), QStringLiteral("openai"));
    state.insert(QStringLiteral("model"), QStringLiteral("gpt-5-mini"));
    state.insert(QStringLiteral("systemPrompt"), QStringLiteral("You are a concise assistant."));
    state.insert(QStringLiteral("temperature"), 1.0); // gpt-5-mini only supports default temperature of 1.0
    node->loadState(state);

    // Prepare input with prompt
    DataPacket inputs;
    inputs.insert(QStringLiteral("prompt"), QStringLiteral("What is the capital of France?"));

    // Execute
    QFuture<DataPacket> future = node->Execute(inputs);
    future.waitForFinished();
    const DataPacket output = future.result();

    // Verify response
    const QString response = output.value(QStringLiteral("response")).toString();
    ASSERT_FALSE(response.isEmpty()) << "Response should not be empty";
    
    // Check for error
    if (output.contains(QStringLiteral("__error"))) {
        const QString error = output.value(QStringLiteral("__error")).toString();
        FAIL() << "LLM request failed with error: " << error.toStdString();
    }

    // Verify usage tokens (should be > 0 for successful request)
    const int totalTokens = output.value(QStringLiteral("_usage.total_tokens")).toInt();
    EXPECT_GT(totalTokens, 0) << "Total tokens should be greater than 0";

    // Basic sanity check: response should mention Paris
    const QString responseLower = response.toLower();
    EXPECT_NE(responseLower.indexOf(QStringLiteral("paris")), -1) 
        << "Response should mention 'Paris'. Response was: " << response.toStdString();

    delete node;
}

TEST(UniversalLLMNodeTest, GoogleIntegration)
{
    ensureApp();

    // Try to get API key from environment or accounts.json
    QString apiKey = qEnvironmentVariable("GOOGLE_API_KEY");
    if (apiKey.isEmpty()) {
        apiKey = LLMProviderRegistry::instance().getCredential(QStringLiteral("google"));
    }
    
    if (apiKey.isEmpty()) {
        GTEST_SKIP() << "No Google API key provided. Set GOOGLE_API_KEY environment variable or add to accounts.json.";
    }

    auto* node = new UniversalLLMNode();

    // Configure via loadState: provider "google", model "gemini-2.5-flash-lite"
    QJsonObject state;
    state.insert(QStringLiteral("provider"), QStringLiteral("google"));
    state.insert(QStringLiteral("model"), QStringLiteral("gemini-2.5-flash-lite"));
    state.insert(QStringLiteral("systemPrompt"), QStringLiteral("You are a concise assistant."));
    node->loadState(state);

    // Prepare input with prompt
    DataPacket inputs;
    inputs.insert(QStringLiteral("prompt"), QStringLiteral("What is the capital of France?"));

    // Execute
    QFuture<DataPacket> future = node->Execute(inputs);
    future.waitForFinished();
    const DataPacket output = future.result();

    // Verify response
    const QString responseRaw = output.value(QStringLiteral("response")).toString();
    const QString response = responseRaw.toLower();
    
    // Check for model availability issues (environment-specific)
    if (response.contains(QStringLiteral("is not found for api version v1")) ||
        response.contains(QStringLiteral("is not supported for generatecontent"))) {
        GTEST_SKIP() << "Google Gemini model is not available for v1/generateContent in this environment. "
                     << "Full response: " << responseRaw.toStdString();
    }
    
    // Check for error
    if (output.contains(QStringLiteral("__error"))) {
        const QString error = output.value(QStringLiteral("__error")).toString();
        FAIL() << "LLM request failed with error: " << error.toStdString();
    }

    // Verify response is not empty
    ASSERT_FALSE(response.isEmpty()) << "Response should not be empty";

    // Verify usage tokens (should be > 0 for successful request)
    const int totalTokens = output.value(QStringLiteral("_usage.total_tokens")).toInt();
    EXPECT_GT(totalTokens, 0) << "Total tokens should be greater than 0";

    // Basic sanity check: response should mention Paris
    EXPECT_NE(response.indexOf(QStringLiteral("paris")), -1) 
        << "Response should mention 'Paris'. Response was: " << responseRaw.toStdString();

    delete node;
}
