#include "VaultOutputPropertiesWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

QString providerDisplayText(const ProviderCatalogEntry& entry)
{
    return QStringLiteral("%1 (%2)").arg(entry.name, entry.statusText);
}

void addSectionHeader(QComboBox* combo, const QString& text)
{
    combo->addItem(text, QString());
    if (auto* model = qobject_cast<QStandardItemModel*>(combo->model())) {
        if (auto* item = model->item(combo->count() - 1)) {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        }
    }
}

QString modelDisplayText(const ModelCatalogEntry& entry)
{
    QString text = entry.displayName.isEmpty() ? entry.id : entry.displayName;
    if (text != entry.id) {
        text += QStringLiteral(" [%1]").arg(entry.id);
    }
    if (!entry.driverProfileId.isEmpty()) {
        text += QStringLiteral(" - %1").arg(entry.driverProfileId);
    }
    if (entry.visibility == ModelCatalogVisibility::Hidden && !entry.filterReason.isEmpty()) {
        text += QStringLiteral(" - Investigate: %1").arg(entry.filterReason);
    } else if (!entry.description.isEmpty()) {
        text += QStringLiteral(" - %1").arg(entry.description);
    }
    return text;
}

void addModelEntry(QComboBox* combo, const ModelCatalogEntry& entry)
{
    combo->addItem(modelDisplayText(entry), entry.id);
    if (entry.visibility == ModelCatalogVisibility::Hidden) {
        if (auto* model = qobject_cast<QStandardItemModel*>(combo->model())) {
            if (auto* item = model->item(combo->count() - 1)) {
                item->setForeground(Qt::darkGray);
            }
        }
    }
}

void addModelGroup(QComboBox* combo,
                   const QList<ModelCatalogEntry>& entries,
                   ModelCatalogVisibility visibility,
                   const QString& header)
{
    bool hasGroup = false;
    for (const auto& entry : entries) {
        if (entry.visibility == visibility) {
            hasGroup = true;
            break;
        }
    }
    if (!hasGroup) {
        return;
    }

    addSectionHeader(combo, header);
    for (const auto& entry : entries) {
        if (entry.visibility == visibility) {
            addModelEntry(combo, entry);
        }
    }
}

} // namespace

VaultOutputPropertiesWidget::VaultOutputPropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);

    auto* form = new QFormLayout();

    m_vaultRootEdit = new QLineEdit(this);
    m_browseVaultRootButton = new QPushButton(tr("Browse..."), this);
    auto* rootRow = new QHBoxLayout();
    rootRow->addWidget(m_vaultRootEdit);
    rootRow->addWidget(m_browseVaultRootButton);
    form->addRow(tr("Vault Root:"), rootRow);

    m_providerCombo = new QComboBox(this);
    const auto providers = ModelCatalogService::instance().providers(ModelCatalogKind::Chat);
    for (const auto& provider : providers) {
        m_providerCombo->addItem(providerDisplayText(provider), provider.id);
    }
    form->addRow(tr("Provider:"), m_providerCombo);

    m_modelCombo = new QComboBox(this);
    form->addRow(tr("Model:"), m_modelCombo);

    m_showFilteredCheck = new QCheckBox(tr("Show filtered models"), this);
    m_showFilteredCheck->setToolTip(tr("Show provider models hidden by capability or driver rules."));
    form->addRow(QString(), m_showFilteredCheck);

    auto* testRow = new QHBoxLayout();
    m_testModelButton = new QPushButton(tr("Test Selection"), this);
    m_testStatusLabel = new QLabel(this);
    m_testStatusLabel->setWordWrap(true);
    testRow->addWidget(m_testModelButton);
    testRow->addWidget(m_testStatusLabel, 1);
    form->addRow(QString(), testRow);

    m_temperatureSpinBox = new QDoubleSpinBox(this);
    m_temperatureSpinBox->setRange(0.0, 2.0);
    m_temperatureSpinBox->setDecimals(2);
    m_temperatureSpinBox->setSingleStep(0.1);
    m_temperatureSpinBox->setValue(0.2);
    form->addRow(tr("Temperature:"), m_temperatureSpinBox);

    m_maxTokensSpinBox = new QSpinBox(this);
    m_maxTokensSpinBox->setRange(64, 100000);
    m_maxTokensSpinBox->setValue(800);
    form->addRow(tr("Max Tokens:"), m_maxTokensSpinBox);

    layout->addLayout(form);

    layout->addWidget(new QLabel(tr("Routing Prompt:"), this));
    m_routingPromptEdit = new QTextEdit(this);
    m_routingPromptEdit->setAcceptRichText(false);
    m_routingPromptEdit->setMaximumHeight(180);
    layout->addWidget(m_routingPromptEdit);

    layout->addStretch();

    connect(m_browseVaultRootButton, &QPushButton::clicked,
            this, &VaultOutputPropertiesWidget::onBrowseVaultRoot);
    connect(m_vaultRootEdit, &QLineEdit::textChanged,
            this, &VaultOutputPropertiesWidget::vaultRootChanged);
    connect(m_providerCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &VaultOutputPropertiesWidget::onProviderIndexChanged);
    connect(m_modelCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &VaultOutputPropertiesWidget::onModelIndexChanged);
    connect(&m_modelFetcher, &QFutureWatcher<QList<ModelCatalogEntry>>::finished,
            this, &VaultOutputPropertiesWidget::onModelsFetched);
    connect(&m_modelTester, &QFutureWatcher<ModelTestResult>::finished,
            this, &VaultOutputPropertiesWidget::onModelTestFinished);
    connect(m_showFilteredCheck, &QCheckBox::toggled,
            this, &VaultOutputPropertiesWidget::onShowFilteredChanged);
    connect(m_testModelButton, &QPushButton::clicked,
            this, &VaultOutputPropertiesWidget::onTestModelClicked);
    connect(m_routingPromptEdit, &QTextEdit::textChanged, this, [this]() {
        emit routingPromptChanged(m_routingPromptEdit->toPlainText());
    });
    connect(m_temperatureSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &VaultOutputPropertiesWidget::temperatureChanged);
    connect(m_maxTokensSpinBox, qOverload<int>(&QSpinBox::valueChanged),
            this, &VaultOutputPropertiesWidget::maxTokensChanged);

    if (m_providerCombo->count() > 0) {
        onProviderIndexChanged(0);
    }
}

