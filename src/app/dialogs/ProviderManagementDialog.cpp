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
#include "ProviderManagementDialog.h"

#include "ModelCapsRegistry.h"
#include "ai/backends/ILLMBackend.h"
#include "ai/registry/LLMProviderRegistry.h"
#include "Logger.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QAbstractItemView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int kProviderIdRole = Qt::UserRole + 1;
constexpr int kProviderNameRole = Qt::UserRole + 2;
constexpr int kModelIdRole = Qt::UserRole + 3;
constexpr int kRuleJsonRole = Qt::UserRole + 4;

QString visibilityText(ModelCatalogVisibility visibility)
{
    switch (visibility) {
    case ModelCatalogVisibility::Recommended:
        return QStringLiteral("Recommended");
    case ModelCatalogVisibility::Available:
        return QStringLiteral("Available");
    case ModelCatalogVisibility::Hidden:
        return QStringLiteral("Filtered");
    }
    return QStringLiteral("Unknown");
}

QString capabilityText(ModelCapsTypes::Capability capability)
{
    using ModelCapsTypes::Capability;
    switch (capability) {
    case Capability::Chat:
        return QStringLiteral("chat");
    case Capability::Vision:
        return QStringLiteral("vision");
    case Capability::Reasoning:
        return QStringLiteral("reasoning");
    case Capability::ToolUse:
        return QStringLiteral("tooluse");
    case Capability::LongContext:
        return QStringLiteral("longcontext");
    case Capability::Audio:
        return QStringLiteral("audio");
    case Capability::Image:
        return QStringLiteral("image");
    case Capability::Embedding:
        return QStringLiteral("embedding");
    case Capability::Pdf:
        return QStringLiteral("pdf");
    case Capability::StructuredOutput:
        return QStringLiteral("structuredoutput");
    }
    return QStringLiteral("unknown");
}

QString roleModeText(ModelCapsTypes::RoleMode roleMode)
{
    using ModelCapsTypes::RoleMode;
    switch (roleMode) {
    case RoleMode::System:
        return QStringLiteral("system");
    case RoleMode::Developer:
        return QStringLiteral("developer");
    case RoleMode::SystemInstruction:
        return QStringLiteral("system_instruction");
    case RoleMode::SystemParameter:
        return QStringLiteral("system_parameter");
    }
    return QStringLiteral("system");
}

QStringList capabilityTexts(const QSet<ModelCapsTypes::Capability>& capabilities)
{
    using ModelCapsTypes::Capability;
    static const QList<Capability> orderedCapabilities {
        Capability::Chat,
        Capability::Vision,
        Capability::Reasoning,
        Capability::ToolUse,
        Capability::LongContext,
        Capability::Audio,
        Capability::Image,
        Capability::Embedding,
        Capability::Pdf,
        Capability::StructuredOutput
    };

    QStringList values;
    for (const Capability capability : orderedCapabilities) {
        if (capabilities.contains(capability)) {
            values.append(capabilityText(capability));
        }
    }
    return values;
}

QTableWidgetItem* checkItem(bool checked)
{
    auto* item = new QTableWidgetItem();
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    return item;
}

QTableWidgetItem* readOnlyItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    return item;
}

