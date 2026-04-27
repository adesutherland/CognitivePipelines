#pragma once

#include <QFutureWatcher>
#include <QWidget>

#include "ai/catalog/ModelCatalogService.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTextEdit;

class VaultOutputPropertiesWidget : public QWidget {
    Q_OBJECT
public:
    explicit VaultOutputPropertiesWidget(QWidget* parent = nullptr);
    ~VaultOutputPropertiesWidget() override = default;

    void setVaultRoot(const QString& path);
    void setProvider(const QString& providerId);
    void setModel(const QString& modelId);
    void setRoutingPrompt(const QString& prompt);
    void setTemperature(double value);
    void setMaxTokens(int value);

    QString vaultRoot() const;
    QString provider() const;
    QString model() const;
    QString routingPrompt() const;
    double temperature() const;
    int maxTokens() const;

signals:
    void vaultRootChanged(const QString& path);
    void providerChanged(const QString& providerId);
    void modelChanged(const QString& modelId);
    void routingPromptChanged(const QString& prompt);
    void temperatureChanged(double value);
    void maxTokensChanged(int value);

private slots:
    void onBrowseVaultRoot();
    void onProviderIndexChanged(int index);
    void onModelIndexChanged(int index);
    void onModelsFetched();
    void onShowFilteredChanged(bool checked);
    void onTestModelClicked();
    void onModelTestFinished();

private:
    void populateModelCombo(const QList<ModelCatalogEntry>& models);

    QLineEdit* m_vaultRootEdit {nullptr};
    QPushButton* m_browseVaultRootButton {nullptr};
    QComboBox* m_providerCombo {nullptr};
    QComboBox* m_modelCombo {nullptr};
    QCheckBox* m_showFilteredCheck {nullptr};
    QPushButton* m_testModelButton {nullptr};
    QLabel* m_testStatusLabel {nullptr};
    QTextEdit* m_routingPromptEdit {nullptr};
    QDoubleSpinBox* m_temperatureSpinBox {nullptr};
    QSpinBox* m_maxTokensSpinBox {nullptr};
    QFutureWatcher<QList<ModelCatalogEntry>> m_modelFetcher;
    QFutureWatcher<ModelTestResult> m_modelTester;
    QList<ModelCatalogEntry> m_lastModels;
    QString m_pendingModelId;
};