void VaultOutputPropertiesWidget::setVaultRoot(const QString& path)
{
    if (!m_vaultRootEdit || m_vaultRootEdit->text() == path) {
        return;
    }

    const QSignalBlocker blocker(m_vaultRootEdit);
    m_vaultRootEdit->setText(path);
}

void VaultOutputPropertiesWidget::setProvider(const QString& providerId)
{
    if (!m_providerCombo) {
        return;
    }

    const QSignalBlocker blocker(m_providerCombo);
    for (int i = 0; i < m_providerCombo->count(); ++i) {
        if (m_providerCombo->itemData(i).toString() == providerId) {
            m_providerCombo->setCurrentIndex(i);
            onProviderIndexChanged(i);
            break;
        }
    }
}

void VaultOutputPropertiesWidget::setModel(const QString& modelId)
{
    if (!m_modelCombo) {
        return;
    }

    m_pendingModelId = modelId;
    const QSignalBlocker blocker(m_modelCombo);
    for (int i = 0; i < m_modelCombo->count(); ++i) {
        if (m_modelCombo->itemData(i).toString() == modelId) {
            m_modelCombo->setCurrentIndex(i);
            break;
        }
    }
}

void VaultOutputPropertiesWidget::setRoutingPrompt(const QString& prompt)
{
    if (!m_routingPromptEdit || m_routingPromptEdit->toPlainText() == prompt) {
        return;
    }

    const QSignalBlocker blocker(m_routingPromptEdit);
    m_routingPromptEdit->setPlainText(prompt);
}

void VaultOutputPropertiesWidget::setTemperature(double value)
{
    if (!m_temperatureSpinBox) {
        return;
    }

    const QSignalBlocker blocker(m_temperatureSpinBox);
    m_temperatureSpinBox->setValue(value);
}

void VaultOutputPropertiesWidget::setMaxTokens(int value)
{
    if (!m_maxTokensSpinBox) {
        return;
    }

    const QSignalBlocker blocker(m_maxTokensSpinBox);
    m_maxTokensSpinBox->setValue(value);
}

QString VaultOutputPropertiesWidget::vaultRoot() const
{
    return m_vaultRootEdit ? m_vaultRootEdit->text() : QString();
}

QString VaultOutputPropertiesWidget::provider() const
{
    return m_providerCombo ? m_providerCombo->currentData().toString() : QString();
}

QString VaultOutputPropertiesWidget::model() const
{
    return m_modelCombo ? m_modelCombo->currentData().toString() : QString();
}

QString VaultOutputPropertiesWidget::routingPrompt() const
{
    return m_routingPromptEdit ? m_routingPromptEdit->toPlainText() : QString();
}

double VaultOutputPropertiesWidget::temperature() const
{
    return m_temperatureSpinBox ? m_temperatureSpinBox->value() : 0.2;
}

int VaultOutputPropertiesWidget::maxTokens() const
{
    return m_maxTokensSpinBox ? m_maxTokensSpinBox->value() : 800;
}

void VaultOutputPropertiesWidget::onBrowseVaultRoot()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Vault Root"),
        m_vaultRootEdit ? m_vaultRootEdit->text() : QString());
    if (!dir.isEmpty() && m_vaultRootEdit) {
        m_vaultRootEdit->setText(dir);
    }
}

