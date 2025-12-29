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

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>
#include <QTest>

#include "ModelCapsRegistry.h"

class TestModelCaps : public QObject {
    Q_OBJECT
private slots:
    void testRegexResolution();
    void testNegativeLookahead();
    // TDAD Phase 2: Endpoint parsing & defaulting — intentionally failing until implemented
    void testEndpointParsingAndDefaulting();
};

namespace {

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

} // namespace

void TestModelCaps::testRegexResolution()
{
    QJsonArray rules;
    rules.append(QJsonObject{
        { QStringLiteral("pattern"), QStringLiteral("^gpt-5\\.2.*") },
        { QStringLiteral("roleMode"), QStringLiteral("Developer") },
        { QStringLiteral("capabilities"), QJsonArray{ QStringLiteral("Reasoning") } }
    });
    rules.append(QJsonObject{
        { QStringLiteral("pattern"), QStringLiteral("^gemini-2\\.5.*") },
        { QStringLiteral("roleMode"), QStringLiteral("SystemInstruction") },
        { QStringLiteral("capabilities"), QJsonArray{ QStringLiteral("Vision") } }
    });

    QTemporaryFile file;
    QVERIFY2(writeRulesToTempFile(file, rules), "Unable to write temporary rules file");

    QVERIFY2(ModelCapsRegistry::instance().loadFromFile(file.fileName()), "Registry failed to load rules");

    const auto resolved = ModelCapsRegistry::instance().resolve(QStringLiteral("gpt-5.2-preview"));
    QVERIFY2(resolved.has_value(), "Resolution should have produced a value");
    QCOMPARE(resolved->roleMode, ModelCapsTypes::RoleMode::Developer);
    QVERIFY2(resolved->hasCapability(ModelCapsTypes::Capability::Reasoning), "Reasoning capability expected");
}

void TestModelCaps::testNegativeLookahead()
{
    QJsonArray rules;
    rules.append(QJsonObject{
        { QStringLiteral("pattern"), QStringLiteral("^gemini.*(?!-image)") },
        { QStringLiteral("roleMode"), QStringLiteral("System") },
        { QStringLiteral("capabilities"), QJsonArray{ QStringLiteral("Reasoning") } },
        { QStringLiteral("disabledCapabilities"), QJsonArray{ QStringLiteral("Vision") } }
    });

    QTemporaryFile file;
    QVERIFY2(writeRulesToTempFile(file, rules), "Unable to write temporary rules file");

    QVERIFY2(ModelCapsRegistry::instance().loadFromFile(file.fileName()), "Registry failed to load rules");

    const auto nonImage = ModelCapsRegistry::instance().resolve(QStringLiteral("gemini-1.5-flash"));
    QVERIFY2(nonImage.has_value(), "Expected match for non-image Gemini model");
    QVERIFY2(!nonImage->hasCapability(ModelCapsTypes::Capability::Vision), "Vision should be disabled for non-image variant");

    const auto imageVariant = ModelCapsRegistry::instance().resolve(QStringLiteral("gemini-1.5-flash-image"));
    QVERIFY2(!imageVariant.has_value(), "Image variant should not match negative lookahead rule");
}

void TestModelCaps::testEndpointParsingAndDefaulting()
{
    // Case A: Rule omits endpoint -> expect safe default "chat".
    {
        QJsonArray rules;
        rules.append(QJsonObject{
            { QStringLiteral("pattern"), QStringLiteral("^gpt-5\\.2-pro$") },
            { QStringLiteral("backend"), QStringLiteral("openai") }
        });

        QTemporaryFile file;
        QVERIFY2(writeRulesToTempFile(file, rules), "Unable to write temporary rules file (A)");

        QVERIFY2(ModelCapsRegistry::instance().loadFromFile(file.fileName()), "Registry failed to load rules (A)");

        const auto resolved = ModelCapsRegistry::instance().resolveWithRule(QStringLiteral("gpt-5.2-pro"), QStringLiteral("openai"));
        QVERIFY2(resolved.has_value(), "Expected model to match rule (A)");

        // Expect safe default Chat when endpoint field is omitted
        const auto mode = resolved->caps.endpointMode;
        QCOMPARE(mode, ModelCapsTypes::EndpointMode::Chat);
    }

    // Case B: Rule explicitly sets endpoint = "completion" -> expect Completion.
    {
        QJsonArray rules;
        rules.append(QJsonObject{
            { QStringLiteral("pattern"), QStringLiteral("^gpt-5\\.2-pro$") },
            { QStringLiteral("backend"), QStringLiteral("openai") },
            { QStringLiteral("endpoint"), QStringLiteral("completion") }
        });

        QTemporaryFile file;
        QVERIFY2(writeRulesToTempFile(file, rules), "Unable to write temporary rules file (B)");

        QVERIFY2(ModelCapsRegistry::instance().loadFromFile(file.fileName()), "Registry failed to load rules (B)");

        const auto resolved = ModelCapsRegistry::instance().resolveWithRule(QStringLiteral("gpt-5.2-pro"), QStringLiteral("openai"));
        QVERIFY2(resolved.has_value(), "Expected model to match rule (B)");

        const auto mode = resolved->caps.endpointMode;
        QCOMPARE(mode, ModelCapsTypes::EndpointMode::Completion);
    }
}

TEST(ModelCapsRegistryTests, QtHarness)
{
    TestModelCaps testCase;
    const int qtResult = QTest::qExec(&testCase);
    EXPECT_EQ(qtResult, 0);
}

#include "test_model_caps.moc"
