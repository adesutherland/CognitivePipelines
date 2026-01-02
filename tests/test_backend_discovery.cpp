#include <gtest/gtest.h>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFuture>
#include <QtConcurrent>
#include <QTest>
#include <QFile>

#include "backends/OpenAIBackend.h"
#include "ModelCapsRegistry.h"
#include "core/LLMProviderRegistry.h"

// Mock class to override the network fetch
class MockOpenAIBackend : public OpenAIBackend {
public:
    using OpenAIBackend::OpenAIBackend;

protected:
    QFuture<QByteArray> fetchRawModelListJson() override {
        // Return a mock JSON response with standard models
        return QtConcurrent::run([]() -> QByteArray {
            QJsonObject root;
            QJsonArray data;
            
            // o3 matches a rule in model_caps_with_aliases.json
            QJsonObject m1;
            m1.insert(QStringLiteral("id"), QStringLiteral("o3"));
            data.append(m1);

            // gpt-4o does not match any rule in the fixture, but let's include it 
            // to see if filtering works as expected
            QJsonObject m2;
            m2.insert(QStringLiteral("id"), QStringLiteral("gpt-4o"));
            data.append(m2);

            root.insert(QStringLiteral("data"), data);
            return QJsonDocument(root).toJson();
        });
    }
};

class TestBackendDiscovery : public QObject {
    Q_OBJECT
private slots:
    void testVirtualModelInjection();
};

void TestBackendDiscovery::testVirtualModelInjection()
{
    // 1. Setup Registry with the alias fixture
    QString fixturePath = QStringLiteral("tests/fixtures/model_caps_with_aliases.json");
    if (!QFile::exists(fixturePath)) {
        fixturePath = QStringLiteral("../tests/fixtures/model_caps_with_aliases.json");
    }
    
    // Verify fixture exists
    if (!QFile::exists(fixturePath)) {
        QFAIL("Fixture file not found");
    }

    QVERIFY2(ModelCapsRegistry::instance().loadFromFile(fixturePath), "Failed to load fixture");

    // 2. Instantiate Mock Backend
    MockOpenAIBackend backend;

    // 3. Action: Fetch models
    QFuture<QStringList> future = backend.fetchModelList();
    
    // Wait for result
    future.waitForFinished();
    QStringList models = future.result();

    // 4. Assertions (Expected to FAIL until implementation is complete)
    
    // The list should contain the real model o3 (which matched a rule)
    QVERIFY2(models.contains(QStringLiteral("o3")), "List should contain 'o3'");
    
    // The list SHOULD contain the alias 'openai-reasoning-latest'
    // This is expected to fail because injection is not yet implemented.
    QVERIFY2(models.contains(QStringLiteral("openai-reasoning-latest")), 
             qPrintable(QString("Alias 'openai-reasoning-latest' missing. Found: %1").arg(models.join(", "))));
    
    // The alias should be at the top (index 0 or 1, since there's also o3)
    // Actually, if it's "Sort to Top", it should probably be index 0.
    if (!models.isEmpty()) {
        QCOMPARE(models.first(), QStringLiteral("openai-reasoning-latest"));
    } else {
        QFAIL("Model list is empty");
    }
}

TEST(BackendDiscoveryTests, QtHarness)
{
    TestBackendDiscovery testCase;
    const int qtResult = QTest::qExec(&testCase);
    EXPECT_EQ(qtResult, 0);
}

#include "test_backend_discovery.moc"
