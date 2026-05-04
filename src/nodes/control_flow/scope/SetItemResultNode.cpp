#include "SetItemResultNode.h"
#include "ScopeRuntime.h"

#include <QJsonObject>
#include <QLabel>

namespace {
void addInput(NodeDescriptor& desc, const QString& id, const QString& name)
{
    PinDefinition pin;
    pin.direction = PinDirection::Input;
    pin.id = id;
    pin.name = name;
    pin.type = QStringLiteral("text");
    desc.inputPins.insert(pin.id, pin);
}
}

SetItemResultNode::SetItemResultNode(QObject* parent)
    : QObject(parent)
{
}

NodeDescriptor SetItemResultNode::getDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("iterator-set-result");
    desc.name = QStringLiteral("Set Item Result");
    desc.category = QStringLiteral("Control Flow");

    addInput(desc, QString::fromLatin1(kInputContextId), QStringLiteral("Context"));
    addInput(desc, QString::fromLatin1(kInputErrorId), QStringLiteral("Error"));
    addInput(desc, QString::fromLatin1(kInputMessageId), QStringLiteral("Message"));
    addInput(desc, QString::fromLatin1(kInputResultId), QStringLiteral("Result"));
    addInput(desc, QString::fromLatin1(kInputSkipId), QStringLiteral("Skip"));

    return desc;
}

QWidget* SetItemResultNode::createConfigurationWidget(QWidget* parent)
{
    auto* label = new QLabel(QObject::tr("Finishes one Iterator Scope body pass and returns the current item's result."), parent);
    label->setWordWrap(true);
    return label;
}

bool SetItemResultNode::isReady(const QVariantMap& inputs, int incomingConnectionsCount) const
{
    return incomingConnectionsCount > 0 && static_cast<int>(inputs.size()) >= incomingConnectionsCount;
}

TokenList SetItemResultNode::execute(const TokenList& incomingTokens)
{
    DataPacket inputs;
    for (const auto& token : incomingTokens) {
        for (auto it = token.data.cbegin(); it != token.data.cend(); ++it) {
            inputs.insert(it.key(), it.value());
        }
    }

    const QString resultKey = QString::fromLatin1(kInputResultId);
    const QString error = inputs.value(QString::fromLatin1(kInputErrorId)).toString().trimmed();

    QVariantMap result;
    result.insert(QStringLiteral("output"), inputs.value(resultKey));
    result.insert(QStringLiteral("skip"), scopeVariantTruthy(inputs.value(QString::fromLatin1(kInputSkipId))));
    result.insert(QStringLiteral("context"), scopeVariantToMap(inputs.value(QString::fromLatin1(kInputContextId))));
    result.insert(QStringLiteral("message"), inputs.value(QString::fromLatin1(kInputMessageId)).toString());
    result.insert(QStringLiteral("error"), error);

    DataPacket output;
    output.insert(QString::fromLatin1(kOutputBodyResultId), result);
    output.insert(QStringLiteral("result"), inputs.value(resultKey));
    output.insert(QStringLiteral("text"), inputs.value(resultKey));
    if (!error.isEmpty()) {
        output.insert(QStringLiteral("__error"), error);
        output.insert(QStringLiteral("message"), error);
    }

    ExecutionToken token;
    token.data = output;
    return TokenList{std::move(token)};
}

QJsonObject SetItemResultNode::saveState() const
{
    return {};
}

void SetItemResultNode::loadState(const QJsonObject& data)
{
    Q_UNUSED(data);
}
