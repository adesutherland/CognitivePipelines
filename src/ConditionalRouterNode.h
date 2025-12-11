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

    // Pin identifiers (text-only data flow)
    static constexpr const char* kInputDataId = "in";
    static constexpr const char* kInputConditionId = "condition";
    static constexpr const char* kOutputTrueId = "true";
    static constexpr const char* kOutputFalseId = "false";

    // Accessor for default condition used when the condition input pin is empty
    QString defaultCondition() const { return m_defaultCondition; }

public slots:
    void setDefaultCondition(const QString& condition);

signals:
    void defaultConditionChanged(const QString& condition);

private:
    // Helper to check whether a given condition string is considered "true".
    static bool isConditionTrue(const QString& value);

    QString m_defaultCondition { QStringLiteral("false") };
};
