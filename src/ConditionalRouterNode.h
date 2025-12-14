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

#include "IToolConnector.h"
#include "CommonDataTypes.h"

/**
 * @brief Conditional router node implementing an "if/else" style control flow.
 *
 * Inputs (all type "text"):
 *  - in: payload to be forwarded unchanged
 *  - condition: string condition to evaluate ("true"/"false" oriented)
 *
 * Outputs (all type "text"):
 *  - true: receives payload when condition is considered true
 *  - false: receives payload otherwise
 */
class ConditionalRouterNode : public QObject, public IToolConnector {
    Q_OBJECT
    Q_INTERFACES(IToolConnector)

public:
    explicit ConditionalRouterNode(QObject* parent = nullptr);
    ~ConditionalRouterNode() override = default;

    // IToolConnector interface (V3 tokens API)
    NodeDescriptor getDescriptor() const override;
    QWidget* createConfigurationWidget(QWidget* parent) override;
    TokenList execute(const TokenList& incomingTokens) override;
    QJsonObject saveState() const override;
    void loadState(const QJsonObject& data) override;

    // Scheduling predicate: ready when main Data input is present; condition is optional.
    bool isReady(const QVariantMap& inputs, int incomingConnectionsCount) const override;

    // Pin identifiers (text-only data flow)
    static constexpr const char* kInputDataId = "in";
    static constexpr const char* kInputConditionId = "condition";
    static constexpr const char* kOutputTrueId = "true";
    static constexpr const char* kOutputFalseId = "false";

    // Accessor exposes the current router mode as a string token for the UI: "false"/"true"/"wait"
    QString defaultCondition() const {
        switch (m_routerMode) {
        case 1: return QStringLiteral("true");
        case 2: return QStringLiteral("wait");
        case 0:
        default:
            return QStringLiteral("false");
        }
    }

public slots:
    // UI slot mapping dropdown selection to internal router mode (0=false, 1=true, 2=wait)
    void setDefaultCondition(const QString& condition);

signals:
    void defaultConditionChanged(const QString& condition);

private:
    // Helper to check whether a given condition string is considered "true".
    static bool isConditionTrue(const QString& value);

    // Router behavior mode:
    // 0: Default to False (Immediate execution)
    // 1: Default to True (Immediate execution)
    // 2: Wait for Signal (Synchronized execution)
    int m_routerMode { 0 };
};
