//
// Unit tests for LoopUntilNode (Adversarial Feedback Loop)
//

#include <gtest/gtest.h>

#include <QApplication>

#include "test_app.h"
#include "LoopUntilNode.h"

static QApplication* ensureAppForLoopUntil()
{
    return sharedTestApp();
}

class LoopUntilNodeTest : public ::testing::Test {
protected:
    void SetUp() override { ensureAppForLoopUntil(); }
};

// Test 1 (updated semantics): Hybrid Kickstart — when Start and Condition arrive together, Start takes precedence
// The node should immediately emit 'current' (kickstart) and ignore the condition in this packet.
TEST_F(LoopUntilNodeTest, StartAndConditionTrueEmitsResult)
{
    LoopUntilNode node;

    DataPacket in;
    in.insert(QString::fromLatin1(LoopUntilNode::kInputStartId), QStringLiteral("seed"));
    in.insert(QString::fromLatin1(LoopUntilNode::kInputConditionId), QStringLiteral("true"));

    ExecutionToken t; t.data = in;
    TokenList inputs; inputs.push_back(t);

    const TokenList outputs = node.execute(inputs);
    ASSERT_EQ(outputs.size(), 1u);

    const DataPacket& out = outputs.front().data;
    const QString resultKey = QString::fromLatin1(LoopUntilNode::kOutputResultId);
    const QString currentKey = QString::fromLatin1(LoopUntilNode::kOutputCurrentId);

    ASSERT_TRUE(out.contains(currentKey));
    ASSERT_FALSE(out.contains(resultKey));
    EXPECT_EQ(out.value(currentKey).toString(), QStringLiteral("seed"));
}

// Test 2: Loop 3 times — current fires 3 times, result on 4th when condition flips
TEST_F(LoopUntilNodeTest, LoopThreeTimesThenStop)
{
    LoopUntilNode node;

    // First call: no feedback yet, condition false
    {
        DataPacket in;
        in.insert(QString::fromLatin1(LoopUntilNode::kInputStartId), QStringLiteral("v0"));
        in.insert(QString::fromLatin1(LoopUntilNode::kInputConditionId), QStringLiteral("false"));
        ExecutionToken t; t.data = in;
        TokenList inputs; inputs.push_back(t);
        const TokenList out1 = node.execute(inputs);
        ASSERT_EQ(out1.size(), 1u);
        const DataPacket& dp1 = out1.front().data;
        ASSERT_TRUE(dp1.contains(QString::fromLatin1(LoopUntilNode::kOutputCurrentId)));
        EXPECT_EQ(dp1.value(QString::fromLatin1(LoopUntilNode::kOutputCurrentId)).toString(), QStringLiteral("v0"));
    }

    // Second call: provide feedback v1, still false
    {
        DataPacket in;
        in.insert(QString::fromLatin1(LoopUntilNode::kInputFeedbackId), QStringLiteral("v1"));
        in.insert(QString::fromLatin1(LoopUntilNode::kInputConditionId), QStringLiteral("no"));
        ExecutionToken t; t.data = in;
        TokenList inputs; inputs.push_back(t);
        const TokenList out2 = node.execute(inputs);
        ASSERT_EQ(out2.size(), 1u);
        const DataPacket& dp2 = out2.front().data;
        ASSERT_TRUE(dp2.contains(QString::fromLatin1(LoopUntilNode::kOutputCurrentId)));
        EXPECT_EQ(dp2.value(QString::fromLatin1(LoopUntilNode::kOutputCurrentId)).toString(), QStringLiteral("v1"));
    }

    // Third call: provide feedback v2, still false
    {
        DataPacket in;
        in.insert(QString::fromLatin1(LoopUntilNode::kInputFeedbackId), QStringLiteral("v2"));
        in.insert(QString::fromLatin1(LoopUntilNode::kInputConditionId), QStringLiteral("0"));
        ExecutionToken t; t.data = in;
        TokenList inputs; inputs.push_back(t);
        const TokenList out3 = node.execute(inputs);
        ASSERT_EQ(out3.size(), 1u);
        const DataPacket& dp3 = out3.front().data;
        ASSERT_TRUE(dp3.contains(QString::fromLatin1(LoopUntilNode::kOutputCurrentId)));
        EXPECT_EQ(dp3.value(QString::fromLatin1(LoopUntilNode::kOutputCurrentId)).toString(), QStringLiteral("v2"));
    }

    // Fourth call: provide feedback v3, condition true => result
    {
        DataPacket in;
        in.insert(QString::fromLatin1(LoopUntilNode::kInputFeedbackId), QStringLiteral("v3"));
        in.insert(QString::fromLatin1(LoopUntilNode::kInputConditionId), QStringLiteral("ok"));
        ExecutionToken t; t.data = in;
        TokenList inputs; inputs.push_back(t);
        const TokenList out4 = node.execute(inputs);
        ASSERT_EQ(out4.size(), 1u);
        const DataPacket& dp4 = out4.front().data;
        ASSERT_TRUE(dp4.contains(QString::fromLatin1(LoopUntilNode::kOutputResultId)));
        EXPECT_EQ(dp4.value(QString::fromLatin1(LoopUntilNode::kOutputResultId)).toString(), QStringLiteral("v3"));
    }
}

