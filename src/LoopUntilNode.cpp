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

    const bool hasStart = inputs.contains(startKey);
    const bool hasFeedback = inputs.contains(feedbackKey);
    const bool hasCondition = inputs.contains(condKey);

    // Always allow scheduling when a Condition value is present
    if (hasCondition) {
        return true;
    }

    // Allow Start to schedule only when it's the first iteration of a run,
    // or when the Start value actually changed (begin a new run).
    bool startChanged = false;
    if (hasStart) {
        const QString inStart = inputs.value(startKey).toString();
        // Compare against the last Start value we processed, not the feedback latch
        startChanged = (!m_hasLastStartValue) || (inStart != m_lastStartValue);
        if (m_isFirstIteration || startChanged) {
            return true;
        }
    }

    // Allow Feedback-only scheduling only after we've observed at least one
    // Condition evaluation in this run (prevents premature ticks that drain the queue).
    if (hasFeedback && m_hasLastEvaluatedCondition) {
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

    // Extract the triggering pin from the token (set by ExecutionEngine)
    // This tells us which pin received a FRESH value vs. stale data lake values
    QString triggeringPin;
    for (const auto& tok : incomingTokens) {
        if (!tok.triggeringPinId.isEmpty()) {
            triggeringPin = tok.triggeringPinId;
            break;
        }
    }

    // Determine which pins are present in the snapshot (Engine provides a snapshot per trigger)
    // conditionPresent logic:
    // - True if condition is the triggering pin (fresh condition value)
    // - True if feedback is the triggering pin AND condition exists in snapshot (evaluate with new feedback)
    // - True if no triggering info (backward compatibility for unit tests)
    bool startPresent = false;
    bool feedbackPresent = false;
    bool conditionInSnapshot = false;  // True if condition value exists in snapshot
    QVariant startVal;
    QVariant feedbackVal;
    QVariant condVal;

    // Backward compatibility: if no triggeringPin is set, treat all pins as fresh
    const bool noTriggeringInfo = triggeringPin.isEmpty();
    const bool conditionIsTrigger = (triggeringPin == condKey);

    for (const auto& tok : incomingTokens) {
        const auto& d = tok.data;
        if (d.contains(startKey)) { startPresent = true; startVal = d.value(startKey); }
        if (d.contains(feedbackKey)) { feedbackPresent = true; feedbackVal = d.value(feedbackKey); }
        if (d.contains(condKey)) {
            const QVariant v = d.value(condKey);
            if (v.isValid() && !v.isNull()) {
                if (v.typeId() == QMetaType::QString) {
                    if (!v.toString().trimmed().isEmpty()) {
                        conditionInSnapshot = true;
                        condVal = v;
                    }
                } else {
                    conditionInSnapshot = true;
                    condVal = v;
                }
            }
        }
    }

    // Condition should be evaluated when:
    // 1. Condition is the triggering pin (fresh condition), OR
    // 2. No triggering info (backward compat for unit tests) AND condition exists
    // NOTE: We do NOT evaluate when feedback is the trigger - the condition in the
    // snapshot would be stale (from a previous iteration). We must wait for a fresh
    // condition value to arrive.
    const bool conditionPresent = conditionInSnapshot && 
        (noTriggeringInfo || conditionIsTrigger);

    TokenList outputs;

    // Priority 1: Start handling — gate by first-iteration flag OR new Start value
    // Rationale: Engine snapshots include Start on every tick. We only kick on:
    //  - the very first iteration of a run, or
    //  - when the Start value changes (treated as beginning a new run).
    // Compare against the last Start value we processed, not the feedback latch
    const bool startChanged = startPresent && (!m_hasLastStartValue || startVal.toString() != m_lastStartValue);
    if (startPresent && (m_isFirstIteration || startChanged)) {
        m_iterationCount = 0;
        m_pendingFeedback = startVal.toString();
        m_hasPendingFeedback = true; // seed latch with start value for subsequent condition-only triggers
        m_isFirstIteration = false;  // only the first Start should kick off immediately
        // Reset condition tracking at the beginning of a run
        m_hasLastEvaluatedCondition = false;
        // Remember the Start value we processed to detect actual changes
        m_lastStartValue = startVal.toString();
        m_hasLastStartValue = true;

        DataPacket out;
        out.insert(QStringLiteral("text"), m_pendingFeedback);
        out.insert(QString::fromLatin1(kOutputCurrentId), m_pendingFeedback);
        ExecutionToken tok; tok.data = out;
        outputs.push_back(std::move(tok));
        return outputs;
    }

    // Priority 2: Feedback token → latch only
    if (feedbackPresent) {
        m_pendingFeedback = feedbackVal.toString();
        m_hasPendingFeedback = true;
        // Do not emit yet
    }

    // Priority 3: Condition token → trigger if latch is present
    if (conditionPresent) {
        if (!m_hasPendingFeedback) {
            return outputs; // nothing to emit
        }

        const bool stopNow = isTruthy(condVal);

        const bool finalStop = stopNow || ((m_iterationCount + 1) >= m_maxIterations);

        DataPacket out;
        out.insert(QStringLiteral("text"), m_pendingFeedback);
        if (finalStop) {
            out.insert(QString::fromLatin1(kOutputResultId), m_pendingFeedback);
            m_hasPendingFeedback = false; // consume latch on stop
            m_isFirstIteration = true;    // prepare for a new run
            m_hasLastEvaluatedCondition = false; // reset condition tracking
            m_hasLastStartValue = false;  // reset start tracking for new run
        } else {
            out.insert(QString::fromLatin1(kOutputCurrentId), m_pendingFeedback);
            ++m_iterationCount; // increment only during loop iterations
            // keep latch for next round
            m_hasLastEvaluatedCondition = true;
        }

        ExecutionToken tok; tok.data = out;
        outputs.push_back(std::move(tok));
        return outputs;
    }

    // No trigger to emit (maybe feedback latched) → return empty
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
    // First-iteration flag is runtime-only; ensure it's set when node loads
    m_isFirstIteration = true;
}
