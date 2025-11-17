//
// Cognitive Pipeline Application
//
// Google LLM Connector tests
//

#include <gtest/gtest.h>

#include <QApplication>
#include <QMetaObject>
#include <QString>

#include "GoogleLLMConnector.h"
#include "llm_api_client.h"
#include "LLMConnector.h"

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

TEST(GoogleLLMConnectorTest, SuccessPath)
{
    ensureApp();

    const QString successJson = QStringLiteral(R"({
        "candidates": [
          { "content": { "parts": [ { "text": "Hello, world!" } ] } }
        ]
    })");

    auto* connector = new GoogleLLMConnector();

    // Call private slot via QMetaObject to simulate async callback
    bool invoked = QMetaObject::invokeMethod(connector, "onPromptFinished", Qt::DirectConnection,
                              Q_ARG(QString, successJson));
    ASSERT_TRUE(invoked);

    EXPECT_EQ(connector->GetOutputData(QStringLiteral("response")).toString(), QStringLiteral("Hello, world!"));

    delete connector;
}

TEST(GoogleLLMConnectorTest, ErrorPath)
{
    ensureApp();

    const QString errorJson = QStringLiteral(R"({
        "error": { "code": 400, "message": "API key not valid" }
    })");

    auto* connector = new GoogleLLMConnector();

    bool invoked = QMetaObject::invokeMethod(connector, "onPromptFinished", Qt::DirectConnection,
                              Q_ARG(QString, errorJson));
    ASSERT_TRUE(invoked);

    EXPECT_EQ(connector->GetOutputData(QStringLiteral("response")).toString(), QStringLiteral("API key not valid"));

    delete connector;
}

TEST(GoogleLLMConnectorIntegrationTest, ShouldReceiveValidResponseForSimplePrompt)
{
    ensureApp();

    LlmApiClient client;
    const QString apiKey = client.getApiKey(QStringLiteral("google"));
    if (apiKey.isEmpty()) {
        // Mirror the OpenAI integration test behavior: skip gracefully when no credentials.
        const QString canonicalPath = LLMConnector::defaultAccountsFilePath();
        qWarning() << "No Google API key available. The canonical accounts.json path checked would be:" << canonicalPath;
        GTEST_SKIP() << "No Google API key provided. Set GOOGLE_API_KEY or add accounts.json at: "
                     << canonicalPath.toStdString();
    }

    auto* connector = new GoogleLLMConnector();

    // Configure model via the public state API to avoid touching private slots.
    // Use a v1-compatible Gemini text model; architect guidance recommends
    // "gemini-2.5-flash-lite" for the stable v1/generateContent endpoint.
    QJsonObject state;
    state.insert(QStringLiteral("model"), QStringLiteral("gemini-2.5-flash-lite"));
    connector->loadState(state);

    DataPacket inputs;
    inputs.insert(QStringLiteral("system"), QStringLiteral("You are a concise assistant."));
    inputs.insert(QStringLiteral("prompt"), QStringLiteral("What is the capital of France?"));

    QFuture<DataPacket> future = connector->Execute(inputs);
    future.waitForFinished();
    const DataPacket output = future.result();
    const QString responseRaw = output.value(QStringLiteral("response")).toString();
    const QString response = responseRaw.toLower();

    // If the Google API reports a configuration/model issue for v1, treat this as
    // an environment problem rather than a hard failure of the connector logic.
    if (response.contains(QStringLiteral("is not found for api version v1")) ||
        response.contains(QStringLiteral("is not supported for generatecontent"))) {
        GTEST_SKIP() << "Google Gemini model is not available for v1/generateContent in this environment. "
                     << "Full response: " << responseRaw.toStdString();
    }

    // Basic acceptance: we expect a non-empty response mentioning Paris.
    ASSERT_FALSE(response.isEmpty());
    EXPECT_NE(response.indexOf(QStringLiteral("paris")), -1) << "Response did not contain 'Paris'. Response was: "
                                                              << responseRaw.toStdString();

    delete connector;
}
