#include "TransformScopeNode.h"
#include "TransformScopePropertiesWidget.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>

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

void addOutput(NodeDescriptor& desc, const QString& id, const QString& name)
{
    PinDefinition pin;
    pin.direction = PinDirection::Output;
    pin.id = id;
    pin.name = name;
    pin.type = QStringLiteral("text");
    desc.outputPins.insert(pin.id, pin);
}

QVariantMap historyEntry(int attempt, const ScopeBodyResult& body)
{
    QVariantMap entry;
    entry.insert(QStringLiteral("attempt"), attempt);
    entry.insert(QStringLiteral("accepted"), body.accepted);
    entry.insert(QStringLiteral("output"), body.output);
    entry.insert(QStringLiteral("next_input"), body.nextInput);
    entry.insert(QStringLiteral("message"), body.message);
    entry.insert(QStringLiteral("status"), body.status);
    return entry;
}
}

TransformScopeNode::TransformScopeNode(QObject* parent)
    : QObject(parent)
    , m_bodyId(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

NodeDescriptor TransformScopeNode::getDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("transform-scope");
    desc.name = QStringLiteral("Transform Scope");
    desc.category = QStringLiteral("Control Flow");

    addInput(desc, QString::fromLatin1(kInputContextId), QStringLiteral("Context"));
    addInput(desc, QString::fromLatin1(kInputInputId), QStringLiteral("Input"));
    addOutput(desc, QString::fromLatin1(kOutputContextId), QStringLiteral("Context"));
    addOutput(desc, QString::fromLatin1(kOutputErrorId), QStringLiteral("Error"));
    addOutput(desc, QString::fromLatin1(kOutputOutputId), QStringLiteral("Output"));
    addOutput(desc, QString::fromLatin1(kOutputStatusId), QStringLiteral("Status"));
    addOutput(desc, QString::fromLatin1(kOutputTextId), QStringLiteral("Text"));

    return desc;
}

QWidget* TransformScopeNode::createConfigurationWidget(QWidget* parent)
{
    return new TransformScopePropertiesWidget(this, parent);
}

bool TransformScopeNode::isReady(const QVariantMap& inputs, int incomingConnectionsCount) const
{
    Q_UNUSED(incomingConnectionsCount);
    return inputs.contains(QString::fromLatin1(kInputInputId));
}

TokenList TransformScopeNode::execute(const TokenList& incomingTokens)
{
    DataPacket inputs;
    for (const auto& token : incomingTokens) {
        for (auto it = token.data.cbegin(); it != token.data.cend(); ++it) {
            inputs.insert(it.key(), it.value());
        }
    }

    if (!m_bodyRunner) {
        return errorOutput(QStringLiteral("Transform Scope has no body runner configured."));
    }

    QVariant currentInput = inputs.value(QString::fromLatin1(kInputInputId));
    QVariant lastOutput;
    QVariantMap context = scopeVariantToMap(inputs.value(QString::fromLatin1(kInputContextId)));
    QVariantList history;
    QString status = QStringLiteral("completed");
    QString message;

    const bool retry = (m_mode == QStringLiteral("retry_until_accepted"));
    const int limit = retry ? qMax(1, m_maxAttempts) : 1;

    for (int attempt = 0; attempt < limit; ++attempt) {
        setLastStatus(QStringLiteral("Attempt %1/%2").arg(attempt + 1).arg(limit));
        const ScopeFrame frame = makeFrame(inputs, currentInput, lastOutput, attempt, context, history);
        ScopeBodyResult body = m_bodyRunner(m_bodyId, ScopeBodyKind::Transform, frame, inputs);
        if (!body.ok) {
            return errorOutput(body.error.isEmpty() ? QStringLiteral("Transform body failed.") : body.error);
        }

        scopeMergeMap(context, body.context);
        lastOutput = scopePreferredValue(body.output, currentInput);
        history.append(historyEntry(attempt, body));
        message = body.message;

        if (!retry || body.accepted) {
            status = retry ? QStringLiteral("accepted") : (body.accepted ? QStringLiteral("completed") : QStringLiteral("completed_unaccepted"));
            break;
        }

        currentInput = scopePreferredValue(body.nextInput, lastOutput);
        status = QStringLiteral("exhausted");
    }

    QVariantMap scopeInfo;
    scopeInfo.insert(QStringLiteral("body_id"), m_bodyId);
    scopeInfo.insert(QStringLiteral("kind"), scopeBodyKindToString(ScopeBodyKind::Transform));
    scopeInfo.insert(QStringLiteral("mode"), m_mode);
    scopeInfo.insert(QStringLiteral("status"), status);
    scopeInfo.insert(QStringLiteral("attempts"), history.size());
    scopeInfo.insert(QStringLiteral("message"), message);
    scopeInfo.insert(QStringLiteral("history"), history);

    QVariantMap outputContext = context;
    outputContext.insert(QStringLiteral("_scope"), scopeInfo);

    DataPacket output;
    output.insert(QString::fromLatin1(kOutputOutputId), lastOutput);
    output.insert(QString::fromLatin1(kOutputTextId), lastOutput);
    output.insert(QString::fromLatin1(kOutputContextId), outputContext);
    output.insert(QString::fromLatin1(kOutputStatusId), status);
    output.insert(QString::fromLatin1(kOutputErrorId), QString());

    setLastStatus(status);
    ExecutionToken token;
    token.data = output;
    return TokenList{std::move(token)};
}

