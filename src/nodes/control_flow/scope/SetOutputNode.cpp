#include "SetOutputNode.h"
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

SetOutputNode::SetOutputNode(QObject* parent)
    : QObject(parent)
{
}

NodeDescriptor SetOutputNode::getDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("scope-set-output");
    desc.name = QStringLiteral("Set Output");
    desc.category = QStringLiteral("Control Flow");

    addInput(desc, QString::fromLatin1(kInputAcceptedId), QStringLiteral("Accepted"));
    addInput(desc, QString::fromLatin1(kInputContextId), QStringLiteral("Context"));
    addInput(desc, QString::fromLatin1(kInputErrorId), QStringLiteral("Error"));
    addInput(desc, QString::fromLatin1(kInputMessageId), QStringLiteral("Message"));
    addInput(desc, QString::fromLatin1(kInputNextInputId), QStringLiteral("Next Input"));
    addInput(desc, QString::fromLatin1(kInputOutputId), QStringLiteral("Output"));

    return desc;
}

QWidget* SetOutputNode::createConfigurationWidget(QWidget* parent)
{
    auto* label = new QLabel(QObject::tr("Finishes one Transform Scope body pass. Accepted=false asks the parent scope to retry."), parent);
    label->setWordWrap(true);
    return label;
}

bool SetOutputNode::isReady(const QVariantMap& inputs, int incomingConnectionsCount) const
{
    return incomingConnectionsCount > 0 && static_cast<int>(inputs.size()) >= incomingConnectionsCount;
}

TokenList SetOutputNode::execute(const TokenList& incomingTokens)
{
    DataPacket inputs;
    for (const auto& token : incomingTokens) {
        for (auto it = token.data.cbegin(); it != token.data.cend(); ++it) {
            inputs.insert(it.key(), it.value());
        }
    }

    const QString outputKey = QString::fromLatin1(kInputOutputId);
    const QString acceptedKey = QString::fromLatin1(kInputAcceptedId);
    const QString error = inputs.value(QString::fromLatin1(kInputErrorId)).toString().trimmed();

    QVariantMap result;
    result.insert(QStringLiteral("output"), inputs.value(outputKey));
    result.insert(QStringLiteral("accepted"), inputs.contains(acceptedKey) ? scopeVariantTruthy(inputs.value(acceptedKey)) : true);
    result.insert(QStringLiteral("next_input"), inputs.value(QString::fromLatin1(kInputNextInputId)));
    result.insert(QStringLiteral("context"), scopeVariantToMap(inputs.value(QString::fromLatin1(kInputContextId))));
    result.insert(QStringLiteral("message"), inputs.value(QString::fromLatin1(kInputMessageId)).toString());
    result.insert(QStringLiteral("error"), error);

    DataPacket output;
    output.insert(QString::fromLatin1(kOutputBodyResultId), result);
    output.insert(QStringLiteral("output"), inputs.value(outputKey));
    output.insert(QStringLiteral("text"), inputs.value(outputKey));
    if (!error.isEmpty()) {
        output.insert(QStringLiteral("__error"), error);
        output.insert(QStringLiteral("message"), error);
    }

    ExecutionToken token;
    token.data = output;
    return TokenList{std::move(token)};
}

QJsonObject SetOutputNode::saveState() const
{
    return {};
}

void SetOutputNode::loadState(const QJsonObject& data)
{
    Q_UNUSED(data);
}
