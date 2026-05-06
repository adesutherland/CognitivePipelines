//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>

#include "IToolNode.h"
#include "CommonDataTypes.h"
#include "UniversalScriptTemplates.h"

/**
 * @brief Node that executes a script using a registered script engine.
 */
class UniversalScriptNode : public QObject, public IToolNode {
    Q_OBJECT
    Q_INTERFACES(IToolNode)

public:
    explicit UniversalScriptNode(QObject* parent = nullptr);
    ~UniversalScriptNode() override = default;

    static constexpr const char* kInputId = "input";
    static constexpr const char* kOutputId = "output";
    static constexpr const char* kStatusId = "status";

    // IToolNode interface
    NodeDescriptor getDescriptor() const override;
    QWidget* createConfigurationWidget(QWidget* parent) override;
    TokenList execute(const TokenList& incomingTokens) override;
    QJsonObject saveState() const override;
    void loadState(const QJsonObject& data) override;
    bool isReady(const QVariantMap& inputs, int incomingConnectionsCount) const override;

    QStringList inputPins() const { return m_inputPins; }
    QStringList outputPins() const { return m_outputPins; }

signals:
    void inputPinsChanged();
    void outputPinsChanged();

private slots:
    void onScriptChanged(const QString& script);
    void onEngineChanged(const QString& engineId);
    void onFanOutChanged(bool enabled);
    void onSyntaxHighlightingChanged(bool enabled);
    void onInputPinsChanged(const QStringList& pins);
    void onOutputPinsChanged(const QStringList& pins);

private:
    static QStringList sanitizePinList(QStringList pins, const QStringList& fallback);
    static void addPin(QMap<QString, PinDefinition>& pins,
                       QStringList& order,
                       PinDirection direction,
                       const QString& id);

    QString m_scriptCode;
    QString m_engineId{QStringLiteral("quickjs")};
    bool m_enableFanOut = false;
    bool m_enableSyntaxHighlighting = true;
    QStringList m_inputPins{QStringLiteral("input")};
    QStringList m_outputPins{QStringLiteral("output"), QStringLiteral("status")};
};