void VaultOutputPropertiesWidget::onProviderIndexChanged(int index)
{
    if (index < 0 || !m_providerCombo || !m_modelCombo) {
        return;
    }

    const QString providerId = m_providerCombo->itemData(index).toString();
    {
        const QSignalBlocker blocker(m_modelCombo);
        m_modelCombo->clear();
        m_modelCombo->addItem(tr("Fetching..."));
    }
    m_modelCombo->setEnabled(false);
    if (m_testModelButton) {
        m_testModelButton->setEnabled(false);
    }
    if (m_testStatusLabel) {
        m_testStatusLabel->clear();
    }
    m_pendingModelId.clear();

    m_modelFetcher.setFuture(ModelCatalogService::instance().fetchModels(providerId, ModelCatalogKind::Chat));

    emit providerChanged(providerId);
}

void VaultOutputPropertiesWidget::onModelIndexChanged(int index)
{
    if (!m_modelCombo || index < 0) {
        return;
    }

    const QString modelId = m_modelCombo->itemData(index).toString();
    if (modelId.isEmpty()) {
        return;
    }
    if (m_testModelButton) {
        m_testModelButton->setEnabled(true);
    }
    if (m_testStatusLabel && !m_modelTester.isRunning()) {
        m_testStatusLabel->clear();
    }
    emit modelChanged(modelId);
}

void VaultOutputPropertiesWidget::populateModelCombo(const QList<ModelCatalogEntry>& models)
{
    if (!m_modelCombo) {
        return;
    }

    const QString desiredModelId = !m_pendingModelId.isEmpty()
                                       ? m_pendingModelId
                                       : m_modelCombo->currentData().toString();
    bool pendingIsHidden = false;
    for (const auto& model : models) {
        if (model.id == desiredModelId && model.visibility == ModelCatalogVisibility::Hidden) {
            pendingIsHidden = true;
            break;
        }
    }

    const bool includeHidden = (m_showFilteredCheck && m_showFilteredCheck->isChecked()) || pendingIsHidden;
    int selectedIndex = -1;
    bool selectedFound = false;
    {
        const QSignalBlocker blocker(m_modelCombo);
        m_modelCombo->clear();

        addModelGroup(m_modelCombo, models, ModelCatalogVisibility::Recommended, tr("Recommended"));
        addModelGroup(m_modelCombo, models, ModelCatalogVisibility::Available, tr("Available"));
        if (includeHidden) {
            addModelGroup(m_modelCombo, models, ModelCatalogVisibility::Hidden, tr("Investigate"));
        }

        if (!desiredModelId.isEmpty()) {
            selectedIndex = m_modelCombo->findData(desiredModelId);
            selectedFound = selectedIndex >= 0;
        }
        if (selectedIndex < 0) {
            for (int i = 0; i < m_modelCombo->count(); ++i) {
                if (!m_modelCombo->itemData(i).toString().isEmpty()) {
                    selectedIndex = i;
                    break;
                }
            }
        }
        if (selectedIndex >= 0) {
            m_modelCombo->setCurrentIndex(selectedIndex);
        }
    }

    m_modelCombo->setEnabled(selectedIndex >= 0);
    if (m_testModelButton) {
        m_testModelButton->setEnabled(selectedIndex >= 0);
    }
    if (selectedIndex >= 0 && !m_modelCombo->currentData().toString().isEmpty()) {
        if (selectedFound && !m_pendingModelId.isEmpty()) {
            const QSignalBlocker blocker(this);
            emit modelChanged(m_modelCombo->currentData().toString());
        } else {
            emit modelChanged(m_modelCombo->currentData().toString());
        }
    }
}

void VaultOutputPropertiesWidget::onModelsFetched()
{
    m_lastModels = m_modelFetcher.result();
    const QString providerId = provider();
    if (m_lastModels.isEmpty()) {
        m_lastModels = ModelCatalogService::instance().fallbackModels(providerId, ModelCatalogKind::Chat);
    }

    populateModelCombo(m_lastModels);
}

void VaultOutputPropertiesWidget::onShowFilteredChanged(bool checked)
{
    Q_UNUSED(checked)
    m_pendingModelId = m_modelCombo ? m_modelCombo->currentData().toString() : QString();
    populateModelCombo(m_lastModels);
}

void VaultOutputPropertiesWidget::onTestModelClicked()
{
    if (m_modelTester.isRunning()) {
        return;
    }

    const QString providerId = provider();
    const QString modelId = model();
    if (providerId.isEmpty() || modelId.isEmpty()) {
        return;
    }

    if (m_testModelButton) {
        m_testModelButton->setEnabled(false);
    }
    if (m_testStatusLabel) {
        m_testStatusLabel->setText(tr("Testing..."));
    }

    m_modelTester.setFuture(ModelCatalogService::instance().testModel(providerId, modelId, ModelCatalogKind::Chat));
}

void VaultOutputPropertiesWidget::onModelTestFinished()
{
    const ModelTestResult result = m_modelTester.result();
    if (m_testModelButton) {
        m_testModelButton->setEnabled(m_modelCombo && !m_modelCombo->currentData().toString().isEmpty());
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
