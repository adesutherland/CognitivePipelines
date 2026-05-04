//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//

#pragma once

#include <QObject>
#include <QString>
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

private slots:
    void onScriptChanged(const QString& script);
    void onEngineChanged(const QString& engineId);
    void onFanOutChanged(bool enabled);
    void onSyntaxHighlightingChanged(bool enabled);

private:
    QString m_scriptCode;
    QString m_engineId{QStringLiteral("quickjs")};
    bool m_enableFanOut = false;
    bool m_enableSyntaxHighlighting = true;
};
