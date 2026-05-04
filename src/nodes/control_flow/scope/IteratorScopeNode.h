#pragma once

#include "IToolNode.h"
#include "ScopeRuntime.h"

#include <QObject>
#include <QString>

class IteratorScopeNode : public QObject, public IToolNode {
    Q_OBJECT
    Q_INTERFACES(IToolNode)

public:
    explicit IteratorScopeNode(QObject* parent = nullptr);
    ~IteratorScopeNode() override = default;

    NodeDescriptor getDescriptor() const override;
    QWidget* createConfigurationWidget(QWidget* parent) override;
    bool isReady(const QVariantMap& inputs, int incomingConnectionsCount) const override;
    TokenList execute(const TokenList& incomingTokens) override;
    QJsonObject saveState() const override;
    void loadState(const QJsonObject& data) override;

    void setBodyRunner(ScopeBodyRunner runner);

    QString bodyId() const { return m_bodyId; }
    QString failurePolicy() const { return m_failurePolicy; }
    QString lastStatus() const { return m_lastStatus; }

    static constexpr const char* kInputContextId = "context";
    static constexpr const char* kInputItemsId = "items";
    static constexpr const char* kOutputContextId = "context";
    static constexpr const char* kOutputErrorsId = "errors";
    static constexpr const char* kOutputResultsId = "results";
    static constexpr const char* kOutputStatusId = "status";
    static constexpr const char* kOutputSummaryId = "summary";
    static constexpr const char* kOutputTextId = "text";

public slots:
    void setFailurePolicy(const QString& policy);
    void requestOpenBody();

signals:
    void failurePolicyChanged(const QString& policy);
    void statusChanged(const QString& status);
    void openBodyRequested(const QString& bodyId, const QString& title);

private:
    TokenList errorOutput(const QString& message, const QVariantList& errors = {});
    ScopeFrame makeFrame(const QVariant& item,
                         int index,
                         int count,
                         const QVariantMap& context,
                         const QVariantList& history) const;
    void setLastStatus(const QString& status);

private:
    QString m_bodyId;
    QString m_failurePolicy {QStringLiteral("stop")};
    QString m_lastStatus;
    ScopeBodyRunner m_bodyRunner;
};
