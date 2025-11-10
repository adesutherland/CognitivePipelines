//
// Qt Test for GoogleLLMConnector response parsing
//
#include <QtTest/QtTest>
#include <QObject>
#include <QString>

#include "GoogleLLMConnector.h"

class TestGoogleLLMConnector : public QObject {
    Q_OBJECT
private slots:
    void testSuccessPath();
    void testErrorPath();
};

void TestGoogleLLMConnector::testSuccessPath()
{
    // Minimal successful Google generateContent response
    const QString successJson = QStringLiteral(R"({
        "candidates": [
          { "content": { "parts": [ { "text": "Hello, world!" } ] } }
        ]
    })");

    auto* connector = new GoogleLLMConnector();

    // Directly invoke the slot
    QMetaObject::invokeMethod(connector, "onPromptFinished", Qt::DirectConnection,
                              Q_ARG(QString, successJson));

    QCOMPARE(connector->GetOutputData(QStringLiteral("response")).toString(), QStringLiteral("Hello, world!"));

    delete connector;
}

void TestGoogleLLMConnector::testErrorPath()
{
    // Minimal Google error response
    const QString errorJson = QStringLiteral(R"({
        "error": { "code": 400, "message": "API key not valid" }
    })");

    auto* connector = new GoogleLLMConnector();

    QMetaObject::invokeMethod(connector, "onPromptFinished", Qt::DirectConnection,
                              Q_ARG(QString, errorJson));

    QCOMPARE(connector->GetOutputData(QStringLiteral("response")).toString(), QStringLiteral("API key not valid"));

    delete connector;
}

QTEST_MAIN(TestGoogleLLMConnector)
#include "TestGoogleLLMConnector.moc"
