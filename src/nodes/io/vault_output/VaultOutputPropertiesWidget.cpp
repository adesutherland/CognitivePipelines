#include "VaultOutputPropertiesWidget.h"

#include "ai/backends/ILLMBackend.h"
#include "ai/registry/LLMProviderRegistry.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

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
    const auto backends = LLMProviderRegistry::instance().allBackends();
    for (ILLMBackend* backend : backends) {
        if (backend) {
            m_providerCombo->addItem(backend->name(), backend->id());
        }
    }
    form->addRow(tr("Provider:"), m_providerCombo);

    m_modelCombo = new QComboBox(this);
    form->addRow(tr("Model:"), m_modelCombo);

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
    connect(&m_modelFetcher, &QFutureWatcher<QStringList>::finished,
            this, &VaultOutputPropertiesWidget::onModelsFetched);
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
    m_pendingModelId.clear();

    if (ILLMBackend* backend = LLMProviderRegistry::instance().getBackend(providerId)) {
        m_modelFetcher.setFuture(backend->fetchModelList());
    }

    emit providerChanged(providerId);
}

void VaultOutputPropertiesWidget::onModelIndexChanged(int index)
{
    if (!m_modelCombo || index < 0) {
        return;
    }

    emit modelChanged(m_modelCombo->itemData(index).toString());
}

void VaultOutputPropertiesWidget::onModelsFetched()
{
    if (!m_modelCombo) {
        return;
    }

    QStringList models = m_modelFetcher.result();
    const QString providerId = provider();
    if (models.isEmpty()) {
        if (ILLMBackend* backend = LLMProviderRegistry::instance().getBackend(providerId)) {
            models = backend->availableModels();
        }
    }

    int selectedIndex = 0;
    {
        const QSignalBlocker blocker(m_modelCombo);
        m_modelCombo->clear();
        for (const QString& model : models) {
            m_modelCombo->addItem(model, model);
        }
        if (!m_pendingModelId.isEmpty()) {
            const int pendingIndex = m_modelCombo->findData(m_pendingModelId);
            if (pendingIndex >= 0) {
                selectedIndex = pendingIndex;
            }
        }
        if (m_modelCombo->count() > 0) {
            m_modelCombo->setCurrentIndex(selectedIndex);
        }
    }

    m_modelCombo->setEnabled(true);
    if (m_modelCombo->count() > 0) {
        emit modelChanged(m_modelCombo->currentData().toString());
    }
}
