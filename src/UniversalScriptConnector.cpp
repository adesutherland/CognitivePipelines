//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//

#include "UniversalScriptConnector.h"
#include "ScriptPropertiesWidget.h"
#include "IScriptHost.h"
#include "ExecutionScriptHost.h"
#include "ExecutionToken.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include "Logger.h"
#include <memory>

UniversalScriptConnector::UniversalScriptConnector(QObject* parent)
    : QObject(parent)
{
}

NodeDescriptor UniversalScriptConnector::getDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("universal-script");
    desc.name = QStringLiteral("Universal Script");
    desc.category = QStringLiteral("Scripting");

    PinDefinition in;
    in.direction = PinDirection::Input;
    in.id = QStringLiteral("in");
    in.name = QStringLiteral("Input");
    in.type = QStringLiteral("text");
    desc.inputPins.insert(in.id, in);

    PinDefinition out;
    out.direction = PinDirection::Output;
    out.id = QStringLiteral("out");
    out.name = QStringLiteral("Output");
    out.type = QStringLiteral("text");
    desc.outputPins.insert(out.id, out);

    return desc;
}

QWidget* UniversalScriptConnector::createConfigurationWidget(QWidget* parent)
{
    auto* widget = new ScriptPropertiesWidget(parent);
    widget->setScript(m_scriptCode);
    widget->setEngineId(m_engineId);
    widget->setFanOut(m_enableFanOut);

    connect(widget, &ScriptPropertiesWidget::scriptChanged, this, &UniversalScriptConnector::onScriptChanged);
    connect(widget, &ScriptPropertiesWidget::engineChanged, this, &UniversalScriptConnector::onEngineChanged);
    connect(widget, &ScriptPropertiesWidget::fanOutChanged, this, &UniversalScriptConnector::onFanOutChanged);

    return widget;
}

TokenList UniversalScriptConnector::execute(const TokenList& incomingTokens)
{
    // Step 1: Merge incoming tokens into a single DataPacket
    DataPacket input;
    for (const auto& token : incomingTokens) {
        for (auto it = token.data.cbegin(); it != token.data.cend(); ++it) {
            input.insert(it.key(), it.value());
        }
    }

    DataPacket output;
    QList<QString> logs;

    // Step 2: Retrieve the ScriptEngineFactory
    // Note: The instruction mentions ScriptEngineRegistry::instance().getFactory(m_engineId)
    // but IScriptHost.h shows createEngine(id). I'll use createEngine.
    std::unique_ptr<IScriptEngine> engine = ScriptEngineRegistry::instance().createEngine(m_engineId);

    if (!engine) {
        CP_WARN << "Engine not found:" << m_engineId;
        // Optionally log error via output if appropriate, but following instructions
        return {};
    }

    // Step 3: Create the bridge
    ExecutionScriptHost host(input, output, logs);

    // Step 4: Run it
    bool success = engine->execute(m_scriptCode, &host);

    if (!success) {
        CP_WARN << "Script execution failed";
    }

    // Step 5: Inject summary into logs for visibility in the Stage Output panel
    auto formatVariant = [](const QVariant& v, bool indented) -> QString {
        if (v.typeId() == QMetaType::QVariantList || v.typeId() == QMetaType::QStringList || v.typeId() == QMetaType::QVariantMap) {
            return QJsonDocument::fromVariant(v).toJson(indented ? QJsonDocument::Indented : QJsonDocument::Compact).trimmed();
        }
        return v.toString();
    };

    QString summary;
    if (m_enableFanOut && output.contains(QStringLiteral("out"))) {
        QVariant outVal = output.value(QStringLiteral("out"));
        if (outVal.typeId() == QMetaType::QVariantList || outVal.typeId() == QMetaType::QStringList) {
            QVariantList list = outVal.toList();
            for (int i = 0; i < list.size(); ++i) {
                if (!summary.isEmpty()) summary += QStringLiteral("\n");
                summary += QStringLiteral("out[%1]: %2").arg(i + 1).arg(formatVariant(list.at(i), false));
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
    if (m_enableFanOut && output.contains(QStringLiteral("out"))) {
        QVariant outVal = output.value(QStringLiteral("out"));
        if (outVal.typeId() == QMetaType::QVariantList || outVal.typeId() == QMetaType::QStringList) {
            TokenList result;
            QVariantList list = outVal.toList();
            for (const auto& item : list) {
                ExecutionToken t;
                t.data = output;
                t.data.insert(QStringLiteral("out"), item);
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

QJsonObject UniversalScriptConnector::saveState() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("scriptCode"), m_scriptCode);
    obj.insert(QStringLiteral("engineId"), m_engineId);
    obj.insert(QStringLiteral("enableFanOut"), m_enableFanOut);
    return obj;
}

void UniversalScriptConnector::loadState(const QJsonObject& data)
{
    if (data.contains(QStringLiteral("scriptCode"))) {
        m_scriptCode = data.value(QStringLiteral("scriptCode")).toString();
    }
    if (data.contains(QStringLiteral("engineId"))) {
        m_engineId = data.value(QStringLiteral("engineId")).toString();
    }
    if (data.contains(QStringLiteral("enableFanOut"))) {
        m_enableFanOut = data.value(QStringLiteral("enableFanOut")).toBool();
    }
    
    if (m_engineId.isEmpty()) {
        m_engineId = QStringLiteral("quickjs");
    }
}

void UniversalScriptConnector::onScriptChanged(const QString& script)
{
    m_scriptCode = script;
}

void UniversalScriptConnector::onEngineChanged(const QString& engineId)
{
    m_engineId = engineId;
}

void UniversalScriptConnector::onFanOutChanged(bool enabled)
{
    m_enableFanOut = enabled;
}