TokenList TransformScopeNode::errorOutput(const QString& message)
{
    setLastStatus(QStringLiteral("error: %1").arg(message));

    QVariantMap scopeInfo;
    scopeInfo.insert(QStringLiteral("body_id"), m_bodyId);
    scopeInfo.insert(QStringLiteral("kind"), scopeBodyKindToString(ScopeBodyKind::Transform));
    scopeInfo.insert(QStringLiteral("mode"), m_mode);
    scopeInfo.insert(QStringLiteral("status"), QStringLiteral("error"));
    scopeInfo.insert(QStringLiteral("message"), message);

    DataPacket output;
    output.insert(QStringLiteral("__error"), message);
    output.insert(QStringLiteral("message"), message);
    output.insert(QString::fromLatin1(kOutputErrorId), message);
    output.insert(QString::fromLatin1(kOutputStatusId), QStringLiteral("error"));
    output.insert(QString::fromLatin1(kOutputContextId), QVariantMap{{QStringLiteral("_scope"), scopeInfo}});

    ExecutionToken token;
    token.data = output;
    return TokenList{std::move(token)};
}

ScopeFrame TransformScopeNode::makeFrame(const DataPacket& inputs,
                                         const QVariant& currentInput,
                                         const QVariant& previousOutput,
                                         int attempt,
                                         const QVariantMap& context,
                                         const QVariantList& history) const
{
    Q_UNUSED(inputs);
    ScopeFrame frame;
    frame.bodyId = m_bodyId;
    frame.kind = ScopeBodyKind::Transform;
    frame.activationId = QUuid::createUuid();
    frame.attempt = attempt;
    frame.input = currentInput;
    frame.item = currentInput;
    frame.previousOutput = previousOutput;
    frame.context = context;
    frame.history = history;
    return frame;
}

QJsonObject TransformScopeNode::saveState() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("body_id"), m_bodyId);
    obj.insert(QStringLiteral("mode"), m_mode);
    obj.insert(QStringLiteral("max_attempts"), m_maxAttempts);
    return obj;
}

void TransformScopeNode::loadState(const QJsonObject& data)
{
    const QString bodyId = data.value(QStringLiteral("body_id")).toString();
    if (!bodyId.trimmed().isEmpty()) {
        m_bodyId = bodyId;
    }
    setMode(data.value(QStringLiteral("mode")).toString(m_mode));
    if (data.contains(QStringLiteral("max_attempts"))) {
        setMaxAttempts(data.value(QStringLiteral("max_attempts")).toInt(m_maxAttempts));
    }
}

void TransformScopeNode::setBodyRunner(ScopeBodyRunner runner)
{
    m_bodyRunner = std::move(runner);
}

void TransformScopeNode::setMode(const QString& mode)
{
    QString normalized = mode.trimmed().toLower();
    if (normalized != QStringLiteral("run_once") &&
        normalized != QStringLiteral("retry_until_accepted")) {
        normalized = QStringLiteral("run_once");
    }
    if (normalized == m_mode) {
        emit modeChanged(m_mode);
        return;
    }
    m_mode = normalized;
    emit modeChanged(m_mode);
}

void TransformScopeNode::setMaxAttempts(int value)
{
    const int clamped = qBound(1, value, 1000);
    if (clamped == m_maxAttempts) {
        emit maxAttemptsChanged(m_maxAttempts);
        return;
    }
    m_maxAttempts = clamped;
    emit maxAttemptsChanged(m_maxAttempts);
}

void TransformScopeNode::requestOpenBody()
{
    emit openBodyRequested(m_bodyId, QStringLiteral("Transform Body %1").arg(m_bodyId.left(8)));
}

void TransformScopeNode::setLastStatus(const QString& status)
{
    if (m_lastStatus == status) {
        return;
    }
    m_lastStatus = status;
    emit statusChanged(m_lastStatus);
}
