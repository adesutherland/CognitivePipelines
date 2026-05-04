#include "IteratorScopeNode.h"
#include "IteratorScopePropertiesWidget.h"

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

QVariantMap historyEntry(int index, const ScopeBodyResult& body)
{
    QVariantMap entry;
    entry.insert(QStringLiteral("index"), index);
    entry.insert(QStringLiteral("skip"), body.skip);
    entry.insert(QStringLiteral("output"), body.output);
    entry.insert(QStringLiteral("message"), body.message);
    entry.insert(QStringLiteral("status"), body.status);
    return entry;
}

QVariantMap errorEntry(int index, const QVariant& item, const QString& error)
{
    QVariantMap entry;
    entry.insert(QStringLiteral("index"), index);
    entry.insert(QStringLiteral("item"), item);
    entry.insert(QStringLiteral("error"), error);
    return entry;
}
}

IteratorScopeNode::IteratorScopeNode(QObject* parent)
    : QObject(parent)
    , m_bodyId(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

NodeDescriptor IteratorScopeNode::getDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("iterator-scope");
    desc.name = QStringLiteral("Iterator Scope");
    desc.category = QStringLiteral("Control Flow");

    addInput(desc, QString::fromLatin1(kInputContextId), QStringLiteral("Context"));
    addInput(desc, QString::fromLatin1(kInputItemsId), QStringLiteral("Items"));
    addOutput(desc, QString::fromLatin1(kOutputContextId), QStringLiteral("Context"));
    addOutput(desc, QString::fromLatin1(kOutputErrorsId), QStringLiteral("Errors"));
    addOutput(desc, QString::fromLatin1(kOutputResultsId), QStringLiteral("Results"));
    addOutput(desc, QString::fromLatin1(kOutputStatusId), QStringLiteral("Status"));
    addOutput(desc, QString::fromLatin1(kOutputSummaryId), QStringLiteral("Summary"));
    addOutput(desc, QString::fromLatin1(kOutputTextId), QStringLiteral("Text"));

    return desc;
}

QWidget* IteratorScopeNode::createConfigurationWidget(QWidget* parent)
{
    return new IteratorScopePropertiesWidget(this, parent);
}

bool IteratorScopeNode::isReady(const QVariantMap& inputs, int incomingConnectionsCount) const
{
    Q_UNUSED(incomingConnectionsCount);
    return inputs.contains(QString::fromLatin1(kInputItemsId));
}

TokenList IteratorScopeNode::execute(const TokenList& incomingTokens)
{
    DataPacket inputs;
    for (const auto& token : incomingTokens) {
        for (auto it = token.data.cbegin(); it != token.data.cend(); ++it) {
            inputs.insert(it.key(), it.value());
        }
    }

    if (!m_bodyRunner) {
        return errorOutput(QStringLiteral("Iterator Scope has no body runner configured."));
    }

    const QVariant itemsValue = inputs.value(QString::fromLatin1(kInputItemsId));
    const QVariantList items = scopeVariantToList(itemsValue);
    QVariantMap context = scopeVariantToMap(inputs.value(QString::fromLatin1(kInputContextId)));
    QVariantList history;
    QVariantList results;
    QVariantList errors;
    int skipped = 0;

    for (int index = 0; index < items.size(); ++index) {
        const QVariant item = items.at(index);
        setLastStatus(QStringLiteral("Item %1/%2").arg(index + 1).arg(items.size()));

        const ScopeFrame frame = makeFrame(item, index, items.size(), context, history);
        ScopeBodyResult body = m_bodyRunner(m_bodyId, ScopeBodyKind::Iterator, frame, inputs);
        if (!body.ok) {
            const QString message = body.error.isEmpty() ? QStringLiteral("Iterator body failed.") : body.error;
            errors.append(errorEntry(index, item, message));

            if (m_failurePolicy == QStringLiteral("stop")) {
                return errorOutput(message, errors);
            }
            if (m_failurePolicy == QStringLiteral("include_error")) {
                results.append(QVariantMap{
                    {QStringLiteral("__error"), message},
                    {QStringLiteral("index"), index},
                    {QStringLiteral("item"), item}
                });
            }
            continue;
        }

        scopeMergeMap(context, body.context);
        history.append(historyEntry(index, body));

        if (body.skip) {
            ++skipped;
            continue;
        }
        results.append(scopePreferredValue(body.output, item));
    }

    QVariantMap summary;
    summary.insert(QStringLiteral("body_id"), m_bodyId);
    summary.insert(QStringLiteral("kind"), scopeBodyKindToString(ScopeBodyKind::Iterator));
    summary.insert(QStringLiteral("status"), QStringLiteral("completed"));
    summary.insert(QStringLiteral("input_count"), items.size());
    summary.insert(QStringLiteral("result_count"), results.size());
    summary.insert(QStringLiteral("skipped"), skipped);
    summary.insert(QStringLiteral("error_count"), errors.size());
    summary.insert(QStringLiteral("failure_policy"), m_failurePolicy);
    summary.insert(QStringLiteral("history"), history);

    QVariantMap outputContext = context;
    outputContext.insert(QStringLiteral("_scope"), summary);

    DataPacket output;
    output.insert(QString::fromLatin1(kOutputResultsId), results);
    output.insert(QString::fromLatin1(kOutputTextId), results);
    output.insert(QString::fromLatin1(kOutputContextId), outputContext);
    output.insert(QString::fromLatin1(kOutputSummaryId), summary);
    output.insert(QString::fromLatin1(kOutputErrorsId), errors);
    output.insert(QString::fromLatin1(kOutputStatusId), QStringLiteral("completed"));

    setLastStatus(QStringLiteral("completed"));
    ExecutionToken token;
    token.data = output;
    return TokenList{std::move(token)};
}

