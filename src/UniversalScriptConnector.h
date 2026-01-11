//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//

#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>

#include "IToolConnector.h"
#include "CommonDataTypes.h"

/**
 * @brief Connector that executes a script using a registered script engine.
 */
class UniversalScriptConnector : public QObject, public IToolConnector {
    Q_OBJECT
    Q_INTERFACES(IToolConnector)

public:
    explicit UniversalScriptConnector(QObject* parent = nullptr);
    ~UniversalScriptConnector() override = default;

    // IToolConnector interface
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
