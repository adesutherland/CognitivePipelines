//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//

#include "UniversalScriptNode.h"
#include "UniversalScriptPropertiesWidget.h"
#include "IScriptHost.h"
#include "ExecutionScriptHost.h"
#include "ExecutionToken.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include "Logger.h"
#include <memory>

namespace {
QStringList pinsFromJsonValue(const QJsonValue& value)
{
    QStringList pins;
    if (value.isArray()) {
        const QJsonArray arr = value.toArray();
        for (const QJsonValue& item : arr) {
            pins << item.toString();
        }
    } else if (value.isString()) {
        QString text = value.toString();
        text.replace(QLatin1Char('\n'), QLatin1Char(','));
        text.replace(QLatin1Char(';'), QLatin1Char(','));
        pins = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
    }
    return pins;
}

QJsonArray pinsToJsonArray(const QStringList& pins)
{
    QJsonArray arr;
    for (const QString& pin : pins) {
        arr.append(pin);
    }
    return arr;
}
} // namespace

UniversalScriptNode::UniversalScriptNode(QObject* parent)
    : QObject(parent)
{
}

NodeDescriptor UniversalScriptNode::getDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("universal-script");
    desc.name = QStringLiteral("Universal Script");
    desc.category = QStringLiteral("Scripting");

    for (const QString& pinId : m_inputPins) {
        addPin(desc.inputPins, desc.inputPinOrder, PinDirection::Input, pinId);
    }
    for (const QString& pinId : m_outputPins) {
        addPin(desc.outputPins, desc.outputPinOrder, PinDirection::Output, pinId);
    }

    return desc;
}

QWidget* UniversalScriptNode::createConfigurationWidget(QWidget* parent)
{
    auto* widget = new UniversalScriptPropertiesWidget(parent);
    widget->setScript(m_scriptCode);
    widget->setEngineId(m_engineId);
    widget->setFanOut(m_enableFanOut);
    widget->setSyntaxHighlighting(m_enableSyntaxHighlighting);
    widget->setInputPins(m_inputPins);
    widget->setOutputPins(m_outputPins);

    connect(widget, &UniversalScriptPropertiesWidget::scriptChanged, this, &UniversalScriptNode::onScriptChanged);
    connect(widget, &UniversalScriptPropertiesWidget::engineChanged, this, &UniversalScriptNode::onEngineChanged);
    connect(widget, &UniversalScriptPropertiesWidget::fanOutChanged, this, &UniversalScriptNode::onFanOutChanged);
    connect(widget, &UniversalScriptPropertiesWidget::syntaxHighlightingChanged, this, &UniversalScriptNode::onSyntaxHighlightingChanged);
    connect(widget, &UniversalScriptPropertiesWidget::inputPinsChanged, this, &UniversalScriptNode::onInputPinsChanged);
    connect(widget, &UniversalScriptPropertiesWidget::outputPinsChanged, this, &UniversalScriptNode::onOutputPinsChanged);

    return widget;
}

