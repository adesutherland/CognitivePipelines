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

#include "LoopUntilNode.h"
#include "LoopUntilPropertiesWidget.h"

#include <QJsonObject>
#include <QVariant>
#include <QtNodes/internal/Definitions.hpp>

LoopUntilNode::LoopUntilNode(QObject* parent)
    : QObject(parent)
{
}

void LoopUntilNode::setMaxIterations(int value)
{
    if (value < 1) value = 1;
    if (m_maxIterations == value) return;
    m_maxIterations = value;
    emit maxIterationsChanged(m_maxIterations);
}

NodeDescriptor LoopUntilNode::getDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("loop-until");
    desc.name = QStringLiteral("Loop Until");
    desc.category = QStringLiteral("Control Flow");

    // Inputs (strict order: 0 Start Value, 1 Loop Feedback, 2 Stop Condition)
    {
        PinDefinition pin;
        pin.direction = PinDirection::Input;
        pin.id = QString::fromLatin1(kInputStartId);
        pin.name = QStringLiteral("Start Value");
        pin.type = QStringLiteral("text"); // accept generic; engine doesn't enforce types strictly
        desc.inputPins.insert(pin.id, pin);
    }
    {
        PinDefinition pin;
        pin.direction = PinDirection::Input;
        pin.id = QString::fromLatin1(kInputFeedbackId);
        pin.name = QStringLiteral("Loop Feedback");
        pin.type = QStringLiteral("text");
        desc.inputPins.insert(pin.id, pin);
    }
    {
        PinDefinition pin;
        pin.direction = PinDirection::Input;
        pin.id = QString::fromLatin1(kInputConditionId);
        pin.name = QStringLiteral("Stop Condition");
        pin.type = QStringLiteral("text");
        desc.inputPins.insert(pin.id, pin);
    }

    // Outputs (strict order: 0 Final Result, 1 Current Value)
    {
        PinDefinition pin;
        pin.direction = PinDirection::Output;
        pin.id = QString::fromLatin1(kOutputResultId);
        pin.name = QStringLiteral("Final Result");
        pin.type = QStringLiteral("text");
        desc.outputPins.insert(pin.id, pin);
    }
    {
        PinDefinition pin;
        pin.direction = PinDirection::Output;
        pin.id = QString::fromLatin1(kOutputCurrentId);
        pin.name = QStringLiteral("Current Value");
        pin.type = QStringLiteral("text");
        desc.outputPins.insert(pin.id, pin);
    }

    return desc;
}

QWidget* LoopUntilNode::createConfigurationWidget(QWidget* parent)
{
    auto* w = new LoopUntilPropertiesWidget(parent);
    w->setMaxIterations(m_maxIterations);
    QObject::connect(w, &LoopUntilPropertiesWidget::maxIterationsChanged,
                     this, &LoopUntilNode::setMaxIterations);
    QObject::connect(this, &LoopUntilNode::maxIterationsChanged,
                     w, &LoopUntilPropertiesWidget::setMaxIterations);
    return w;
}

bool LoopUntilNode::isReady(const QVariantMap& inputs, int incomingConnectionsCount) const
{
    Q_UNUSED(incomingConnectionsCount);
    const QString startKey = QString::fromLatin1(kInputStartId);
    const QString feedbackKey = QString::fromLatin1(kInputFeedbackId);
    const QString condKey = QString::fromLatin1(kInputConditionId);

    // 1. Condition is always a reason to be ready if we are processing
    if (m_isProcessing && inputs.contains(condKey)) {
        return true;
    }

    // 2. Start is a reason to be ready if it's new OR it's the first time
    if (inputs.contains(startKey)) {
        const QVariant inStart = inputs.value(startKey);
        const bool startChanged = (!m_hasLastIngestedStart) || (inStart != m_lastIngestedStart);
        if (startChanged) {
            return true;
        }
    }

    // 3. Feedback is a reason only if we are processing AND have seen a condition (to avoid premature ticks)
    if (m_isProcessing && inputs.contains(feedbackKey) && m_hasLastEvaluatedCondition) {
        return true;
    }

    return false;
}

bool LoopUntilNode::isTruthy(const QVariant& v)
{
    // False when missing/invalid
    if (!v.isValid() || v.isNull()) return false;

    // Explicit bools
    if (v.typeId() == QMetaType::Bool) {
        return v.toBool();
    }

    // Strings: case-insensitive handling per spec + compatibility (ok/pass)
    if (v.typeId() == QMetaType::QString) {
        const QString s = v.toString().trimmed().toLower();
        if (s.isEmpty()) return false;
        if (s == QStringLiteral("true") || s == QStringLiteral("yes") || s == QStringLiteral("1") ||
            s == QStringLiteral("ok")   || s == QStringLiteral("pass")) return true;
        if (s == QStringLiteral("false") || s == QStringLiteral("no") || s == QStringLiteral("0")) return false;
        // If it's a number string, treat non-zero as true
        bool ok = false;
        const double d = s.toDouble(&ok);
        if (ok) return d != 0.0;
        // Otherwise, not part of allowed truthy set → false
        return false;
    }

    // Numerics: non-zero true
    if (v.canConvert<double>()) {
        return v.toDouble() != 0.0;
    }

    // Any other type → treat as false by default per spec (only explicit values considered truthy)
    return false;
}

