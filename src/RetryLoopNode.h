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
#include <QVariant>
#include <QString>
#include <deque>
#include "IToolConnector.h"

/**
 * @brief RetryLoopNode acts as a "Reliability Supervisor" that retries a task if worker feedback indicates failure.
 */
class RetryLoopNode : public QObject, public IToolConnector {
    Q_OBJECT
    Q_INTERFACES(IToolConnector)

public:
    explicit RetryLoopNode(QObject* parent = nullptr);
    ~RetryLoopNode() override = default;

    // IToolConnector interface
    NodeDescriptor getDescriptor() const override;
    QWidget* createConfigurationWidget(QWidget* parent) override;
    TokenList execute(const TokenList& incomingTokens) override;
    QJsonObject saveState() const override;
    void loadState(const QJsonObject& data) override;

    bool isReady(const QVariantMap& inputs, int incomingConnectionsCount) const override;

    // Getters and Setters
    QString getFailureString() const { return m_failureString; }
    void setFailureString(const QString& value);

    int getMaxRetries() const { return m_maxRetries; }
    void setMaxRetries(int value);

signals:
    void failureStringChanged(const QString& value);
    void maxRetriesChanged(int value);

public:
    // Pin identifiers
    static constexpr const char* kInputTaskId = "task_in";
    static constexpr const char* kOutputWorkerInstructionId = "worker_instruction";
    static constexpr const char* kInputWorkerFeedbackId = "worker_feedback";
    static constexpr const char* kOutputVerifiedResultId = "verified_result";

private:
    std::deque<QVariant> m_taskQueue;
    bool m_isProcessing {false};
    QVariant m_cachedPayload;
    int m_retryCount {0};
    int m_maxRetries {3};
    QString m_failureString {QStringLiteral("FAIL")};
};