TokenList UniversalScriptNode::execute(const TokenList& incomingTokens)
{
    // Step 1: Merge incoming tokens into a single DataPacket
    DataPacket input;
    QVariantList tokenSnapshots;
    for (const auto& token : incomingTokens) {
        QVariantMap tokenSnapshot;
        for (auto it = token.data.cbegin(); it != token.data.cend(); ++it) {
            input.insert(it.key(), it.value());
            tokenSnapshot.insert(it.key(), it.value());
        }
        if (!tokenSnapshot.isEmpty()) {
            tokenSnapshots.append(tokenSnapshot);
        }
    }
    if (!tokenSnapshots.isEmpty()) {
        input.insert(QStringLiteral("_tokens"), tokenSnapshots);
    }

    DataPacket output;
    QList<QString> logs;

    // Step 2: Retrieve the ScriptEngineFactory
    // Note: The instruction mentions ScriptEngineRegistry::instance().getFactory(m_engineId)
    // but IScriptHost.h shows createEngine(id). I'll use createEngine.
    std::unique_ptr<IScriptEngine> engine = ScriptEngineRegistry::instance().createEngine(m_engineId);

    if (!engine) {
        const QString msg = QStringLiteral("Script engine not found: %1").arg(m_engineId);
        CP_WARN << msg;
        ExecutionToken token;
        token.data.insert(QStringLiteral("__error"), msg);
        token.data.insert(QStringLiteral("status"), QStringLiteral("FAIL"));
        return TokenList{token};
    }

    // Step 3: Create the bridge
    ExecutionScriptHost host(input, output, logs);

    // Step 4: Run it
    bool success = engine->execute(m_scriptCode, &host);

    if (!success) {
        CP_WARN << "Script execution failed";
        if (!output.contains(QStringLiteral("__error"))) {
            output.insert(QStringLiteral("__error"), QStringLiteral("Script execution failed"));
        }
    }

    // Handle Status
    const QString statusKey = QString::fromLatin1(kStatusId);
    const QString outputKey = QString::fromLatin1(kOutputId);

    if (!output.contains(statusKey)) {
        output.insert(statusKey, success ? QStringLiteral("OK") : QStringLiteral("FAIL"));
    }
    if (!output.contains(QStringLiteral("text"))) {
        for (const QString& pinId : m_outputPins) {
            if (pinId.startsWith(QLatin1Char('_')) || pinId == statusKey) {
                continue;
            }
            if (output.contains(pinId)) {
                output.insert(QStringLiteral("text"), output.value(pinId));
                break;
            }
        }
    }

    // Step 5: Inject summary into logs for visibility in the Stage Output panel
    auto formatVariant = [](const QVariant& v, bool indented) -> QString {
        if (v.typeId() == QMetaType::QVariantList || v.typeId() == QMetaType::QStringList || v.typeId() == QMetaType::QVariantMap) {
            return QJsonDocument::fromVariant(v).toJson(indented ? QJsonDocument::Indented : QJsonDocument::Compact).trimmed();
        }
        return v.toString();
    };

    QString summary;
    if (m_enableFanOut && output.contains(outputKey)) {
        QVariant outVal = output.value(outputKey);
        if (outVal.typeId() == QMetaType::QVariantList || outVal.typeId() == QMetaType::QStringList) {
            QVariantList list = outVal.toList();
            for (int i = 0; i < list.size(); ++i) {
                if (!summary.isEmpty()) summary += QStringLiteral("\n");
                summary += QStringLiteral("output[%1]: %2").arg(i + 1).arg(formatVariant(list.at(i), false));
            }
        }
    }

    if (!summary.isEmpty()) {
        QString currentLogs = output.value(QStringLiteral("logs")).toString();
        if (!currentLogs.isEmpty()) {
            currentLogs += QStringLiteral("  \n");
        }
        currentLogs += summary;
        output.insert(QStringLiteral("logs"), currentLogs);
    }

    // Prepare output tokens
    if (m_enableFanOut && output.contains(outputKey)) {
        QVariant outVal = output.value(outputKey);
        if (outVal.typeId() == QMetaType::QVariantList || outVal.typeId() == QMetaType::QStringList) {
            TokenList result;
            QVariantList list = outVal.toList();
            for (const auto& item : list) {
                ExecutionToken t;
                t.data = output;
                t.data.insert(outputKey, item);
                result.push_back(std::move(t));
            }
            return result;
        }
    }

    ExecutionToken outToken;
    outToken.data = output;

    TokenList result;
    result.push_back(std::move(outToken));
    return result;
}

QJsonObject UniversalScriptNode::saveState() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("scriptCode"), m_scriptCode);
    obj.insert(QStringLiteral("engineId"), m_engineId);
    obj.insert(QStringLiteral("enableFanOut"), m_enableFanOut);
    obj.insert(QStringLiteral("enableSyntaxHighlighting"), m_enableSyntaxHighlighting);
    obj.insert(QStringLiteral("inputPins"), pinsToJsonArray(m_inputPins));
    obj.insert(QStringLiteral("outputPins"), pinsToJsonArray(m_outputPins));
    return obj;
}

