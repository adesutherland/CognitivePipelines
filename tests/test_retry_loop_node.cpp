#include <gtest/gtest.h>
#include "RetryLoopNode.h"
#include <QVariantMap>

class RetryLoopNodeTest : public ::testing::Test {
protected:
    RetryLoopNode node;
};

TEST_F(RetryLoopNodeTest, InitialTaskStartsLoop) {
    TokenList inputs;
    ExecutionToken t;
    t.triggeringPinId = QString::fromLatin1(RetryLoopNode::kInputTaskId);
    t.data[QString::fromLatin1(RetryLoopNode::kInputTaskId)] = "Initial Task";
    inputs.push_back(t);

    TokenList outputs = node.execute(inputs);

    ASSERT_EQ(outputs.size(), 1);
    EXPECT_EQ(outputs.front().data[QString::fromLatin1(RetryLoopNode::kOutputWorkerInstructionId)].toString(), "Initial Task");
    EXPECT_EQ(outputs.front().data["text"].toString(), "Initial Task");
}

TEST_F(RetryLoopNodeTest, FailureTriggersRetry) {
    // Start task
    {
        TokenList inputs;
        ExecutionToken t;
        t.triggeringPinId = QString::fromLatin1(RetryLoopNode::kInputTaskId);
        t.data[QString::fromLatin1(RetryLoopNode::kInputTaskId)] = "PayLoad";
        inputs.push_back(t);
        node.execute(inputs);
    }

    // Send failure feedback
    TokenList inputs;
    ExecutionToken t;
    t.triggeringPinId = QString::fromLatin1(RetryLoopNode::kInputWorkerFeedbackId);
    t.data[QString::fromLatin1(RetryLoopNode::kInputWorkerFeedbackId)] = "Some error FAIL occurred";
    inputs.push_back(t);

    TokenList outputs = node.execute(inputs);

    ASSERT_EQ(outputs.size(), 1);
    EXPECT_EQ(outputs.front().data[QString::fromLatin1(RetryLoopNode::kOutputWorkerInstructionId)].toString(), "PayLoad");
    EXPECT_TRUE(outputs.front().forceExecution);
}

TEST_F(RetryLoopNodeTest, SuccessEmitsVerifiedResult) {
    // Start task
    {
        TokenList inputs;
        ExecutionToken t;
        t.triggeringPinId = QString::fromLatin1(RetryLoopNode::kInputTaskId);
        t.data[QString::fromLatin1(RetryLoopNode::kInputTaskId)] = "PayLoad";
        inputs.push_back(t);
        node.execute(inputs);
    }

    // Send success feedback
    TokenList inputs;
    ExecutionToken t;
    t.triggeringPinId = QString::fromLatin1(RetryLoopNode::kInputWorkerFeedbackId);
    t.data[QString::fromLatin1(RetryLoopNode::kInputWorkerFeedbackId)] = "All GOOD";
    inputs.push_back(t);

    TokenList outputs = node.execute(inputs);

    ASSERT_EQ(outputs.size(), 1);
    EXPECT_EQ(outputs.front().data[QString::fromLatin1(RetryLoopNode::kOutputVerifiedResultId)].toString(), "All GOOD");
}

TEST_F(RetryLoopNodeTest, MaxRetriesEnforced) {
    node.setMaxRetries(2);
    
    // Start task
    {
        TokenList inputs;
        ExecutionToken t;
        t.triggeringPinId = QString::fromLatin1(RetryLoopNode::kInputTaskId);
        t.data[QString::fromLatin1(RetryLoopNode::kInputTaskId)] = "PayLoad";
        inputs.push_back(t);
        node.execute(inputs);
    }

    // Retry 1
    {
        TokenList inputs;
        ExecutionToken t;
        t.triggeringPinId = QString::fromLatin1(RetryLoopNode::kInputWorkerFeedbackId);
        t.data[QString::fromLatin1(RetryLoopNode::kInputWorkerFeedbackId)] = "FAIL";
        inputs.push_back(t);
        auto out = node.execute(inputs);
        EXPECT_EQ(out.size(), 1);
    }

    // Retry 2
    {
        TokenList inputs;
        ExecutionToken t;
        t.triggeringPinId = QString::fromLatin1(RetryLoopNode::kInputWorkerFeedbackId);
        t.data[QString::fromLatin1(RetryLoopNode::kInputWorkerFeedbackId)] = "FAIL";
        inputs.push_back(t);
        auto out = node.execute(inputs);
        EXPECT_EQ(out.size(), 1);
    }

    // Hit Max - should emit an error packet
    {
        TokenList inputs;
        ExecutionToken t;
        t.triggeringPinId = QString::fromLatin1(RetryLoopNode::kInputWorkerFeedbackId);
        t.data[QString::fromLatin1(RetryLoopNode::kInputWorkerFeedbackId)] = "FAIL";
        inputs.push_back(t);
        auto out = node.execute(inputs);
        ASSERT_EQ(out.size(), 1);
        EXPECT_TRUE(out.front().data.contains("__error"));
        EXPECT_EQ(out.front().data["__error"].toString(), "RetryLoopNode: Max retries exceeded.");
    }
}

TEST_F(RetryLoopNodeTest, IsReadyLogic) {
    QVariantMap inputs;
    
    // Default connection count (not used by RetryLoopNode's override)
    EXPECT_FALSE(node.isReady(inputs, 2));

    inputs[QString::fromLatin1(RetryLoopNode::kInputTaskId)] = "something";
    EXPECT_TRUE(node.isReady(inputs, 2));

    inputs.clear();
    inputs[QString::fromLatin1(RetryLoopNode::kInputWorkerFeedbackId)] = "result";
    EXPECT_TRUE(node.isReady(inputs, 2));
}
