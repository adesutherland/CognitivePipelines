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

#pragma once

#include <QObject>
#include <QWidget>
#include <QString>
#include <QVariant>
#include <deque>

#include "IToolConnector.h"
#include "CommonDataTypes.h"

class LoopUntilPropertiesWidget;

/**
 * @brief LoopUntilNode implements an adversarial/optimization loop controller.
 *
 * Pins:
 *  - Inputs:
 *      start     (any): Initial data
 *      feedback  (any): New/updated data from the loop body
 *      condition (text/any): Stop condition (truthy => stop)
 *  - Outputs:
 *      current   (any): Current attempt (emitted while looping)
 *      result    (any): Final result (when condition true or max iterations reached)
 *
 * Properties:
 *  - Max Iterations (default 10)
 */
class LoopUntilNode : public QObject, public IToolConnector {
    Q_OBJECT
    Q_INTERFACES(IToolConnector)

public:
    explicit LoopUntilNode(QObject* parent = nullptr);
    ~LoopUntilNode() override = default;

    // IToolConnector
    NodeDescriptor getDescriptor() const override;
    QWidget* createConfigurationWidget(QWidget* parent) override;
    TokenList execute(const TokenList& incomingTokens) override;
    QJsonObject saveState() const override;
    void loadState(const QJsonObject& data) override;

    // Scheduling predicate: ready when any of Start, Feedback, or Condition is present (OR semantics).
    bool isReady(const QVariantMap& inputs, int incomingConnectionsCount) const override;

    // Pin identifiers
    static constexpr const char* kInputStartId = "start";
    static constexpr const char* kInputFeedbackId = "feedback";
    static constexpr const char* kInputConditionId = "condition";
    static constexpr const char* kOutputCurrentId = "current";
    static constexpr const char* kOutputResultId = "result";

    // Accessors
    int maxIterations() const { return m_maxIterations; }

public slots:
    void setMaxIterations(int value);

signals:
    void maxIterationsChanged(int value);

private:
    static bool isTruthy(const QVariant& v);

private:
    int m_maxIterations {10};
    int m_iterationCount {0};
    bool m_isProcessing {false};
    std::deque<QVariant> m_taskQueue;
    QVariant m_lastIngestedStart;
    bool m_hasLastIngestedStart {false};
    bool m_hasLastEvaluatedCondition {false};

    // Event-driven latch state (persisted): feedback payload waiting for a condition trigger
    QString m_pendingFeedback;
    bool    m_hasPendingFeedback {false};
};