TokenList IteratorScopeNode::errorOutput(const QString& message, const QVariantList& errors)
{
    setLastStatus(QStringLiteral("error: %1").arg(message));

    QVariantMap summary;
    summary.insert(QStringLiteral("body_id"), m_bodyId);
    summary.insert(QStringLiteral("kind"), scopeBodyKindToString(ScopeBodyKind::Iterator));
    summary.insert(QStringLiteral("status"), QStringLiteral("error"));
    summary.insert(QStringLiteral("message"), message);
    summary.insert(QStringLiteral("error_count"), errors.size());
    summary.insert(QStringLiteral("failure_policy"), m_failurePolicy);

    DataPacket output;
    output.insert(QStringLiteral("__error"), message);
    output.insert(QStringLiteral("message"), message);
    output.insert(QString::fromLatin1(kOutputErrorsId), errors);
    output.insert(QString::fromLatin1(kOutputStatusId), QStringLiteral("error"));
    output.insert(QString::fromLatin1(kOutputSummaryId), summary);
    output.insert(QString::fromLatin1(kOutputContextId), QVariantMap{{QStringLiteral("_scope"), summary}});

    ExecutionToken token;
    token.data = output;
    return TokenList{std::move(token)};
}

ScopeFrame IteratorScopeNode::makeFrame(const QVariant& item,
                                        int index,
                                        int count,
                                        const QVariantMap& context,
                                        const QVariantList& history) const
{
    ScopeFrame frame;
    frame.bodyId = m_bodyId;
    frame.kind = ScopeBodyKind::Iterator;
    frame.activationId = QUuid::createUuid();
    frame.index = index;
    frame.count = count;
    frame.item = item;
    frame.input = item;
    frame.context = context;
    frame.history = history;
    return frame;
}

QJsonObject IteratorScopeNode::saveState() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("body_id"), m_bodyId);
    obj.insert(QStringLiteral("failure_policy"), m_failurePolicy);
    return obj;
}

void IteratorScopeNode::loadState(const QJsonObject& data)
{
    const QString bodyId = data.value(QStringLiteral("body_id")).toString();
    if (!bodyId.trimmed().isEmpty()) {
        m_bodyId = bodyId;
    }
    setFailurePolicy(data.value(QStringLiteral("failure_policy")).toString(m_failurePolicy));
}

void IteratorScopeNode::setBodyRunner(ScopeBodyRunner runner)
{
    m_bodyRunner = std::move(runner);
}

void IteratorScopeNode::setFailurePolicy(const QString& policy)
{
    QString normalized = policy.trimmed().toLower();
    if (normalized != QStringLiteral("stop") &&
        normalized != QStringLiteral("skip") &&
        normalized != QStringLiteral("include_error")) {
        normalized = QStringLiteral("stop");
    }
    if (normalized == m_failurePolicy) {
        emit failurePolicyChanged(m_failurePolicy);
        return;
    }
    m_failurePolicy = normalized;
    emit failurePolicyChanged(m_failurePolicy);
}

void IteratorScopeNode::requestOpenBody()
{
    emit openBodyRequested(m_bodyId, QStringLiteral("Iterator Body %1").arg(m_bodyId.left(8)));
}

void IteratorScopeNode::setLastStatus(const QString& status)
{
    if (m_lastStatus == status) {
        return;
    }
    m_lastStatus = status;
    emit statusChanged(m_lastStatus);
}
