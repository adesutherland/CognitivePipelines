//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
#pragma once

#include <QDialog>
#include <QFutureWatcher>
#include <QJsonObject>
#include <QList>

#include "ai/catalog/ModelCatalogService.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QTextEdit;

class ProviderManagementDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProviderManagementDialog(QWidget* parent = nullptr);
    ~ProviderManagementDialog() override = default;

private slots:
    void onSaveProviders();
    void onReloadCatalogEditor();
    void onFormatCatalogEditor();
    void onSaveCatalogEditor();
    void onResetCatalogEditor();
    void onRefreshModels();
    void onModelsFetched();
    void onTestSelectedModel();
    void onModelTestFinished();
    void onCopyRuleToCatalogEditor();

private:
    enum ProviderColumn {
        ProviderEnabledColumn = 0,
        ProviderNameColumn,
        ProviderRequiresKeyColumn,
        ProviderBaseUrlColumn,
        ProviderStatusColumn,
        ProviderColumnCount
    };

    enum ModelColumn {
        ModelIdColumn = 0,
        ModelVisibilityColumn,
        ModelDriverColumn,
        ModelReasonColumn,
        ModelColumnCount
    };

    enum RuleColumn {
        RuleIdColumn = 0,
        RuleProviderColumn,
        RulePriorityColumn,
        RulePatternColumn,
        RuleDriverColumn,
        RuleCapabilitiesColumn,
        RuleColumnCount
    };

    void buildUi();
    void populateProviderTable();
    void populateProviderCombo();
    void populateModelTable(const QList<ModelCatalogEntry>& entries);
    void populateRulesTable();
    QJsonObject readCatalogEditor(bool* ok = nullptr);
    QJsonObject readUserCatalogFile() const;
    bool writeUserCatalogFile(const QJsonObject& root);
    QString userCatalogPath() const;
    bool userCatalogExists() const;
    void updateCatalogLocationLabels();
    ModelCatalogKind selectedCatalogKind() const;
    QString selectedProviderId() const;
    QString selectedModelId() const;
    QString providerDisplayName(const QString& providerId) const;
    QString providerStatusText(const QString& providerId, bool enabled, bool requiresCredential) const;
    void reloadRegistryAndViews();

    QTableWidget* m_providerTable {nullptr};
    QComboBox* m_modelProviderCombo {nullptr};
    QComboBox* m_modelKindCombo {nullptr};
    QCheckBox* m_showFilteredModelsCheck {nullptr};
    QPushButton* m_refreshModelsButton {nullptr};
    QPushButton* m_testModelButton {nullptr};
    QLabel* m_testStatusLabel {nullptr};
    QTableWidget* m_modelTable {nullptr};
    QTableWidget* m_rulesTable {nullptr};
    QLabel* m_distributionCatalogPathLabel {nullptr};
    QLabel* m_userCatalogPathLabel {nullptr};
    QLabel* m_catalogStatusLabel {nullptr};
    QTextEdit* m_catalogEditor {nullptr};

    QFutureWatcher<QList<ModelCatalogEntry>> m_modelFetcher;
    QFutureWatcher<ModelTestResult> m_modelTester;
    QList<ModelCatalogEntry> m_lastModels;
};