QJsonObject ruleToOverrideObject(const ModelCapsTypes::ModelRule& rule)
{
    QJsonObject obj;
    if (!rule.id.isEmpty()) {
        obj.insert(QStringLiteral("id"), rule.id);
    }
    if (!rule.backend.isEmpty()) {
        obj.insert(QStringLiteral("backend"), rule.backend);
    }
    obj.insert(QStringLiteral("priority"), rule.priority);
    obj.insert(QStringLiteral("pattern"), rule.pattern.pattern());
    if (!rule.driverProfileId.isEmpty()) {
        obj.insert(QStringLiteral("driver"), rule.driverProfileId);
    }
    obj.insert(QStringLiteral("role_mode"), roleModeText(rule.caps.roleMode));
    if (rule.requiresBackend) {
        obj.insert(QStringLiteral("requires_backend"), true);
    }

    const QStringList capabilities = capabilityTexts(rule.caps.capabilities);
    if (!capabilities.isEmpty()) {
        QJsonArray capArray;
        for (const QString& capability : capabilities) {
            capArray.append(capability);
        }
        obj.insert(QStringLiteral("capabilities"), capArray);
    }

    QJsonObject constraints;
    const auto& capsConstraints = rule.caps.constraints;
    if (capsConstraints.maxInputTokens.has_value()) {
        constraints.insert(QStringLiteral("maxInputTokens"), *capsConstraints.maxInputTokens);
    }
    if (capsConstraints.maxOutputTokens.has_value()) {
        constraints.insert(QStringLiteral("maxOutputTokens"), *capsConstraints.maxOutputTokens);
    }
    if (capsConstraints.temperature.has_value()) {
        QJsonObject temperature;
        if (capsConstraints.temperature->defaultValue.has_value()) {
            temperature.insert(QStringLiteral("default"), *capsConstraints.temperature->defaultValue);
        }
        if (capsConstraints.temperature->min.has_value()) {
            temperature.insert(QStringLiteral("min"), *capsConstraints.temperature->min);
        }
        if (capsConstraints.temperature->max.has_value()) {
            temperature.insert(QStringLiteral("max"), *capsConstraints.temperature->max);
        }
        constraints.insert(QStringLiteral("temperature"), temperature);
    }
    if (capsConstraints.reasoningEffort.has_value()) {
        QJsonObject reasoning;
        if (capsConstraints.reasoningEffort->defaultValue.has_value()
            && !capsConstraints.reasoningEffort->defaultValue->isEmpty()) {
            reasoning.insert(QStringLiteral("default"), *capsConstraints.reasoningEffort->defaultValue);
        }
        QJsonArray allowed;
        for (const QString& value : capsConstraints.reasoningEffort->allowed) {
            allowed.append(value);
        }
        if (!allowed.isEmpty()) {
            reasoning.insert(QStringLiteral("allowed"), allowed);
        }
        constraints.insert(QStringLiteral("reasoning_effort"), reasoning);
    }
    if (capsConstraints.omitTemperature.value_or(false)) {
        constraints.insert(QStringLiteral("omitTemperature"), true);
    }
    if (capsConstraints.tokenFieldName.has_value() && !capsConstraints.tokenFieldName->isEmpty()) {
        constraints.insert(QStringLiteral("tokenFieldName"), *capsConstraints.tokenFieldName);
    }
    if (!constraints.isEmpty()) {
        obj.insert(QStringLiteral("parameter_constraints"), constraints);
    }

    if (!rule.caps.customHeaders.isEmpty()) {
        QJsonObject headers;
        for (auto it = rule.caps.customHeaders.begin(); it != rule.caps.customHeaders.end(); ++it) {
            headers.insert(it.key(), it.value());
        }
        obj.insert(QStringLiteral("headers"), headers);
    }

    return obj;
}

QJsonArray mergeProviderArray(QJsonArray providers, const QList<QJsonObject>& overrides)
{
    for (const QJsonObject& overrideObj : overrides) {
        const QString id = overrideObj.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) {
            continue;
        }

        int existingIndex = -1;
        for (int i = 0; i < providers.size(); ++i) {
            if (providers.at(i).isObject()
                && providers.at(i).toObject().value(QStringLiteral("id")).toString() == id) {
                existingIndex = i;
                break;
            }
        }

        QJsonObject merged = existingIndex >= 0 ? providers.at(existingIndex).toObject() : QJsonObject{};
        for (auto it = overrideObj.begin(); it != overrideObj.end(); ++it) {
            if (it.value().isUndefined()) {
                merged.remove(it.key());
            } else {
                merged.insert(it.key(), it.value());
            }
        }

        if (existingIndex >= 0) {
            providers.replace(existingIndex, merged);
        } else {
            providers.append(merged);
        }
    }

    return providers;
}

void removeProviderKeys(QJsonArray& providers, const QString& providerId, const QStringList& keys)
{
    for (int i = 0; i < providers.size(); ++i) {
        if (!providers.at(i).isObject()) {
            continue;
        }
        QJsonObject provider = providers.at(i).toObject();
        if (provider.value(QStringLiteral("id")).toString() != providerId) {
            continue;
        }
        for (const QString& key : keys) {
            provider.remove(key);
        }
        providers.replace(i, provider);
        return;
    }
}

} // namespace

ProviderManagementDialog::ProviderManagementDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Provider Management"));
    resize(920, 680);
    buildUi();
    populateProviderTable();
    populateProviderCombo();
    populateRulesTable();
    onReloadCatalogEditor();
    onRefreshModels();
}