// Test 3: Max iterations safety — set Max=5, keep condition false and no new feedback; should stop after 5 loop evaluations
TEST_F(LoopUntilNodeTest, MaxIterationsSafetyBrake)
{
    LoopUntilNode node;
    node.setMaxIterations(5);

    // First call: start provided, false => current
    {
        DataPacket in;
        in.insert(QString::fromLatin1(LoopUntilNode::kInputStartId), QStringLiteral("seed"));
        in.insert(QString::fromLatin1(LoopUntilNode::kInputConditionId), QStringLiteral("false"));
        ExecutionToken t; t.data = in;
        TokenList inputs; inputs.push_back(t);
        const TokenList out = node.execute(inputs);
        ASSERT_EQ(out.size(), 1u);
        ASSERT_TRUE(out.front().data.contains(QString::fromLatin1(LoopUntilNode::kOutputCurrentId)));
    }

    // Next 5 calls: condition false, no feedback => still uses last payload; on 5th evaluation we should hit safety
    for (int i = 0; i < 5; ++i) {
        DataPacket in;
        in.insert(QString::fromLatin1(LoopUntilNode::kInputConditionId), QStringLiteral("no"));
        ExecutionToken t; t.data = in;
        TokenList inputs; inputs.push_back(t);
        const TokenList out = node.execute(inputs);
        if (i < 4) {
            // First 4 iterations produce 'current'
            ASSERT_EQ(out.size(), 1u);
            ASSERT_TRUE(out.front().data.contains(QString::fromLatin1(LoopUntilNode::kOutputCurrentId)));
        } else {
            // On the 5th condition evaluation we should stop due to safety => result
            ASSERT_EQ(out.size(), 1u);
            ASSERT_TRUE(out.front().data.contains(QString::fromLatin1(LoopUntilNode::kOutputResultId)));
        }
    }
}

// Test 4 (updated semantics): Start-only or Start+blankCondition should KICKSTART by emitting 'current' immediately
TEST_F(LoopUntilNodeTest, StartWithMissingOrBlankConditionProducesNoOutput)
{
    // Case A: condition key entirely missing
    {
        LoopUntilNode node;
        DataPacket in;
        in.insert(QString::fromLatin1(LoopUntilNode::kInputStartId), QStringLiteral("seedA"));
        ExecutionToken t; t.data = in;
        TokenList inputs; inputs.push_back(t);

        const TokenList out = node.execute(inputs);
        ASSERT_EQ(out.size(), 1u);
        ASSERT_TRUE(out.front().data.contains(QString::fromLatin1(LoopUntilNode::kOutputCurrentId)));
        EXPECT_EQ(out.front().data.value(QString::fromLatin1(LoopUntilNode::kOutputCurrentId)).toString(), QStringLiteral("seedA"));
    }

    // Case B: condition explicitly provided but blank/whitespace
    {
        LoopUntilNode node;
        DataPacket in;
        in.insert(QString::fromLatin1(LoopUntilNode::kInputStartId), QStringLiteral("seedB"));
        in.insert(QString::fromLatin1(LoopUntilNode::kInputConditionId), QStringLiteral("   ")); // blank
        ExecutionToken t; t.data = in;
        TokenList inputs; inputs.push_back(t);

        const TokenList out = node.execute(inputs);
        ASSERT_EQ(out.size(), 1u);
        ASSERT_TRUE(out.front().data.contains(QString::fromLatin1(LoopUntilNode::kOutputCurrentId)));
        EXPECT_EQ(out.front().data.value(QString::fromLatin1(LoopUntilNode::kOutputCurrentId)).toString(), QStringLiteral("seedB"));
    }
}

