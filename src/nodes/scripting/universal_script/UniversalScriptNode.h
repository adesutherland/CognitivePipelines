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

/**
 * @brief Node that executes a script using a registered script engine.
 */
class UniversalScriptNode : public QObject, public IToolNode {
    Q_OBJECT
    Q_INTERFACES(IToolNode)

public:
    explicit UniversalScriptNode(QObject* parent = nullptr);
    ~UniversalScriptNode() override = default;

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

private:
    QString m_scriptCode;
    QString m_engineId{QStringLiteral("quickjs")};
    bool m_enableFanOut = false;
};