void ProviderManagementDialog::buildUi()
{
    auto* rootLayout = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);

    auto* providersPage = new QWidget(tabs);
    auto* providersLayout = new QVBoxLayout(providersPage);
    m_providerTable = new QTableWidget(0, ProviderColumnCount, providersPage);
    m_providerTable->setHorizontalHeaderLabels({
        tr("Enabled"),
        tr("Provider"),
        tr("Requires Key"),
        tr("Base URL"),
        tr("Status")
    });
    m_providerTable->horizontalHeader()->setStretchLastSection(true);
    m_providerTable->horizontalHeader()->setSectionResizeMode(ProviderNameColumn, QHeaderView::ResizeToContents);
    m_providerTable->horizontalHeader()->setSectionResizeMode(ProviderBaseUrlColumn, QHeaderView::Stretch);
    m_providerTable->verticalHeader()->setVisible(false);
    providersLayout->addWidget(m_providerTable);

    auto* providerButtonRow = new QHBoxLayout();
    auto* saveProvidersButton = new QPushButton(tr("Save Provider Overrides"), providersPage);
    providerButtonRow->addStretch();
    providerButtonRow->addWidget(saveProvidersButton);
    providersLayout->addLayout(providerButtonRow);
    tabs->addTab(providersPage, tr("Providers"));

    auto* modelsPage = new QWidget(tabs);
    auto* modelsLayout = new QVBoxLayout(modelsPage);
    auto* modelForm = new QFormLayout();
    m_modelProviderCombo = new QComboBox(modelsPage);
    m_modelKindCombo = new QComboBox(modelsPage);
    m_modelKindCombo->addItem(tr("Chat"), static_cast<int>(ModelCatalogKind::Chat));
    m_modelKindCombo->addItem(tr("Embeddings"), static_cast<int>(ModelCatalogKind::Embedding));
    m_modelKindCombo->addItem(tr("Images"), static_cast<int>(ModelCatalogKind::Image));
    modelForm->addRow(tr("Provider:"), m_modelProviderCombo);
    modelForm->addRow(tr("Capability:"), m_modelKindCombo);
    modelsLayout->addLayout(modelForm);

    auto* modelActions = new QHBoxLayout();
    m_showFilteredModelsCheck = new QCheckBox(tr("Show filtered models"), modelsPage);
    m_refreshModelsButton = new QPushButton(tr("Refresh Models"), modelsPage);
    m_testModelButton = new QPushButton(tr("Test Selection"), modelsPage);
    m_testStatusLabel = new QLabel(modelsPage);
    m_testStatusLabel->setWordWrap(true);
    modelActions->addWidget(m_showFilteredModelsCheck);
    modelActions->addStretch();
    modelActions->addWidget(m_refreshModelsButton);
    modelActions->addWidget(m_testModelButton);
    modelsLayout->addLayout(modelActions);
    modelsLayout->addWidget(m_testStatusLabel);

    m_modelTable = new QTableWidget(0, ModelColumnCount, modelsPage);
    m_modelTable->setHorizontalHeaderLabels({
        tr("Model"),
        tr("Status"),
        tr("Driver"),
        tr("Reason")
    });
    m_modelTable->horizontalHeader()->setStretchLastSection(true);
    m_modelTable->horizontalHeader()->setSectionResizeMode(ModelIdColumn, QHeaderView::Stretch);
    m_modelTable->horizontalHeader()->setSectionResizeMode(ModelDriverColumn, QHeaderView::ResizeToContents);
    m_modelTable->verticalHeader()->setVisible(false);
    m_modelTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_modelTable->setSelectionMode(QAbstractItemView::SingleSelection);
    modelsLayout->addWidget(m_modelTable);
    tabs->addTab(modelsPage, tr("Model Inspector"));

    auto* rulesPage = new QWidget(tabs);
    auto* rulesLayout = new QVBoxLayout(rulesPage);
    m_rulesTable = new QTableWidget(0, RuleColumnCount, rulesPage);
    m_rulesTable->setHorizontalHeaderLabels({
        tr("Rule"),
        tr("Provider"),
        tr("Priority"),
        tr("Regex"),
        tr("Driver"),
        tr("Capabilities")
    });
    m_rulesTable->horizontalHeader()->setStretchLastSection(true);
    m_rulesTable->horizontalHeader()->setSectionResizeMode(RuleIdColumn, QHeaderView::ResizeToContents);
    m_rulesTable->horizontalHeader()->setSectionResizeMode(RuleProviderColumn, QHeaderView::ResizeToContents);
    m_rulesTable->horizontalHeader()->setSectionResizeMode(RulePatternColumn, QHeaderView::Stretch);
    m_rulesTable->verticalHeader()->setVisible(false);
    m_rulesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_rulesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    rulesLayout->addWidget(m_rulesTable);

    auto* rulesButtonRow = new QHBoxLayout();
    auto* copyRuleButton = new QPushButton(tr("Copy Rule to Overrides"), rulesPage);
    rulesButtonRow->addStretch();
    rulesButtonRow->addWidget(copyRuleButton);
    rulesLayout->addLayout(rulesButtonRow);
    tabs->addTab(rulesPage, tr("Rules"));

    auto* catalogPage = new QWidget(tabs);
    auto* catalogLayout = new QVBoxLayout(catalogPage);
    m_distributionCatalogPathLabel = new QLabel(catalogPage);
    m_distributionCatalogPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    catalogLayout->addWidget(m_distributionCatalogPathLabel);

    m_userCatalogPathLabel = new QLabel(catalogPage);
    m_userCatalogPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    catalogLayout->addWidget(m_userCatalogPathLabel);

    m_catalogStatusLabel = new QLabel(catalogPage);
    catalogLayout->addWidget(m_catalogStatusLabel);

    m_catalogEditor = new QTextEdit(catalogPage);
    m_catalogEditor->setAcceptRichText(false);
    m_catalogEditor->setLineWrapMode(QTextEdit::NoWrap);
    catalogLayout->addWidget(m_catalogEditor);

    auto* catalogButtonRow = new QHBoxLayout();
    auto* reloadCatalogButton = new QPushButton(tr("Reload User Copy"), catalogPage);
    auto* formatCatalogButton = new QPushButton(tr("Format JSON"), catalogPage);
    auto* resetCatalogButton = new QPushButton(tr("Reset to Distribution Baseline"), catalogPage);
    auto* saveCatalogButton = new QPushButton(tr("Save User Copy"), catalogPage);
    catalogButtonRow->addWidget(reloadCatalogButton);
    catalogButtonRow->addWidget(formatCatalogButton);
    catalogButtonRow->addWidget(resetCatalogButton);
    catalogButtonRow->addStretch();
    catalogButtonRow->addWidget(saveCatalogButton);
    catalogLayout->addLayout(catalogButtonRow);
    tabs->addTab(catalogPage, tr("Catalog Overrides"));

    rootLayout->addWidget(tabs);
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    rootLayout->addWidget(buttonBox);

    connect(saveProvidersButton, &QPushButton::clicked, this, &ProviderManagementDialog::onSaveProviders);
    connect(reloadCatalogButton, &QPushButton::clicked, this, &ProviderManagementDialog::onReloadCatalogEditor);
    connect(formatCatalogButton, &QPushButton::clicked, this, &ProviderManagementDialog::onFormatCatalogEditor);
    connect(saveCatalogButton, &QPushButton::clicked, this, &ProviderManagementDialog::onSaveCatalogEditor);
    connect(resetCatalogButton, &QPushButton::clicked, this, &ProviderManagementDialog::onResetCatalogEditor);
    connect(copyRuleButton, &QPushButton::clicked, this, &ProviderManagementDialog::onCopyRuleToCatalogEditor);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_modelProviderCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &ProviderManagementDialog::onRefreshModels);
    connect(m_modelKindCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &ProviderManagementDialog::onRefreshModels);
    connect(m_showFilteredModelsCheck, &QCheckBox::toggled, this, [this]() {
        populateModelTable(m_lastModels);
    });
    connect(m_refreshModelsButton, &QPushButton::clicked, this, &ProviderManagementDialog::onRefreshModels);
    connect(m_testModelButton, &QPushButton::clicked, this, &ProviderManagementDialog::onTestSelectedModel);
    connect(&m_modelFetcher, &QFutureWatcher<QList<ModelCatalogEntry>>::finished,
            this, &ProviderManagementDialog::onModelsFetched);
    connect(&m_modelTester, &QFutureWatcher<ModelTestResult>::finished,
            this, &ProviderManagementDialog::onModelTestFinished);
}

