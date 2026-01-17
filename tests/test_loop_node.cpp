//
// Unit tests for LoopNode (For-Each Iterator)
//

#include <gtest/gtest.h>

#include <QApplication>

#include "test_app.h"
#include "LoopNode.h"

static QApplication* ensureAppForLoopNode()
{
    return sharedTestApp();
}

class LoopNodeTest : public ::testing::Test {
protected:
    void SetUp() override { ensureAppForLoopNode(); }
};

TEST_F(LoopNodeTest, NewlineSplitProducesBodyAndPassthrough)
{
    LoopNode node;

    DataPacket in;
    in.insert(QString::fromLatin1(LoopNode::kInputListId), QStringLiteral("Apple\nBanana\nCherry"));

    ExecutionToken t; t.data = in;
    TokenList inputs; inputs.push_back(t);

    const TokenList outputs = node.execute(inputs);
    // Expect 3 body tokens + 1 passthrough = 4 total
    ASSERT_EQ(outputs.size(), 4u);

    int bodyCount = 0;
    int passthroughCount = 0;
    const QString bodyKey = QString::fromLatin1(LoopNode::kOutputBodyId);
    const QString passthroughKey = QString::fromLatin1(LoopNode::kOutputPassthroughId);
    for (const auto& tok : outputs) {
        if (tok.data.contains(bodyKey)) ++bodyCount;
        if (tok.data.contains(passthroughKey)) ++passthroughCount;
    }
    EXPECT_EQ(bodyCount, 3);
    EXPECT_EQ(passthroughCount, 1);
}

TEST_F(LoopNodeTest, JsonArrayProducesCorrectCounts)
{
    LoopNode node;

    DataPacket in;
    in.insert(QString::fromLatin1(LoopNode::kInputListId), QStringLiteral("[\"One\",\"Two\"]"));

    ExecutionToken t; t.data = in;
    TokenList inputs; inputs.push_back(t);

    const TokenList outputs = node.execute(inputs);
    ASSERT_EQ(outputs.size(), 3u); // 2 body + 1 passthrough

    int bodyCount = 0;
    int passthroughCount = 0;
    const QString bodyKey = QString::fromLatin1(LoopNode::kOutputBodyId);
    const QString passthroughKey = QString::fromLatin1(LoopNode::kOutputPassthroughId);
    for (const auto& tok : outputs) {
        if (tok.data.contains(bodyKey)) ++bodyCount;
        if (tok.data.contains(passthroughKey)) ++passthroughCount;
    }
    EXPECT_EQ(bodyCount, 2);
    EXPECT_EQ(passthroughCount, 1);
}

TEST_F(LoopNodeTest, EmptyInputEmitsOnlyPassthrough)
{
    LoopNode node;

    DataPacket in;
    in.insert(QString::fromLatin1(LoopNode::kInputListId), QString());

    ExecutionToken t; t.data = in;
    TokenList inputs; inputs.push_back(t);

    const TokenList outputs = node.execute(inputs);
    ASSERT_EQ(outputs.size(), 1u); // Only passthrough

    const QString bodyKey = QString::fromLatin1(LoopNode::kOutputBodyId);
    const QString passthroughKey = QString::fromLatin1(LoopNode::kOutputPassthroughId);

    int bodyCount = 0;
    int passthroughCount = 0;
    for (const auto& tok : outputs) {
        if (tok.data.contains(bodyKey)) ++bodyCount;
        if (tok.data.contains(passthroughKey)) ++passthroughCount;
    }
    EXPECT_EQ(bodyCount, 0);
    EXPECT_EQ(passthroughCount, 1);
}

TEST_F(LoopNodeTest, MarkdownBulletsProduceTwoBodyTokens)
{
    LoopNode node;

    const QString original = QStringLiteral("* Item A\n* Item B");
    DataPacket in;
    in.insert(QString::fromLatin1(LoopNode::kInputListId), original);

    ExecutionToken t; t.data = in;
    TokenList inputs; inputs.push_back(t);

    const TokenList outputs = node.execute(inputs);
    // 2 body + 1 passthrough
    ASSERT_EQ(outputs.size(), 3u);

    int bodyCount = 0;
    const QString bodyKey = QString::fromLatin1(LoopNode::kOutputBodyId);
    for (const auto& tok : outputs) {
        if (tok.data.contains(bodyKey)) ++bodyCount;
    }
    EXPECT_EQ(bodyCount, 2);
}

TEST_F(LoopNodeTest, MarkdownNumberedProduceTwoBodyTokens)
{
    LoopNode node;

    const QString original = QStringLiteral("1. First\n2. Second");
    DataPacket in;
    in.insert(QString::fromLatin1(LoopNode::kInputListId), original);

    ExecutionToken t; t.data = in;
    TokenList inputs; inputs.push_back(t);

    const TokenList outputs = node.execute(inputs);
    // 2 body + 1 passthrough
    ASSERT_EQ(outputs.size(), 3u);

    int bodyCount = 0;
    const QString bodyKey = QString::fromLatin1(LoopNode::kOutputBodyId);
    for (const auto& tok : outputs) {
        if (tok.data.contains(bodyKey)) ++bodyCount;
    }
    EXPECT_EQ(bodyCount, 2);
}

TEST_F(LoopNodeTest, PassthroughPayloadEchoesOriginalInput)
{
    LoopNode node;

    const QString original = QStringLiteral("Alpha\nBeta\nGamma");
    DataPacket in;
    in.insert(QString::fromLatin1(LoopNode::kInputListId), original);

    ExecutionToken t; t.data = in;
    TokenList inputs; inputs.push_back(t);

    const TokenList outputs = node.execute(inputs);
    const QString passthroughKey = QString::fromLatin1(LoopNode::kOutputPassthroughId);

    bool found = false;
    for (const auto& tok : outputs) {
        if (tok.data.contains(passthroughKey)) {
            EXPECT_EQ(tok.data.value(QStringLiteral("text")).toString(), original);
            EXPECT_EQ(tok.data.value(passthroughKey).toString(), original);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(LoopNodeTest, MultipleInputTokensProcessedSequentially)
{
    LoopNode node;

    TokenList inputs;
    {
        DataPacket in;
        in.insert(QString::fromLatin1(LoopNode::kInputListId), QStringLiteral("A\nB"));
        ExecutionToken t; t.data = in;
        inputs.push_back(t);
    }
    {
        DataPacket in;
        in.insert(QString::fromLatin1(LoopNode::kInputListId), QStringLiteral("C\nD"));
        ExecutionToken t; t.data = in;
        inputs.push_back(t);
    }

    const TokenList outputs = node.execute(inputs);
    // (2 items + 1 passthrough) * 2 = 6 total
    ASSERT_EQ(outputs.size(), 6u);

    int passthroughCount = 0;
    const QString passthroughKey = QString::fromLatin1(LoopNode::kOutputPassthroughId);
    for (const auto& tok : outputs) {
        if (tok.data.contains(passthroughKey)) ++passthroughCount;
    }
    EXPECT_EQ(passthroughCount, 2);
}