void UniversalScriptNode::loadState(const QJsonObject& data)
{
    if (data.contains(QStringLiteral("engineId"))) {
        m_engineId = data.value(QStringLiteral("engineId")).toString();
    }
    if (data.contains(QStringLiteral("scriptCode"))) {
        m_scriptCode = data.value(QStringLiteral("scriptCode")).toString();
    }
    if (data.contains(QStringLiteral("enableFanOut"))) {
        m_enableFanOut = data.value(QStringLiteral("enableFanOut")).toBool();
    }
    if (data.contains(QStringLiteral("enableSyntaxHighlighting"))) {
        m_enableSyntaxHighlighting = data.value(QStringLiteral("enableSyntaxHighlighting")).toBool(true);
    }
    if (data.contains(QStringLiteral("inputPins"))) {
        m_inputPins = sanitizePinList(pinsFromJsonValue(data.value(QStringLiteral("inputPins"))),
                                      QStringList{QString::fromLatin1(kInputId)});
        emit inputPinsChanged();
    }
    if (data.contains(QStringLiteral("outputPins"))) {
        m_outputPins = sanitizePinList(pinsFromJsonValue(data.value(QStringLiteral("outputPins"))),
                                       QStringList{QString::fromLatin1(kOutputId), QString::fromLatin1(kStatusId)});
        emit outputPinsChanged();
    }
    
    if (m_engineId.isEmpty()) {
        m_engineId = QStringLiteral("quickjs");
    }

    if (!m_scriptCode.trimmed().isEmpty() && UniversalScriptTemplates::isManagedTemplate(m_scriptCode)) {
        m_scriptCode = UniversalScriptTemplates::forEngine(m_engineId);
    }
}

bool UniversalScriptNode::isReady(const QVariantMap& inputs, int incomingConnectionsCount) const
{
    if (incomingConnectionsCount == 0) {
        return true;
    }
    return inputs.size() >= incomingConnectionsCount;
}

void UniversalScriptNode::onScriptChanged(const QString& script)
{
    m_scriptCode = script;
}

void UniversalScriptNode::onEngineChanged(const QString& engineId)
{
    if (!m_scriptCode.trimmed().isEmpty() && UniversalScriptTemplates::isManagedTemplate(m_scriptCode)) {
        m_scriptCode = UniversalScriptTemplates::forEngine(engineId);
    }
    m_engineId = engineId;
}

void UniversalScriptNode::onFanOutChanged(bool enabled)
{
    m_enableFanOut = enabled;
}

void UniversalScriptNode::onSyntaxHighlightingChanged(bool enabled)
{
    m_enableSyntaxHighlighting = enabled;
}

void UniversalScriptNode::onInputPinsChanged(const QStringList& pins)
{
    const QStringList next = sanitizePinList(pins, QStringList{QString::fromLatin1(kInputId)});
    if (m_inputPins == next) {
        return;
    }
    m_inputPins = next;
    emit inputPinsChanged();
}

void UniversalScriptNode::onOutputPinsChanged(const QStringList& pins)
{
    const QStringList next = sanitizePinList(pins, QStringList{QString::fromLatin1(kOutputId), QString::fromLatin1(kStatusId)});
    if (m_outputPins == next) {
        return;
    }
    m_outputPins = next;
    emit outputPinsChanged();
}

QStringList UniversalScriptNode::sanitizePinList(QStringList pins, const QStringList& fallback)
{
    QStringList cleaned;
    for (QString pin : pins) {
        pin = pin.trimmed();
        if (pin.isEmpty()) {
            continue;
        }
        pin.replace(QLatin1Char(' '), QLatin1Char('_'));
        if (!cleaned.contains(pin)) {
            cleaned << pin;
        }
    }
    return cleaned.isEmpty() ? fallback : cleaned;
}

void UniversalScriptNode::addPin(QMap<QString, PinDefinition>& pins,
                                 QStringList& order,
                                 PinDirection direction,
                                 const QString& id)
{
    PinDefinition pin;
    pin.direction = direction;
    pin.id = id;
    pin.name = id == QString::fromLatin1(kInputId) ? QStringLiteral("Input")
             : id == QString::fromLatin1(kOutputId) ? QStringLiteral("Output")
             : id == QString::fromLatin1(kStatusId) ? QStringLiteral("Status")
             : id;
    pin.type = QStringLiteral("text");
    pins.insert(pin.id, pin);
    order << pin.id;
}