QString ProviderManagementDialog::userCatalogPath() const
{
    return ModelCapsRegistry::instance().userConfigPath();
}

bool ProviderManagementDialog::userCatalogExists() const
{
    return QFileInfo::exists(userCatalogPath());
}

void ProviderManagementDialog::updateCatalogLocationLabels()
{
    const auto& registry = ModelCapsRegistry::instance();
    if (m_distributionCatalogPathLabel) {
        m_distributionCatalogPathLabel->setText(
            tr("Distribution baseline: %1 (compiled from resources/model_caps.json)")
                .arg(registry.distributionConfigPath()));
    }
    if (m_userCatalogPathLabel) {
        m_userCatalogPathLabel->setText(tr("User copy: %1").arg(userCatalogPath()));
    }
    if (m_catalogStatusLabel) {
        m_catalogStatusLabel->setText(userCatalogExists()
                                          ? tr("Active config: distribution baseline plus user copy overrides")
                                          : tr("Active config: distribution baseline only"));
    }
}

QJsonObject ProviderManagementDialog::readUserCatalogFile() const
{
    QFile file(userCatalogPath());
    if (!file.exists()) {
        return {};
    }
    if (!file.open(QIODevice::ReadOnly)) {
        CP_WARN << "ProviderManagementDialog: failed to open catalog override" << file.fileName() << file.errorString();
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        CP_WARN << "ProviderManagementDialog: failed to parse catalog override" << file.fileName()
                << parseError.errorString();
        return {};
    }
    return doc.object();
}

