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

#include "RetryLoopNode.h"
#include "RetryLoopPropertiesWidget.h"
#include <QDebug>

RetryLoopNode::RetryLoopNode(QObject* parent)
    : QObject(parent)
{
}

NodeDescriptor RetryLoopNode::getDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("retry-loop");
    desc.name = QStringLiteral("Retry Loop");
    desc.category = QStringLiteral("Flow Control");

    // Input: task_in (The original payload)
    PinDefinition taskIn;
    taskIn.direction = PinDirection::Input;
    taskIn.id = QString::fromLatin1(kInputTaskId);
    taskIn.name = QStringLiteral("Task In");
    taskIn.type = QStringLiteral("text");
    desc.inputPins.insert(taskIn.id, taskIn);

    // Input: worker_feedback (The result/check from the worker)
    PinDefinition workerFeedback;
    workerFeedback.direction = PinDirection::Input;
    workerFeedback.id = QString::fromLatin1(kInputWorkerFeedbackId);
    workerFeedback.name = QStringLiteral("Worker Feedback");
    workerFeedback.type = QStringLiteral("text");
    desc.inputPins.insert(workerFeedback.id, workerFeedback);

    // Output: worker_instruction (The payload sent to the worker)
    PinDefinition workerInstruction;
    workerInstruction.direction = PinDirection::Output;
    workerInstruction.id = QString::fromLatin1(kOutputWorkerInstructionId);
    workerInstruction.name = QStringLiteral("Worker Instruction");
    workerInstruction.type = QStringLiteral("text");
    desc.outputPins.insert(workerInstruction.id, workerInstruction);

    // Output: verified_result (The final approved output)
    PinDefinition verifiedResult;
    verifiedResult.direction = PinDirection::Output;
    verifiedResult.id = QString::fromLatin1(kOutputVerifiedResultId);
    verifiedResult.name = QStringLiteral("Verified Result");
    verifiedResult.type = QStringLiteral("text");
    desc.outputPins.insert(verifiedResult.id, verifiedResult);

    return desc;
}

void RetryLoopNode::setFailureString(const QString& value)
{
    if (m_failureString != value) {
        m_failureString = value;
        emit failureStringChanged(m_failureString);
    }
}

void RetryLoopNode::setMaxRetries(int value)
{
    if (m_maxRetries != value) {
        m_maxRetries = value;
        emit maxRetriesChanged(m_maxRetries);
    }
}

QWidget* RetryLoopNode::createConfigurationWidget(QWidget* parent)
{
    return new RetryLoopPropertiesWidget(this, parent);
}

bool RetryLoopNode::isReady(const QVariantMap& inputs, int incomingConnectionsCount) const
{
    Q_UNUSED(incomingConnectionsCount);
    // Ready if we have task_in (to start) OR worker_feedback (to continue/finish)
    return inputs.contains(QString::fromLatin1(kInputTaskId)) ||
           inputs.contains(QString::fromLatin1(kInputWorkerFeedbackId));
}

TokenList RetryLoopNode::execute(const TokenList& incomingTokens)
{
    TokenList outputs;

    // Step 1: Ingest New Tasks
    for (const auto& token : incomingTokens) {
        if (token.triggeringPinId == QString::fromLatin1(kInputTaskId)) {
            m_taskQueue.push_back(token.data.value(QString::fromLatin1(kInputTaskId)));
        }
    }

    // Step 2: Process Feedback
    if (m_isProcessing) {
        for (const auto& token : incomingTokens) {
            if (token.triggeringPinId == QString::fromLatin1(kInputWorkerFeedbackId)) {
                QVariant feedbackPayload = token.data.value(QString::fromLatin1(kInputWorkerFeedbackId));
                QString feedbackStr = feedbackPayload.toString();

                if (feedbackStr.contains(m_failureString, Qt::CaseInsensitive)) {
                    if (m_retryCount < m_maxRetries) {
                        m_retryCount++;

                        ExecutionToken outToken;
                        outToken.forceExecution = true; // Bypass deduplication for retries
                        outToken.data.insert(QString::fromLatin1(kOutputWorkerInstructionId), m_cachedPayload);
                        outToken.data.insert(QStringLiteral("text"), m_cachedPayload);
                        outputs.push_back(std::move(outToken));
                    } else {
                        // Max retries reached, abort this task
                        ExecutionToken errorToken;
                        errorToken.data.insert(QStringLiteral("__error"), QStringLiteral("RetryLoopNode: Max retries exceeded."));
                        outputs.push_back(std::move(errorToken));

                        m_isProcessing = false;
                        m_taskQueue.clear();
                        m_cachedPayload = QVariant();
                    }
                } else {
                    // Success
                    ExecutionToken outToken;
                    outToken.data.insert(QString::fromLatin1(kOutputVerifiedResultId), feedbackPayload);
                    outToken.data.insert(QStringLiteral("text"), feedbackPayload);
                    outputs.push_back(std::move(outToken));

                    m_isProcessing = false;
                    m_cachedPayload = QVariant();
                }
                break; // Handle only one feedback at a time
            }
        }
    }

    // Step 3: Drive Queue
    if (!m_isProcessing && !m_taskQueue.empty()) {
        m_cachedPayload = m_taskQueue.front();
        m_taskQueue.pop_front();
        m_retryCount = 0;
        m_isProcessing = true;

        ExecutionToken outToken;
        outToken.data.insert(QString::fromLatin1(kOutputWorkerInstructionId), m_cachedPayload);
        outToken.data.insert(QStringLiteral("text"), m_cachedPayload);
        outputs.push_back(std::move(outToken));
    }

    return outputs;
}

QJsonObject RetryLoopNode::saveState() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("maxRetries"), m_maxRetries);
    obj.insert(QStringLiteral("failureString"), m_failureString);
    return obj;
}

void RetryLoopNode::loadState(const QJsonObject& data)
{
    if (data.contains(QStringLiteral("maxRetries"))) {
        setMaxRetries(data.value(QStringLiteral("maxRetries")).toInt());
    }
    if (data.contains(QStringLiteral("failureString"))) {
        setFailureString(data.value(QStringLiteral("failureString")).toString());
    }
}
