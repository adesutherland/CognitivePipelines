#include "GetItemNode.h"

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

GetItemNode::GetItemNode(QObject* parent)
    : QObject(parent)
{
}

NodeDescriptor GetItemNode::getDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("iterator-get-item");
    desc.name = QStringLiteral("Get Item");
    desc.category = QStringLiteral("Control Flow");

    addOutput(desc, QString::fromLatin1(kOutputContextId), QStringLiteral("Context"));
    addOutput(desc, QString::fromLatin1(kOutputCountId), QStringLiteral("Count"));
    addOutput(desc, QString::fromLatin1(kOutputHistoryId), QStringLiteral("History"));
    addOutput(desc, QString::fromLatin1(kOutputIndexId), QStringLiteral("Index"));
    addOutput(desc, QString::fromLatin1(kOutputItemId), QStringLiteral("Item"));
    addOutput(desc, QString::fromLatin1(kOutputTextId), QStringLiteral("Text"));

    return desc;
}

QWidget* GetItemNode::createConfigurationWidget(QWidget* parent)
{
    auto* label = new QLabel(QObject::tr("Reads the current Iterator Scope item."), parent);
    label->setWordWrap(true);
    return label;
}

TokenList GetItemNode::execute(const TokenList& incomingTokens)
{
    DataPacket input;
    for (const auto& token : incomingTokens) {
        for (auto it = token.data.cbegin(); it != token.data.cend(); ++it) {
            input.insert(it.key(), it.value());
        }
    }

    const QVariant item = firstValue(input, QStringLiteral("_iterator_item"));

    DataPacket output;
    output.insert(QString::fromLatin1(kOutputItemId), item);
    output.insert(QString::fromLatin1(kOutputTextId), item);
    output.insert(QString::fromLatin1(kOutputIndexId), firstValue(input, QStringLiteral("_iterator_index")));
    output.insert(QString::fromLatin1(kOutputCountId), firstValue(input, QStringLiteral("_iterator_count")));
    output.insert(QString::fromLatin1(kOutputContextId), firstValue(input, QStringLiteral("_scope_context")));
    output.insert(QString::fromLatin1(kOutputHistoryId), firstValue(input, QStringLiteral("_scope_history")));

    ExecutionToken token;
    token.data = output;
    return TokenList{std::move(token)};
}

QJsonObject GetItemNode::saveState() const
{
    return {};
}

void GetItemNode::loadState(const QJsonObject& data)
{
    Q_UNUSED(data);
}