bool ProviderManagementDialog::writeUserCatalogFile(const QJsonObject& root)
{
    const QString path = userCatalogPath();
    QDir().mkpath(QFileInfo(path).dir().absolutePath());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Save Failed"), tr("Could not open catalog override file:\n%1").arg(file.errorString()));
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        QMessageBox::warning(this, tr("Save Failed"), tr("Could not write catalog override file:\n%1").arg(file.errorString()));
        return false;
    }
    return true;
}

QJsonObject ProviderManagementDialog::readCatalogEditor(bool* ok)
{
    if (ok) {
        *ok = false;
    }
    if (!m_catalogEditor) {
        return {};
    }

    const QByteArray payload = m_catalogEditor->toPlainText().toUtf8();
    if (payload.trimmed().isEmpty()) {
        if (ok) {
            *ok = true;
        }
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this,
                             tr("Invalid JSON"),
                             tr("Catalog overrides must be a JSON object:\n%1").arg(parseError.errorString()));
        return {};
    }

    if (ok) {
        *ok = true;
    }
    return doc.object();
}

QString ProviderManagementDialog::providerDisplayName(const QString& providerId) const
{
    if (const auto settings = ModelCapsRegistry::instance().providerSettings(providerId)) {
        if (!settings->name.trimmed().isEmpty()) {
            return settings->name.trimmed();
        }
    }

    if (ILLMBackend* backend = LLMProviderRegistry::instance().getBackend(providerId)) {
        return backend->name();
    }

    return providerId;
}

QString ProviderManagementDialog::providerStatusText(const QString& providerId,
                                                     bool enabled,
                                                     bool requiresCredential) const
{
    if (!enabled) {
        return tr("Disabled");
    }
    if (!LLMProviderRegistry::instance().getBackend(providerId)) {
        return tr("Not registered");
    }
    if (!requiresCredential) {
        return tr("No key required");
    }
    return LLMProviderRegistry::instance().getCredential(providerId).trimmed().isEmpty()
               ? tr("Missing key")
               : tr("Configured");
}

void ProviderManagementDialog::populateProviderTable()
{
    if (!m_providerTable) {
        return;
    }

    QMap<QString, ModelCapsTypes::ProviderSettings> providersById;
    for (const auto& settings : ModelCapsRegistry::instance().providerSettingsList()) {
        providersById.insert(settings.id, settings);
    }
    for (ILLMBackend* backend : LLMProviderRegistry::instance().allBackends()) {
        if (!backend || providersById.contains(backend->id())) {
            continue;
        }
        ModelCapsTypes::ProviderSettings settings;
        settings.id = backend->id();
        settings.name = backend->name();
        settings.requiresCredential = ModelCatalogService::providerRequiresCredential(settings.id);
        providersById.insert(settings.id, settings);
    }

    QList<ModelCapsTypes::ProviderSettings> providers = providersById.values();
    std::sort(providers.begin(), providers.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.id.localeAwareCompare(rhs.id) < 0;
    });

    m_providerTable->setRowCount(0);
    for (const auto& provider : providers) {
        const int row = m_providerTable->rowCount();
        m_providerTable->insertRow(row);

        auto* enabledItem = checkItem(provider.enabled);
        m_providerTable->setItem(row, ProviderEnabledColumn, enabledItem);

        auto* nameItem = readOnlyItem(provider.name.trimmed().isEmpty() ? provider.id : provider.name);
        nameItem->setData(kProviderIdRole, provider.id);
        nameItem->setData(kProviderNameRole, provider.name);
        m_providerTable->setItem(row, ProviderNameColumn, nameItem);

        m_providerTable->setItem(row, ProviderRequiresKeyColumn, checkItem(provider.requiresCredential));
        m_providerTable->setItem(row, ProviderBaseUrlColumn, new QTableWidgetItem(provider.baseUrl));
        m_providerTable->setItem(row,
                                 ProviderStatusColumn,
                                 readOnlyItem(providerStatusText(provider.id, provider.enabled, provider.requiresCredential)));
    }
}

