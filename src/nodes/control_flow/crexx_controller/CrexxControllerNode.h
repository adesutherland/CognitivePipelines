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

#include "CommonDataTypes.h"
#include "IToolNode.h"

#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QVariant>

#include <deque>

class CrexxControllerNode : public QObject, public IToolNode {
    Q_OBJECT
    Q_INTERFACES(IToolNode)

public:
    explicit CrexxControllerNode(QObject* parent = nullptr);
    ~CrexxControllerNode() override = default;

    static constexpr const char* kInputStartId = "start";
    static constexpr const char* kInputCreatorResultId = "from_creator";
    static constexpr const char* kInputValidatorResultId = "from_validator";
    static constexpr const char* kOutputCreatorId = "to_creator";
    static constexpr const char* kOutputValidatorId = "to_validator";
    static constexpr const char* kOutputDoneId = "done";
    static constexpr const char* kInputFeedbackId = "from_validator";
    static constexpr const char* kOutputNextId = "to_creator";

    static QString defaultScript();

    NodeDescriptor getDescriptor() const override;
    QWidget* createConfigurationWidget(QWidget* parent) override;
    TokenList execute(const TokenList& incomingTokens) override;
    QJsonObject saveState() const override;
    void loadState(const QJsonObject& data) override;
    bool isReady(const QVariantMap& inputs, int incomingConnectionsCount) const override;

    QString scriptCode() const;
    void setScriptCode(const QString& scriptCode);

    QString engineId() const;
    void setEngineId(const QString& engineId);

    int maxIterations() const;
    void setMaxIterations(int maxIterations);

signals:
    void scriptCodeChanged(const QString& scriptCode);
    void engineIdChanged(const QString& engineId);
    void maxIterationsChanged(int maxIterations);

private:
    struct ControllerDecision {
        QString decision;
        QVariant payload;
        QString status;
        QString error;
        QString logs;
    };

    ControllerDecision runControllerScript(const QString& phase,
                                           const QVariant& start,
                                           const QVariant& creatorResult,
                                           const QVariant& validatorResult,
                                           int iteration,
                                           const QVariant& lastCreatorPayload) const;
    ExecutionToken makeOutputToken(const ControllerDecision& decision, bool forceNext) const;
    QVariant payloadFallback(const ControllerDecision& decision, const QVariant& fallback) const;

    static QString variantToText(const QVariant& value);
    static QString normalizedRoute(const QString& value);
    static bool isStartTrigger(const ExecutionToken& token, const QString& startKey);
    static bool isCreatorTrigger(const ExecutionToken& token, const QString& creatorKey);
    static bool isValidatorTrigger(const ExecutionToken& token, const QString& validatorKey);

private:
    mutable QMutex m_mutex;
    QString m_scriptCode;
    QString m_engineId {QStringLiteral("crexx")};
    bool m_enableSyntaxHighlighting {true};
    int m_maxIterations {3};
    int m_iterationCount {0};
    bool m_isProcessing {false};
    std::deque<QVariant> m_startQueue;
    QVariant m_currentStart;
    QVariant m_lastCreatorPayload;
    QVariant m_lastCreatorResult;
    QVariant m_lastValidatorResult;
};