// Test 5: After Start, a Feedback-only tick should NOT emit; wait for Condition
// Then Condition=false emits current, next Feedback-only still does NOT emit,
// and Condition=true emits result.
TEST_F(LoopUntilNodeTest, IterationAfterStart_FeedbackOnlyDoesNotEmit)
{
    LoopUntilNode node;

    // Kickstart with Start only
    {
        DataPacket in;
        in.insert(QString::fromLatin1(LoopUntilNode::kInputStartId), QStringLiteral("S"));
        ExecutionToken t; t.data = in;
        TokenList inputs; inputs.push_back(t);
        const TokenList out = node.execute(inputs);
        ASSERT_EQ(out.size(), 1u);
        ASSERT_TRUE(out.front().data.contains(QString::fromLatin1(LoopUntilNode::kOutputCurrentId)));
        EXPECT_EQ(out.front().data.value(QString::fromLatin1(LoopUntilNode::kOutputCurrentId)).toString(), QStringLiteral("S"));
    }

    // Feedback-only: should NOT emit
    {
        DataPacket in;
        in.insert(QString::fromLatin1(LoopUntilNode::kInputFeedbackId), QStringLiteral("F1"));
        ExecutionToken t; t.data = in;
        TokenList inputs; inputs.push_back(t);
        const TokenList out = node.execute(inputs);
        ASSERT_EQ(out.size(), 0u);
    }

    // Condition=false: should emit current with F1
    {
        DataPacket in;
        in.insert(QString::fromLatin1(LoopUntilNode::kInputConditionId), QStringLiteral("no"));
        ExecutionToken t; t.data = in;
        TokenList inputs; inputs.push_back(t);
        const TokenList out = node.execute(inputs);
        ASSERT_EQ(out.size(), 1u);
        ASSERT_TRUE(out.front().data.contains(QString::fromLatin1(LoopUntilNode::kOutputCurrentId)));
        EXPECT_EQ(out.front().data.value(QString::fromLatin1(LoopUntilNode::kOutputCurrentId)).toString(), QStringLiteral("F1"));
    }

    // Feedback-only again: should NOT emit
    {
        DataPacket in;
        in.insert(QString::fromLatin1(LoopUntilNode::kInputFeedbackId), QStringLiteral("F2"));
        ExecutionToken t; t.data = in;
        TokenList inputs; inputs.push_back(t);
        const TokenList out = node.execute(inputs);
        ASSERT_EQ(out.size(), 0u);
    }

    // Condition=true: should emit result with F2
    {
        DataPacket in;
        in.insert(QString::fromLatin1(LoopUntilNode::kInputConditionId), QStringLiteral("yes"));
        ExecutionToken t; t.data = in;
        TokenList inputs; inputs.push_back(t);
        const TokenList out = node.execute(inputs);
        ASSERT_EQ(out.size(), 1u);
        ASSERT_TRUE(out.front().data.contains(QString::fromLatin1(LoopUntilNode::kOutputResultId)));
        EXPECT_EQ(out.front().data.value(QString::fromLatin1(LoopUntilNode::kOutputResultId)).toString(), QStringLiteral("F2"));
    }
}