TokenList LoopUntilNode::execute(const TokenList& incomingTokens)
{
    const QString startKey = QString::fromLatin1(kInputStartId);
    const QString feedbackKey = QString::fromLatin1(kInputFeedbackId);
    const QString condKey = QString::fromLatin1(kInputConditionId);

    // Ingest: If Start Value is present as a trigger (or no trigger info for tests)
    for (const auto& token : incomingTokens) {
        if (token.triggeringPinId == startKey || (token.triggeringPinId.isEmpty() && token.data.contains(startKey))) {
            const QVariant inStart = token.data.value(startKey);
            m_taskQueue.push_back(inStart);
            m_lastIngestedStart = inStart;
            m_hasLastIngestedStart = true;
        }
    }

    TokenList outputs;

    // Process: If m_isProcessing is true, check for feedback or condition tokens
    if (m_isProcessing) {
        bool conditionTriggered = false;
        QVariant condVal;

        for (const auto& token : incomingTokens) {
            const bool isFeedbackTrigger = (token.triggeringPinId == feedbackKey);
            const bool isConditionTrigger = (token.triggeringPinId == condKey);
            const bool isTestTrigger = token.triggeringPinId.isEmpty();

            if (isFeedbackTrigger || isTestTrigger) {
                if (token.data.contains(feedbackKey)) {
                    m_pendingFeedback = token.data.value(feedbackKey).toString();
                    m_hasPendingFeedback = true;
                }
            }

            if (isConditionTrigger || isTestTrigger) {
                if (token.data.contains(condKey)) {
                    conditionTriggered = true;
                    condVal = token.data.value(condKey);
                    // Also check for feedback in the same token (snapshot)
                    if (token.data.contains(feedbackKey)) {
                        m_pendingFeedback = token.data.value(feedbackKey).toString();
                        m_hasPendingFeedback = true;
                    }
                }
            }
        }

        if (conditionTriggered && m_hasPendingFeedback) {
            m_hasLastEvaluatedCondition = true;
            const bool stopNow = isTruthy(condVal);
            const bool finalStop = stopNow || ((m_iterationCount + 1) >= m_maxIterations);

            DataPacket out;
            out.insert(QStringLiteral("text"), m_pendingFeedback);
            if (finalStop) {
                out.insert(QString::fromLatin1(kOutputResultId), m_pendingFeedback);
                m_isProcessing = false;
                m_hasPendingFeedback = false;
            } else {
                out.insert(QString::fromLatin1(kOutputCurrentId), m_pendingFeedback);
                ++m_iterationCount;
            }

            ExecutionToken tok;
            tok.data = out;
            outputs.push_back(std::move(tok));
        }
    }

    // Drive: If not processing and queue is not empty, start next loop
    if (!m_isProcessing && !m_taskQueue.empty()) {
        QVariant nextStart = m_taskQueue.front();
        m_taskQueue.pop_front();

        m_iterationCount = 0;
        m_pendingFeedback = nextStart.toString();
        m_hasPendingFeedback = true;
        m_isProcessing = true;
        m_hasLastEvaluatedCondition = false; // Reset for new run

        DataPacket out;
        out.insert(QStringLiteral("text"), m_pendingFeedback);
        out.insert(QString::fromLatin1(kOutputCurrentId), m_pendingFeedback);

        ExecutionToken tok;
        tok.data = out;
        outputs.push_back(std::move(tok));
    }

    return outputs;
}

QJsonObject LoopUntilNode::saveState() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("maxIterations"), m_maxIterations);
    obj.insert(QStringLiteral("pending_feedback"), m_pendingFeedback);
    obj.insert(QStringLiteral("has_pending"), m_hasPendingFeedback);
    return obj;
}

void LoopUntilNode::loadState(const QJsonObject& data)
{
    if (data.contains(QStringLiteral("maxIterations"))) {
        setMaxIterations(data.value(QStringLiteral("maxIterations")).toInt());
    }
    if (data.contains(QStringLiteral("pending_feedback"))) {
        m_pendingFeedback = data.value(QStringLiteral("pending_feedback")).toString();
    } else if (data.contains(QStringLiteral("latchedData"))) {
        // Backward compatibility with older state key
        m_pendingFeedback = data.value(QStringLiteral("latchedData")).toString();
    }
    if (data.contains(QStringLiteral("has_pending"))) {
        m_hasPendingFeedback = data.value(QStringLiteral("has_pending")).toBool();
    } else {
        m_hasPendingFeedback = !m_pendingFeedback.isEmpty();
    }
}
