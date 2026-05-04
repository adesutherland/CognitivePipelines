#pragma once

#include "IToolNode.h"
#include "ScopeRuntime.h"

#include <QObject>
#include <QString>

class TransformScopeNode : public QObject, public IToolNode {
    Q_OBJECT
    Q_INTERFACES(IToolNode)

public:
    explicit TransformScopeNode(QObject* parent = nullptr);
    ~TransformScopeNode() override = default;

    NodeDescriptor getDescriptor() const override;
    QWidget* createConfigurationWidget(QWidget* parent) override;
    bool isReady(const QVariantMap& inputs, int incomingConnectionsCount) const override;
    TokenList execute(const TokenList& incomingTokens) override;
    QJsonObject saveState() const override;
    void loadState(const QJsonObject& data) override;

    void setBodyRunner(ScopeBodyRunner runner);

    QString bodyId() const { return m_bodyId; }
    QString mode() const { return m_mode; }
    int maxAttempts() const { return m_maxAttempts; }
    QString lastStatus() const { return m_lastStatus; }

    static constexpr const char* kInputContextId = "context";
    static constexpr const char* kInputInputId = "input";
    static constexpr const char* kOutputContextId = "context";
    static constexpr const char* kOutputErrorId = "error";
    static constexpr const char* kOutputOutputId = "output";
    static constexpr const char* kOutputStatusId = "status";
    static constexpr const char* kOutputTextId = "text";

public slots:
    void setMode(const QString& mode);
    void setMaxAttempts(int value);
    void requestOpenBody();

signals:
    void modeChanged(const QString& mode);
    void maxAttemptsChanged(int value);
    void statusChanged(const QString& status);
    void openBodyRequested(const QString& bodyId, const QString& title);

private:
    TokenList errorOutput(const QString& message);
    ScopeFrame makeFrame(const DataPacket& inputs,
                         const QVariant& currentInput,
                         const QVariant& previousOutput,
                         int attempt,
                         const QVariantMap& context,
                         const QVariantList& history) const;
    void setLastStatus(const QString& status);

private:
    QString m_bodyId;
    QString m_mode {QStringLiteral("run_once")};
    int m_maxAttempts {3};
    QString m_lastStatus;
    ScopeBodyRunner m_bodyRunner;
};
