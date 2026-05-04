#include "GetInputNode.h"

#include <QJsonObject>
#include <QLabel>

namespace {
void addOutput(NodeDescriptor& desc, const QString& id, const QString& name)
{
    PinDefinition pin;
    pin.direction = PinDirection::Output;
    pin.id = id;
    pin.name = name;
    pin.type = QStringLiteral("text");
    desc.outputPins.insert(pin.id, pin);
}

QVariant firstValue(const DataPacket& packet, const QString& key)
{
    return packet.contains(key) ? packet.value(key) : QVariant{};
}
}

GetInputNode::GetInputNode(QObject* parent)
    : QObject(parent)
{
}

NodeDescriptor GetInputNode::getDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("scope-get-input");
    desc.name = QStringLiteral("Get Input");
    desc.category = QStringLiteral("Control Flow");

    addOutput(desc, QString::fromLatin1(kOutputAttemptId), QStringLiteral("Attempt"));
    addOutput(desc, QString::fromLatin1(kOutputContextId), QStringLiteral("Context"));
    addOutput(desc, QString::fromLatin1(kOutputHistoryId), QStringLiteral("History"));
    addOutput(desc, QString::fromLatin1(kOutputInputId), QStringLiteral("Input"));
    addOutput(desc, QString::fromLatin1(kOutputPreviousOutputId), QStringLiteral("Previous Output"));
    addOutput(desc, QString::fromLatin1(kOutputTextId), QStringLiteral("Text"));

    return desc;
}

QWidget* GetInputNode::createConfigurationWidget(QWidget* parent)
{
    auto* label = new QLabel(QObject::tr("Reads the current Transform Scope body input."), parent);
    label->setWordWrap(true);
    return label;
}

TokenList GetInputNode::execute(const TokenList& incomingTokens)
{
    DataPacket input;
    for (const auto& token : incomingTokens) {
        for (auto it = token.data.cbegin(); it != token.data.cend(); ++it) {
            input.insert(it.key(), it.value());
        }
    }

    const QVariant value = firstValue(input, QStringLiteral("_transform_input"));

    DataPacket output;
    output.insert(QString::fromLatin1(kOutputInputId), value);
    output.insert(QString::fromLatin1(kOutputTextId), value);
    output.insert(QString::fromLatin1(kOutputContextId), firstValue(input, QStringLiteral("_scope_context")));
    output.insert(QString::fromLatin1(kOutputAttemptId), firstValue(input, QStringLiteral("_transform_attempt")));
    output.insert(QString::fromLatin1(kOutputPreviousOutputId), firstValue(input, QStringLiteral("_transform_previous_output")));
    output.insert(QString::fromLatin1(kOutputHistoryId), firstValue(input, QStringLiteral("_scope_history")));

    ExecutionToken token;
    token.data = output;
    return TokenList{std::move(token)};
}

QJsonObject GetInputNode::saveState() const
{
    return {};
}

void GetInputNode::loadState(const QJsonObject& data)
{
    Q_UNUSED(data);
}