void ProviderManagementDialog::populateProviderCombo()
{
    if (!m_modelProviderCombo) {
        return;
    }

    const QString previous = m_modelProviderCombo->currentData().toString();
    m_modelProviderCombo->blockSignals(true);
    m_modelProviderCombo->clear();
    for (int row = 0; row < m_providerTable->rowCount(); ++row) {
        const auto* providerItem = m_providerTable->item(row, ProviderNameColumn);
        if (!providerItem) {
            continue;
        }
        const QString providerId = providerItem->data(kProviderIdRole).toString();
        const QString name = providerItem->text();
        m_modelProviderCombo->addItem(QStringLiteral("%1 (%2)").arg(name, providerId), providerId);
    }

    const int previousIndex = m_modelProviderCombo->findData(previous);
    if (previousIndex >= 0) {
        m_modelProviderCombo->setCurrentIndex(previousIndex);
    }
    m_modelProviderCombo->blockSignals(false);
}

ModelCatalogKind ProviderManagementDialog::selectedCatalogKind() const
{
    if (!m_modelKindCombo) {
        return ModelCatalogKind::Chat;
    }
    return static_cast<ModelCatalogKind>(m_modelKindCombo->currentData().toInt());
}

QString ProviderManagementDialog::selectedProviderId() const
{
    return m_modelProviderCombo ? m_modelProviderCombo->currentData().toString() : QString();
}

QString ProviderManagementDialog::selectedModelId() const
{
    if (!m_modelTable || m_modelTable->currentRow() < 0) {
        return {};
    }
    const auto* item = m_modelTable->item(m_modelTable->currentRow(), ModelIdColumn);
    return item ? item->data(kModelIdRole).toString() : QString();
}

void ProviderManagementDialog::populateModelTable(const QList<ModelCatalogEntry>& entries)
{
    if (!m_modelTable) {
        return;
    }

    const bool includeHidden = m_showFilteredModelsCheck && m_showFilteredModelsCheck->isChecked();
    m_modelTable->setRowCount(0);
    for (const auto& entry : entries) {
        if (!includeHidden && entry.visibility == ModelCatalogVisibility::Hidden) {
            continue;
        }

        const int row = m_modelTable->rowCount();
        m_modelTable->insertRow(row);

        auto* modelItem = readOnlyItem(entry.id);
        modelItem->setData(kModelIdRole, entry.id);
        m_modelTable->setItem(row, ModelIdColumn, modelItem);
        m_modelTable->setItem(row, ModelVisibilityColumn, readOnlyItem(visibilityText(entry.visibility)));
        m_modelTable->setItem(row, ModelDriverColumn, readOnlyItem(entry.driverProfileId));
        m_modelTable->setItem(row, ModelReasonColumn, readOnlyItem(entry.filterReason));
    }

    if (m_modelTable->rowCount() > 0) {
        m_modelTable->selectRow(0);
    }
    if (m_testModelButton) {
        m_testModelButton->setEnabled(m_modelTable->rowCount() > 0);
    }
}

void ProviderManagementDialog::populateRulesTable()
{
    if (!m_rulesTable) {
        return;
    }

    auto rules = ModelCapsRegistry::instance().modelRulesList();
    std::sort(rules.begin(), rules.end(), [](const auto& lhs, const auto& rhs) {
        const int providerCompare = lhs.backend.localeAwareCompare(rhs.backend);
        if (providerCompare != 0) {
            return providerCompare < 0;
        }
        if (lhs.priority != rhs.priority) {
            return lhs.priority > rhs.priority;
        }
        return lhs.id.localeAwareCompare(rhs.id) < 0;
    });

    m_rulesTable->setRowCount(0);
    for (const auto& rule : rules) {
        const int row = m_rulesTable->rowCount();
        m_rulesTable->insertRow(row);

        auto* ruleItem = readOnlyItem(rule.id);
        ruleItem->setData(kRuleJsonRole,
                          QString::fromUtf8(QJsonDocument(ruleToOverrideObject(rule)).toJson(QJsonDocument::Compact)));
        m_rulesTable->setItem(row, RuleIdColumn, ruleItem);
        m_rulesTable->setItem(row, RuleProviderColumn, readOnlyItem(rule.backend));
        m_rulesTable->setItem(row, RulePriorityColumn, readOnlyItem(QString::number(rule.priority)));
        m_rulesTable->setItem(row, RulePatternColumn, readOnlyItem(rule.pattern.pattern()));
        m_rulesTable->setItem(row, RuleDriverColumn, readOnlyItem(rule.driverProfileId));
        m_rulesTable->setItem(row, RuleCapabilitiesColumn, readOnlyItem(capabilityTexts(rule.caps.capabilities).join(QStringLiteral(", "))));
    }

    if (m_rulesTable->rowCount() > 0) {
        m_rulesTable->selectRow(0);
    }
}

