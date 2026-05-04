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

    PinDefinition in;
    in.direction = PinDirection::Input;
    in.id = QString::fromLatin1(kInputId);
    in.name = QStringLiteral("Input");
    in.type = QStringLiteral("text");
    desc.inputPins.insert(in.id, in);

    PinDefinition out;
    out.direction = PinDirection::Output;
    out.id = QString::fromLatin1(kOutputId);
    out.name = QStringLiteral("Output");
    out.type = QStringLiteral("text");
    desc.outputPins.insert(out.id, out);

    PinDefinition status;
    status.direction = PinDirection::Output;
    status.id = QString::fromLatin1(kStatusId);
    status.name = QStringLiteral("Status");
    status.type = QStringLiteral("text");
    desc.outputPins.insert(status.id, status);

    return desc;
}

QWidget* UniversalScriptNode::createConfigurationWidget(QWidget* parent)
{
    auto* widget = new UniversalScriptPropertiesWidget(parent);
    widget->setScript(m_scriptCode);
    widget->setEngineId(m_engineId);
    widget->setFanOut(m_enableFanOut);
    widget->setSyntaxHighlighting(m_enableSyntaxHighlighting);

    connect(widget, &UniversalScriptPropertiesWidget::scriptChanged, this, &UniversalScriptNode::onScriptChanged);
    connect(widget, &UniversalScriptPropertiesWidget::engineChanged, this, &UniversalScriptNode::onEngineChanged);
    connect(widget, &UniversalScriptPropertiesWidget::fanOutChanged, this, &UniversalScriptNode::onFanOutChanged);
    connect(widget, &UniversalScriptPropertiesWidget::syntaxHighlightingChanged, this, &UniversalScriptNode::onSyntaxHighlightingChanged);

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
    
    if (m_engineId.isEmpty()) {
        m_engineId = QStringLiteral("quickjs");
    }

    if (!m_scriptCode.trimmed().isEmpty() && UniversalScriptTemplates::isManagedTemplate(m_scriptCode)) {
        m_scriptCode = UniversalScriptTemplates::forEngine(m_engineId);
    }
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