// Test 6: isReady gating (Option B)
// - After Start (first iteration), start-only snapshot should be ready
// - After Start has kicked, feedback-only snapshot should NOT be ready until a condition was observed
// - Once a condition has been evaluated (false), feedback-only snapshot should be ready
// - Start change mid-run should be ready (treated as new run)
TEST_F(LoopUntilNodeTest, IsReadyGatingOptionB)
{
    LoopUntilNode node;

    // Case 1: Initial Start-only should be ready
    {
        QVariantMap snap; // snapshot like the engine would prepare
        snap.insert(QString::fromLatin1(LoopUntilNode::kInputStartId), QStringLiteral("S0"));
        EXPECT_TRUE(node.isReady(snap, 1));
    }

    // Kick the node once with Start to move past first iteration
    {
        DataPacket in; in.insert(QString::fromLatin1(LoopUntilNode::kInputStartId), QStringLiteral("S0"));
        ExecutionToken t; t.data = in; TokenList inputs; inputs.push_back(t);
        const TokenList out = node.execute(inputs);
        ASSERT_EQ(out.size(), 1u);
    }

    // Case 2: Feedback-only (with Start present in snapshot) should NOT be ready before any condition
    {
        QVariantMap snap;
        snap.insert(QString::fromLatin1(LoopUntilNode::kInputStartId), QStringLiteral("S0"));
        snap.insert(QString::fromLatin1(LoopUntilNode::kInputFeedbackId), QStringLiteral("F1"));
        EXPECT_FALSE(node.isReady(snap, 2));
    }

    // Provide a condition=false to register that a condition has been observed
    {
        DataPacket in; in.insert(QString::fromLatin1(LoopUntilNode::kInputConditionId), QStringLiteral("no"));
        ExecutionToken t; t.data = in; TokenList inputs; inputs.push_back(t);
        const TokenList out = node.execute(inputs);
        ASSERT_EQ(out.size(), 1u);
        ASSERT_TRUE(out.front().data.contains(QString::fromLatin1(LoopUntilNode::kOutputCurrentId)));
    }

    // Case 3: Now Feedback-only should be ready (condition has been observed)
    {
        QVariantMap snap;
        snap.insert(QString::fromLatin1(LoopUntilNode::kInputStartId), QStringLiteral("S0"));
        snap.insert(QString::fromLatin1(LoopUntilNode::kInputFeedbackId), QStringLiteral("F2"));
        EXPECT_TRUE(node.isReady(snap, 2));
    }

    // Case 4: Start change mid-run should be ready
    {
        QVariantMap snap;
        snap.insert(QString::fromLatin1(LoopUntilNode::kInputStartId), QStringLiteral("S1")); // changed
        EXPECT_TRUE(node.isReady(snap, 1));
    }
}

TEST_F(LoopUntilNodeTest, QueuesMultipleStarts)
{
    LoopUntilNode node;

    // Start 1
    {
        DataPacket in;
        in.insert(QString::fromLatin1(LoopUntilNode::kInputStartId), QStringLiteral("S1"));
        ExecutionToken t; t.data = in; t.triggeringPinId = QString::fromLatin1(LoopUntilNode::kInputStartId);
        TokenList out = node.execute({t});
        ASSERT_EQ(out.size(), 1u);
        EXPECT_EQ(out.front().data.value(QString::fromLatin1(LoopUntilNode::kOutputCurrentId)).toString(), "S1");
    }

    // Start 2 arrives while S1 is processing
    {
        DataPacket in;
        in.insert(QString::fromLatin1(LoopUntilNode::kInputStartId), QStringLiteral("S2"));
        ExecutionToken t; t.data = in; t.triggeringPinId = QString::fromLatin1(LoopUntilNode::kInputStartId);
        TokenList out = node.execute({t});
        ASSERT_EQ(out.size(), 0u); // Should be queued, nothing emitted yet
    }

    // Finish S1
    {
        DataPacket in;
        in.insert(QString::fromLatin1(LoopUntilNode::kInputConditionId), QStringLiteral("true"));
        ExecutionToken t; t.data = in; t.triggeringPinId = QString::fromLatin1(LoopUntilNode::kInputConditionId);
        TokenList out = node.execute({t});
        // 1 for result of S1, 1 for kickstart of S2
        ASSERT_EQ(out.size(), 2u);
        
        bool foundResultS1 = false;
        bool foundCurrentS2 = false;
        for (const auto& tok : out) {
            if (tok.data.contains(QString::fromLatin1(LoopUntilNode::kOutputResultId))) {
                EXPECT_EQ(tok.data.value(QString::fromLatin1(LoopUntilNode::kOutputResultId)).toString(), "S1");
                foundResultS1 = true;
            }
            if (tok.data.contains(QString::fromLatin1(LoopUntilNode::kOutputCurrentId))) {
                EXPECT_EQ(tok.data.value(QString::fromLatin1(LoopUntilNode::kOutputCurrentId)).toString(), "S2");
                foundCurrentS2 = true;
            }
        }
        EXPECT_TRUE(foundResultS1);
        EXPECT_TRUE(foundCurrentS2);
    }
}