void ProviderManagementDialog::onSaveProviders()
{
    bool ok = false;
    QJsonObject root = readCatalogEditor(&ok);
    if (!ok) {
        return;
    }

    QList<QJsonObject> overrides;
    for (int row = 0; row < m_providerTable->rowCount(); ++row) {
        const auto* nameItem = m_providerTable->item(row, ProviderNameColumn);
        if (!nameItem) {
            continue;
        }

        const QString providerId = nameItem->data(kProviderIdRole).toString();
        if (providerId.isEmpty()) {
            continue;
        }

        QJsonObject provider;
        provider.insert(QStringLiteral("id"), providerId);
        provider.insert(QStringLiteral("name"), providerDisplayName(providerId));
        provider.insert(QStringLiteral("enabled"),
                        m_providerTable->item(row, ProviderEnabledColumn)->checkState() == Qt::Checked);
        provider.insert(QStringLiteral("requiresCredential"),
                        m_providerTable->item(row, ProviderRequiresKeyColumn)->checkState() == Qt::Checked);

        const QString baseUrl = m_providerTable->item(row, ProviderBaseUrlColumn)->text().trimmed();
        if (!baseUrl.isEmpty()) {
            provider.insert(QStringLiteral("baseUrl"), baseUrl);
        }
        overrides.append(provider);
    }

    QJsonArray providers = mergeProviderArray(root.value(QStringLiteral("providers")).toArray(), overrides);
    for (int row = 0; row < m_providerTable->rowCount(); ++row) {
        const auto* nameItem = m_providerTable->item(row, ProviderNameColumn);
        const auto* baseUrlItem = m_providerTable->item(row, ProviderBaseUrlColumn);
        if (!nameItem || !baseUrlItem || !baseUrlItem->text().trimmed().isEmpty()) {
            continue;
        }
        removeProviderKeys(providers,
                           nameItem->data(kProviderIdRole).toString(),
                           {QStringLiteral("baseUrl"), QStringLiteral("base_url")});
    }

    root.insert(QStringLiteral("providers"), providers);
    if (!writeUserCatalogFile(root)) {
        return;
    }

    m_catalogEditor->setPlainText(QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented)));
    reloadRegistryAndViews();
}

void ProviderManagementDialog::onReloadCatalogEditor()
{
    updateCatalogLocationLabels();
    const QJsonObject root = readUserCatalogFile();
    if (m_catalogEditor) {
        m_catalogEditor->setPlainText(QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented)));
    }
}

void ProviderManagementDialog::onFormatCatalogEditor()
{
    bool ok = false;
    const QJsonObject root = readCatalogEditor(&ok);
    if (!ok) {
        return;
    }
    m_catalogEditor->setPlainText(QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented)));
}

void ProviderManagementDialog::onSaveCatalogEditor()
{
    bool ok = false;
    const QJsonObject root = readCatalogEditor(&ok);
    if (!ok || !writeUserCatalogFile(root)) {
        return;
    }
    reloadRegistryAndViews();
}

void ProviderManagementDialog::onResetCatalogEditor()
{
    const QString path = userCatalogPath();
    if (QFileInfo::exists(path)) {
        const auto answer = QMessageBox::question(
            this,
            tr("Reset Catalog"),
            tr("Remove the local user copy and reload the distribution baseline?\n\n%1").arg(path),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }

        QFile file(path);
        if (!file.remove()) {
            QMessageBox::warning(this,
                                 tr("Reset Failed"),
                                 tr("Could not remove the user catalog copy:\n%1").arg(file.errorString()));
            return;
        }
    }

    if (m_catalogEditor) {
        m_catalogEditor->setPlainText(QString::fromUtf8(QJsonDocument(QJsonObject{}).toJson(QJsonDocument::Indented)));
    }
    reloadRegistryAndViews();
}

