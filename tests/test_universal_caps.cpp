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

#include <QSignalSpy>
#include <QTest>

#include "UniversalLLMNode.h"
#include "ModelCapsRegistry.h"

class TestUniversalCaps : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void testVisionPinToggle();
    void testReasoningConstraint();
    void testExcludeNonChatVariant();
};

void TestUniversalCaps::initTestCase()
{
    QVERIFY2(ModelCapsRegistry::instance().loadFromFile(QStringLiteral(":/resources/model_caps.json")),
             "Failed to load model capabilities from resource");
}

void TestUniversalCaps::testVisionPinToggle()
{
    UniversalLLMNode node;
    QSignalSpy spy(&node, &UniversalLLMNode::inputPinsChanged);

    const auto visionCaps = ModelCapsRegistry::instance().resolve(QStringLiteral("gpt-4o"));
    QVERIFY2(visionCaps.has_value(), "Expected capabilities for gpt-4o");
    node.updateCapabilities(*visionCaps);

    const auto descriptorWithVision = node.getDescriptor();
    QVERIFY2(descriptorWithVision.inputPins.contains(QString::fromLatin1(UniversalLLMNode::kInputAttachmentId)),
             "Attachment pin should exist for vision-capable model");

    const auto noVisionCaps = ModelCapsRegistry::instance().resolve(QStringLiteral("o1-preview"));
    QVERIFY2(noVisionCaps.has_value(), "Expected capabilities for o1-preview");
    node.updateCapabilities(*noVisionCaps);

    const auto descriptorWithoutVision = node.getDescriptor();
    QVERIFY2(!descriptorWithoutVision.inputPins.contains(QString::fromLatin1(UniversalLLMNode::kInputAttachmentId)),
             "Attachment pin should be removed for non-vision model");

    QCOMPARE(spy.count(), 1);
}

void TestUniversalCaps::testReasoningConstraint()
{
    UniversalLLMNode node;

    node.onTemperatureChanged(0.5);

    const auto reasoningCaps = ModelCapsRegistry::instance().resolve(QStringLiteral("o1-preview"));
    QVERIFY2(reasoningCaps.has_value(), "Expected capabilities for o1-preview");

    node.updateCapabilities(*reasoningCaps);

    QCOMPARE(node.temperature(), 1.0);
}

void TestUniversalCaps::testExcludeNonChatVariant()
{
    // Ensure non-chat variants (e.g., tts/audio/realtime/search/transcribe) are excluded by rules
    const auto capsTts = ModelCapsRegistry::instance().resolve(QStringLiteral("gpt-4o-tts-2025-06-03"));
    QVERIFY2(!capsTts.has_value(), "gpt-4o-tts-* should be excluded from chat UI by rules");

    const auto capsRealtime = ModelCapsRegistry::instance().resolve(QStringLiteral("gpt-4o-realtime-preview-2025-06-03"));
    QVERIFY2(!capsRealtime.has_value(), "gpt-4o-realtime-* should be excluded from chat UI by rules");
}

TEST(UniversalCapsTests, QtHarness)
{
    TestUniversalCaps testCase;
    const int qtResult = QTest::qExec(&testCase);
    EXPECT_EQ(qtResult, 0);
}

#include "test_universal_caps.moc"