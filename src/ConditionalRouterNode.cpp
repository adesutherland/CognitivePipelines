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

#include "ConditionalRouterNode.h"
#include "ConditionalRouterPropertiesWidget.h"

#include <QJsonObject>

ConditionalRouterNode::ConditionalRouterNode(QObject* parent)
    : QObject(parent)
{
}

void ConditionalRouterNode::setDefaultCondition(const QString& condition)
{
    if (m_defaultCondition == condition) {
        return;
    }
    m_defaultCondition = condition;
    emit defaultConditionChanged(m_defaultCondition);
}

NodeDescriptor ConditionalRouterNode::getDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("conditional-router");
    desc.name = QStringLiteral("Conditional Router");
    desc.category = QStringLiteral("Control Flow");

    // Input: in (Text)
    PinDefinition inData;
    inData.direction = PinDirection::Input;
    inData.id = QString::fromLatin1(kInputDataId);
    inData.name = QStringLiteral("Input");
    inData.type = QStringLiteral("text");
    desc.inputPins.insert(inData.id, inData);

    // Input: condition (Text)
    PinDefinition inCond;
    inCond.direction = PinDirection::Input;
    inCond.id = QString::fromLatin1(kInputConditionId);
    inCond.name = QStringLiteral("Condition");
    inCond.type = QStringLiteral("text");
    desc.inputPins.insert(inCond.id, inCond);

    // Output: true (Text)
    PinDefinition outTrue;
    outTrue.direction = PinDirection::Output;
    outTrue.id = QString::fromLatin1(kOutputTrueId);
    outTrue.name = QStringLiteral("True");
    outTrue.type = QStringLiteral("text");
    desc.outputPins.insert(outTrue.id, outTrue);

    // Output: false (Text)
    PinDefinition outFalse;
    outFalse.direction = PinDirection::Output;
    outFalse.id = QString::fromLatin1(kOutputFalseId);
    outFalse.name = QStringLiteral("False");
    outFalse.type = QStringLiteral("text");
    desc.outputPins.insert(outFalse.id, outFalse);

    return desc;
}

QWidget* ConditionalRouterNode::createConfigurationWidget(QWidget* parent)
{
    auto* widget = new ConditionalRouterPropertiesWidget(parent);

    // Initialize from current state
    widget->setDefaultCondition(m_defaultCondition);

    // UI -> Node
    QObject::connect(widget, &ConditionalRouterPropertiesWidget::defaultConditionChanged,
                     this, &ConditionalRouterNode::setDefaultCondition);

    // Node -> UI
    QObject::connect(this, &ConditionalRouterNode::defaultConditionChanged,
                     widget, &ConditionalRouterPropertiesWidget::setDefaultCondition);

    return widget;
}

TokenList ConditionalRouterNode::execute(const TokenList& incomingTokens)
{
    // Merge incoming tokens into a single DataPacket
    DataPacket inputs;
    for (const auto& token : incomingTokens) {
        for (auto it = token.data.cbegin(); it != token.data.cend(); ++it) {
            inputs.insert(it.key(), it.value());
        }
    }

    // Extract condition string from input pin (if provided)
    QString condition = inputs.value(QString::fromLatin1(kInputConditionId)).toString();
    if (condition.trimmed().isEmpty()) {
        condition = m_defaultCondition;
    }

    const bool isTrue = isConditionTrue(condition);

    // Build output payload: prefer text key, fall back to legacy data key, then pin id
    const QString inputPinKey = QString::fromLatin1(kInputDataId);

    QVariant dataPayload = inputs.value(QStringLiteral("text"));
    if (!dataPayload.isValid()) {
        dataPayload = inputs.value(QStringLiteral("data"));
    }
    if (!dataPayload.isValid()) {
        dataPayload = inputs.value(inputPinKey);
    }

    const QString activeOutputId = isTrue
        ? QString::fromLatin1(kOutputTrueId)
        : QString::fromLatin1(kOutputFalseId);

    DataPacket output;
    // Standard text payload for downstream nodes
    output.insert(QStringLiteral("text"), dataPayload);
    // Also store under the active output pin id so the engine can route
    output.insert(activeOutputId, dataPayload);

    ExecutionToken token;
    token.data = output;

    TokenList result;
    result.push_back(std::move(token));
    return result;
}

QJsonObject ConditionalRouterNode::saveState() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("defaultCondition"), m_defaultCondition);
    return obj;
}

void ConditionalRouterNode::loadState(const QJsonObject& data)
{
    if (data.contains(QStringLiteral("defaultCondition"))) {
        setDefaultCondition(data.value(QStringLiteral("defaultCondition")).toString());
    }
}

bool ConditionalRouterNode::isConditionTrue(const QString& value)
{
    const QString v = value.trimmed().toLower();
    return (v == QStringLiteral("true") ||
            v == QStringLiteral("1") ||
            v == QStringLiteral("yes") ||
            v == QStringLiteral("pass") ||
            v == QStringLiteral("ok"));
}
