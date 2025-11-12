#include <gtest/gtest.h>

#include <QApplication>
#include <QMetaObject>
#include <QString>

#include "GoogleLLMConnector.h"

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
