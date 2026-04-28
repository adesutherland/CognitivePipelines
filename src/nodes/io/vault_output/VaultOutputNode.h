#pragma once

#include <QObject>
#include <QPointer>

#include "IToolNode.h"

class VaultOutputPropertiesWidget;

class VaultOutputNode : public QObject, public IToolNode {
    Q_OBJECT
    Q_INTERFACES(IToolNode)
public:
    explicit VaultOutputNode(QObject* parent = nullptr);
    ~VaultOutputNode() override = default;

    NodeDescriptor getDescriptor() const override;
    QWidget* createConfigurationWidget(QWidget* parent) override;
    TokenList execute(const TokenList& incomingTokens) override;
    QJsonObject saveState() const override;
    void loadState(const QJsonObject& data) override;

    static constexpr const char* kInputMarkdownId = "markdown";
    static constexpr const char* kInputVaultRootId = "vault_root";
    static constexpr const char* kInputPromptId = "prompt";

    static constexpr const char* kOutputSavedPathId = "saved_path";
    static constexpr const char* kOutputSubfolderId = "subfolder";
    static constexpr const char* kOutputFilenameId = "filename";
    static constexpr const char* kOutputDecisionId = "decision";

public slots:
    void setVaultRoot(const QString& path);
    void setProviderId(const QString& providerId);
    void setModelId(const QString& modelId);
    void setRoutingPrompt(const QString& prompt);
    void setTemperature(double value);
    void setMaxTokens(int value);

private:
    void setStatusMessage(const QString& message);

    QString m_vaultRoot;
    QString m_providerId;
    QString m_modelId;
    QString m_routingPrompt;
    double m_temperature {0.2};
    int m_maxTokens {800};
    QPointer<VaultOutputPropertiesWidget> m_widget;
};