void ProviderManagementDialog::reloadRegistryAndViews()
{
    ModelCapsRegistry::instance().loadFromFileWithUserOverrides(ModelCapsRegistry::instance().distributionConfigPath());
    updateCatalogLocationLabels();
    populateProviderTable();
    populateProviderCombo();
    populateRulesTable();
    onRefreshModels();
}

void ProviderManagementDialog::onRefreshModels()
{
    const QString providerId = selectedProviderId();
    if (providerId.isEmpty()) {
        return;
    }
    if (m_modelFetcher.isRunning()) {
        return;
    }

    if (m_refreshModelsButton) {
        m_refreshModelsButton->setEnabled(false);
    }
    if (m_modelProviderCombo) {
        m_modelProviderCombo->setEnabled(false);
    }
    if (m_modelKindCombo) {
        m_modelKindCombo->setEnabled(false);
    }
    if (m_testModelButton) {
        m_testModelButton->setEnabled(false);
    }
    if (m_testStatusLabel) {
        m_testStatusLabel->clear();
    }
    if (m_modelTable) {
        m_modelTable->setRowCount(0);
    }

    m_modelFetcher.setFuture(ModelCatalogService::instance().fetchModels(providerId, selectedCatalogKind()));
}

void ProviderManagementDialog::onModelsFetched()
{
    m_lastModels = m_modelFetcher.result();
    if (m_lastModels.isEmpty()) {
        m_lastModels = ModelCatalogService::instance().fallbackModels(selectedProviderId(), selectedCatalogKind());
    }
    populateModelTable(m_lastModels);
    if (m_refreshModelsButton) {
        m_refreshModelsButton->setEnabled(true);
    }
    if (m_modelProviderCombo) {
        m_modelProviderCombo->setEnabled(true);
    }
    if (m_modelKindCombo) {
        m_modelKindCombo->setEnabled(true);
    }
}

void ProviderManagementDialog::onTestSelectedModel()
{
    const QString providerId = selectedProviderId();
    const QString modelId = selectedModelId();
    if (providerId.isEmpty() || modelId.isEmpty() || m_modelTester.isRunning()) {
        return;
    }

    if (m_testModelButton) {
        m_testModelButton->setEnabled(false);
    }
    if (m_testStatusLabel) {
        m_testStatusLabel->setText(tr("Testing..."));
    }

    m_modelTester.setFuture(ModelCatalogService::instance().testModel(providerId, modelId, selectedCatalogKind()));
}

void ProviderManagementDialog::onModelTestFinished()
{
    const ModelTestResult result = m_modelTester.result();
    if (m_testModelButton) {
        m_testModelButton->setEnabled(!selectedModelId().isEmpty());
    }
    if (m_testStatusLabel) {
        const QString driver = result.driverProfileId.isEmpty()
                                   ? QString()
                                   : tr(" [%1]").arg(result.driverProfileId);
        m_testStatusLabel->setText(result.success
                                       ? tr("OK%1: %2").arg(driver, result.message)
                                       : tr("Failed%1: %2").arg(driver, result.message));
    }
}

void ProviderManagementDialog::onCopyRuleToCatalogEditor()
{
    if (!m_rulesTable || m_rulesTable->currentRow() < 0) {
        return;
    }

    const auto* item = m_rulesTable->item(m_rulesTable->currentRow(), RuleIdColumn);
    if (!item) {
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument ruleDoc = QJsonDocument::fromJson(item->data(kRuleJsonRole).toString().toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !ruleDoc.isObject()) {
        QMessageBox::warning(this, tr("Copy Failed"), tr("Could not serialize the selected rule."));
        return;
    }

    bool ok = false;
    QJsonObject root = readCatalogEditor(&ok);
    if (!ok) {
        return;
    }

    const QJsonObject ruleObj = ruleDoc.object();
    QJsonArray rules = root.value(QStringLiteral("rules")).toArray();
    const QString id = ruleObj.value(QStringLiteral("id")).toString();
    int existingIndex = -1;
    if (!id.isEmpty()) {
        for (int i = 0; i < rules.size(); ++i) {
            if (rules.at(i).isObject() && rules.at(i).toObject().value(QStringLiteral("id")).toString() == id) {
                existingIndex = i;
                break;
            }
        }
    }

    if (existingIndex >= 0) {
        rules.replace(existingIndex, ruleObj);
    } else {
        rules.append(ruleObj);
    }
    root.insert(QStringLiteral("rules"), rules);

    m_catalogEditor->setPlainText(QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented)));
}
