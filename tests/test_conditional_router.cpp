//
// Unit tests for ConditionalRouterNode
//

#include <gtest/gtest.h>

#include <QApplication>
#include <QTest>

#include "test_app.h"
#include "ConditionalRouterNode.h"
#include "ConditionalRouterPropertiesWidget.h"

// Local helper to ensure a QApplication exists (mirrors pattern in test_nodes.cpp)
static QApplication* ensureAppForConditionalRouter()
{
    return sharedTestApp();
}

class ConditionalRouterNodeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ensureAppForConditionalRouter();
    }
};

TEST_F(ConditionalRouterNodeTest, RoutingTrueVariantsGoToTrueOutput)
{
    ConditionalRouterNode node;

    const QStringList trueValues = { QStringLiteral("pass"), QStringLiteral("ok"),
                                     QStringLiteral("TRUE"), QStringLiteral("1") };

    for (const QString& cond : trueValues) {
        DataPacket in;
        // Provide payload using the standard "in" key as required by the router's input pin.
        in.insert(QString::fromLatin1(ConditionalRouterNode::kInputDataId), QStringLiteral("payload"));
        in.insert(QString::fromLatin1(ConditionalRouterNode::kInputConditionId), cond);

        ExecutionToken token;
        token.data = in;
        TokenList inputs;
        inputs.push_back(std::move(token));

        const TokenList outputs = node.execute(inputs);
        ASSERT_FALSE(outputs.empty());
        const DataPacket& out = outputs.front().data;

        const QString textKey = QStringLiteral("text");
        const QString trueKey = QString::fromLatin1(ConditionalRouterNode::kOutputTrueId);
        const QString falseKey = QString::fromLatin1(ConditionalRouterNode::kOutputFalseId);

        // Should always contain the canonical text payload
        ASSERT_TRUE(out.contains(textKey));
        EXPECT_EQ(out.value(textKey).toString(), QStringLiteral("payload"));

        // And the active branch key only
        ASSERT_TRUE(out.contains(trueKey));
        EXPECT_EQ(out.value(trueKey).toString(), QStringLiteral("payload"));
        EXPECT_FALSE(out.contains(falseKey));
    }
}

TEST_F(ConditionalRouterNodeTest, RoutingFalseVariantsGoToFalseOutput)
{
    ConditionalRouterNode node;

    const QStringList falseValues = { QStringLiteral("fail"), QStringLiteral("no"),
                                      QStringLiteral("0"), QStringLiteral("random") };

    for (const QString& cond : falseValues) {
        DataPacket in;
        in.insert(QString::fromLatin1(ConditionalRouterNode::kInputDataId), QStringLiteral("payload"));
        in.insert(QString::fromLatin1(ConditionalRouterNode::kInputConditionId), cond);

        ExecutionToken token;
        token.data = in;
        TokenList inputs;
        inputs.push_back(std::move(token));

        const TokenList outputs = node.execute(inputs);
        ASSERT_FALSE(outputs.empty());
        const DataPacket& out = outputs.front().data;

        const QString textKey = QStringLiteral("text");
        const QString trueKey = QString::fromLatin1(ConditionalRouterNode::kOutputTrueId);
        const QString falseKey = QString::fromLatin1(ConditionalRouterNode::kOutputFalseId);

        ASSERT_TRUE(out.contains(textKey));
        EXPECT_EQ(out.value(textKey).toString(), QStringLiteral("payload"));

        ASSERT_TRUE(out.contains(falseKey));
        EXPECT_EQ(out.value(falseKey).toString(), QStringLiteral("payload"));
        EXPECT_FALSE(out.contains(trueKey));
    }
}

TEST_F(ConditionalRouterNodeTest, DefaultConditionFallbackUsedWhenPinEmpty)
{
    ConditionalRouterNode node;

    // Configure default condition via properties widget to "true"
    QWidget* w = node.createConfigurationWidget(nullptr);
    ASSERT_NE(w, nullptr);
    auto* props = dynamic_cast<ConditionalRouterPropertiesWidget*>(w);
    ASSERT_NE(props, nullptr);

    props->setDefaultCondition(QStringLiteral("true"));
    QApplication::processEvents();

    DataPacket in;
    in.insert(QString::fromLatin1(ConditionalRouterNode::kInputDataId), QStringLiteral("payload"));
    // NOTE: no condition pin provided -> should fall back to default

    ExecutionToken token;
    token.data = in;
    TokenList inputs;
    inputs.push_back(std::move(token));

    const TokenList outputs = node.execute(inputs);
    ASSERT_FALSE(outputs.empty());
    const DataPacket& out = outputs.front().data;

    const QString textKey = QStringLiteral("text");
    const QString trueKey = QString::fromLatin1(ConditionalRouterNode::kOutputTrueId);
    const QString falseKey = QString::fromLatin1(ConditionalRouterNode::kOutputFalseId);

    ASSERT_TRUE(out.contains(textKey));
    EXPECT_EQ(out.value(textKey).toString(), QStringLiteral("payload"));

    ASSERT_TRUE(out.contains(trueKey));
    EXPECT_EQ(out.value(trueKey).toString(), QStringLiteral("payload"));
    EXPECT_FALSE(out.contains(falseKey));

    delete w;
}

TEST_F(ConditionalRouterNodeTest, DataPayloadPassesThroughUnchanged)
{
    ConditionalRouterNode node;

    const QString complexPayload = QStringLiteral("{""a"":42,""b"":""text"",""c"": [1,2,3]}");

    DataPacket in;
    // Provide as input data
    in.insert(QString::fromLatin1(ConditionalRouterNode::kInputDataId), complexPayload);
    in.insert(QString::fromLatin1(ConditionalRouterNode::kInputConditionId), QStringLiteral("ok"));

    ExecutionToken token;
    token.data = in;
    TokenList inputs;
    inputs.push_back(std::move(token));

    const TokenList outputs = node.execute(inputs);
    ASSERT_FALSE(outputs.empty());
    const DataPacket& out = outputs.front().data;

    const QString textKey = QStringLiteral("text");
    const QString trueKey = QString::fromLatin1(ConditionalRouterNode::kOutputTrueId);

    ASSERT_TRUE(out.contains(textKey));
    ASSERT_TRUE(out.contains(trueKey));

    EXPECT_EQ(out.value(textKey).toString(), complexPayload);
    EXPECT_EQ(out.value(trueKey).toString(), complexPayload);
}
