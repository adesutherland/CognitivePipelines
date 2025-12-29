//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include <gtest/gtest.h>

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>
#include <QTest>
#include <QtConcurrent>

#include "backends/OpenAIBackend.h"
#include "ModelCapsRegistry.h"

namespace {

// Helper: write rules JSON to a temporary file
bool writeRulesToTempFile(QTemporaryFile& file, const QJsonArray& rules)
{
    if (!file.open()) {
        return false;
    }

    const QJsonObject root { { QStringLiteral("rules"), rules } };
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);

    const auto written = file.write(payload);
    file.flush();
    file.close();

    return written == payload.size();
}

class TestableOpenAIBackend : public OpenAIBackend {
protected:
    QFuture<QByteArray> fetchRawModelListJson() override
    {
        // Return a hard-coded JSON payload asynchronously
        return QtConcurrent::run([]() {
            const QByteArray json = R"({ "data": [ { "id": "gpt-5-preview" }, { "id": "gpt-3.5-turbo-instruct-legacy" }, { "id": "random-junk" } ] })";
            return json;
        });
    }
};

} // namespace

class DynamicDiscoveryQtTest : public QObject {
    Q_OBJECT
private slots:
    void filtersOpenAIModelsAgainstRegistry();
};

void DynamicDiscoveryQtTest::filtersOpenAIModelsAgainstRegistry()
{
    // Prepare a minimal rule set that accepts ^gpt-5.* only
    QJsonArray rules;
    rules.append(QJsonObject{
        { QStringLiteral("pattern"), QStringLiteral("^gpt-5.*") }
    });

    QTemporaryFile file;
    QVERIFY2(writeRulesToTempFile(file, rules), "Unable to write temporary rules file");
    QVERIFY2(ModelCapsRegistry::instance().loadFromFile(file.fileName()), "Registry failed to load rules");

    TestableOpenAIBackend backend;
    auto fut = backend.fetchModelList();
    fut.waitForFinished();
    const QStringList models = fut.result();

    // Expected behavior (will FAIL now because fetchModelList is a stub):
    QVERIFY2(models.contains(QStringLiteral("gpt-5-preview")), "Expected 'gpt-5-preview' to be present after filtering");
    QVERIFY2(!models.contains(QStringLiteral("gpt-3.5-turbo-instruct-legacy")), "Unexpected legacy model present");
    QVERIFY2(!models.contains(QStringLiteral("random-junk")), "Unexpected junk model present");
}

TEST(DynamicDiscoveryTests, QtHarness)
{
    DynamicDiscoveryQtTest testCase;
    const int qtResult = QTest::qExec(&testCase);
    EXPECT_EQ(qtResult, 0);
}

#include "test_dynamic_discovery.moc"
