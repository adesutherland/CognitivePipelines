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
#include "UniversalLLMPropertiesWidget.h"
#include "core/LLMProviderRegistry.h"
#include "backends/ILLMBackend.h"
#include "ModelCapsRegistry.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QTextEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QGroupBox>
#include <QFormLayout>
#include <QSignalBlocker>
#include "Logger.h"
#include "logging_categories.h"

UniversalLLMPropertiesWidget::UniversalLLMPropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);

    // Provider label
    auto* providerLabel = new QLabel(tr("Provider:"), this);
    layout->addWidget(providerLabel);

    // Provider combo box
    m_providerCombo = new QComboBox(this);
    
    // Populate providers from LLMProviderRegistry
    const auto backends = LLMProviderRegistry::instance().allBackends();
    for (ILLMBackend* backend : backends) {
        if (backend) {
            m_providerCombo->addItem(backend->name(), backend->id());
        }
    }
    layout->addWidget(m_providerCombo);

    // Model label
    auto* modelLabel = new QLabel(tr("Model:"), this);
    layout->addWidget(modelLabel);

    // Model combo box
    m_modelCombo = new QComboBox(this);
    layout->addWidget(m_modelCombo);

    // System Prompt label
    auto* systemPromptLabel = new QLabel(tr("System Prompt:"), this);
    layout->addWidget(systemPromptLabel);

    // System Prompt text edit
    m_systemPromptEdit = new QTextEdit(this);
    m_systemPromptEdit->setPlaceholderText(tr("Enter system instructions..."));
    m_systemPromptEdit->setAcceptRichText(false);
    m_systemPromptEdit->setMaximumHeight(100);
    layout->addWidget(m_systemPromptEdit);

    // User Prompt label
    auto* userPromptLabel = new QLabel(tr("User Prompt:"), this);
    layout->addWidget(userPromptLabel);

    // User Prompt text edit
    m_userPromptEdit = new QTextEdit(this);
    m_userPromptEdit->setPlaceholderText(tr("Enter user prompt..."));
    m_userPromptEdit->setAcceptRichText(false);
    m_userPromptEdit->setMaximumHeight(100);
    layout->addWidget(m_userPromptEdit);

    // Temperature label
    auto* temperatureLabel = new QLabel(tr("Temperature:"), this);
    layout->addWidget(temperatureLabel);

    // Temperature spin box
    m_temperatureSpinBox = new QDoubleSpinBox(this);
    m_temperatureSpinBox->setRange(0.0, 2.0);
    m_temperatureSpinBox->setSingleStep(0.1);
    m_temperatureSpinBox->setDecimals(2);
    m_temperatureSpinBox->setValue(0.7);
    layout->addWidget(m_temperatureSpinBox);

    // Max Tokens label
    auto* maxTokensLabel = new QLabel(tr("Max Tokens:"), this);
    layout->addWidget(maxTokensLabel);

    // Max Tokens spin box
    m_maxTokensSpinBox = new QSpinBox(this);
    m_maxTokensSpinBox->setRange(1, 100000);
    m_maxTokensSpinBox->setValue(1024);
    layout->addWidget(m_maxTokensSpinBox);

    // Resilience & Fallback Group
    auto* fallbackGroup = new QGroupBox(tr("Resilience & Fallback"), this);
    auto* fallbackLayout = new QFormLayout(fallbackGroup);
    fallbackLayout->setContentsMargins(4, 8, 4, 4);
    fallbackLayout->setSpacing(8);

    m_enableFallbackCheck = new QCheckBox(tr("Enable Soft Fallback"), this);
    fallbackLayout->addRow(m_enableFallbackCheck);

    m_fallbackStringEdit = new QLineEdit(this);
    m_fallbackStringEdit->setPlaceholderText(tr("Fallback string (e.g. FAIL)"));
    m_fallbackStringEdit->setText(QStringLiteral("FAIL"));
    m_fallbackStringEdit->setEnabled(false); // Default disabled
    fallbackLayout->addRow(tr("Fallback Output String:"), m_fallbackStringEdit);

    layout->addWidget(fallbackGroup);

    layout->addStretch();

    // Connect signals
    connect(m_providerCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &UniversalLLMPropertiesWidget::onProviderChanged);
    
    connect(m_modelCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &UniversalLLMPropertiesWidget::onModelChanged);

    // Async model discovery watcher
    connect(&m_modelFetcher, &QFutureWatcher<QStringList>::finished,
            this, &UniversalLLMPropertiesWidget::onModelsFetched);

    connect(m_systemPromptEdit, &QTextEdit::textChanged, this, [this]() {
        emit systemPromptChanged(m_systemPromptEdit->toPlainText());
    });

    connect(m_userPromptEdit, &QTextEdit::textChanged, this, [this]() {
        emit userPromptChanged(m_userPromptEdit->toPlainText());
    });

    connect(m_temperatureSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &UniversalLLMPropertiesWidget::temperatureChanged);

    connect(m_maxTokensSpinBox, qOverload<int>(&QSpinBox::valueChanged),
            this, &UniversalLLMPropertiesWidget::maxTokensChanged);

    connect(m_enableFallbackCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_fallbackStringEdit->setEnabled(checked);
        emit enableFallbackChanged(checked);
    });

    connect(m_fallbackStringEdit, &QLineEdit::textChanged, this, &UniversalLLMPropertiesWidget::fallbackStringChanged);

    // Initialize model list for the first provider
    if (m_providerCombo->count() > 0) {
        onProviderChanged(0);
    }
}

void UniversalLLMPropertiesWidget::onProviderChanged(int index)
{
    if (index < 0 || !m_modelCombo) {
        return;
    }

    const QString providerId = m_providerCombo->itemData(index).toString();
    // Instrumentation: log provider selection from the UI (debug‑gated)
    CP_CLOG(cp_lifecycle).noquote() << "[ModelLifecycle] UI: providerChanged -> providerId=" << providerId;

    m_pendingModelId.clear(); // Switching providers invalidates the pending model

    // Clear existing models and set loading state
    {
        const QSignalBlocker blocker(m_modelCombo);
        m_modelCombo->clear();
        m_modelCombo->addItem(tr("Fetching..."));
    }
    m_modelCombo->setEnabled(false);

    // Kick off async fetch via backend
    if (ILLMBackend* backend = LLMProviderRegistry::instance().getBackend(providerId)) {
        CP_CLOG(cp_discovery).noquote() << QStringLiteral("Requesting live Model List for Provider [%1]...")
                                 .arg(backend->name());
        QFuture<QStringList> fut = backend->fetchModelList();
        m_modelFetcher.setFuture(fut);
    }

    // Emit provider changed signal (capabilities will be re-evaluated after models arrive)
    emit providerChanged(providerId);
}

void UniversalLLMPropertiesWidget::onModelChanged(int index)
{
    if (!m_modelCombo || index < 0) {
        return;
    }

    const QString modelId = m_modelCombo->itemData(index).toString();
    // Instrumentation: log model selection from the UI (debug‑gated)
    CP_CLOG(cp_lifecycle).noquote() << "[ModelLifecycle] UI: modelChanged -> modelId=" << modelId;
    emit modelChanged(modelId);

    const QString providerId = m_providerCombo ? m_providerCombo->currentData().toString() : QString();
    const auto caps = ModelCapsRegistry::instance().resolve(modelId, providerId);
    const bool hasVision = caps.has_value() && caps->hasCapability(ModelCapsTypes::Capability::Vision);
    const bool hasReasoning = caps.has_value() && caps->hasCapability(ModelCapsTypes::Capability::Reasoning);
    const bool omitTemp = caps.has_value() && caps->constraints.omitTemperature.value_or(false);

    if (m_temperatureSpinBox) {
        if (hasReasoning || omitTemp) {
            m_temperatureSpinBox->setEnabled(false);
        } else {
            m_temperatureSpinBox->setEnabled(true);
        }

        if (caps.has_value() && caps->constraints.temperature.has_value()) {
            const auto& temp = *caps->constraints.temperature;
            if (temp.defaultValue.has_value()) {
                const QSignalBlocker blocker(m_temperatureSpinBox);
                m_temperatureSpinBox->setValue(*temp.defaultValue);
            }
        }
    }

    const QString tempConstraint = QStringLiteral("N/A");
    const QString logLine = QStringLiteral("UI Enforcement: Selected %1. Vision=%2, Reasoning=%3. Constraints: Temp=[%4]")
            .arg(modelId,
                 hasVision ? QStringLiteral("T") : QStringLiteral("F"),
                 hasReasoning ? QStringLiteral("T") : QStringLiteral("F"),
                 tempConstraint);

    CP_CLOG(cp_lifecycle).noquote() << logLine;
}

void UniversalLLMPropertiesWidget::onModelsFetched()
{
    // Obtain latest provider/ backend for potential fallback
    const QString providerId = m_providerCombo ? m_providerCombo->currentData().toString() : QString();
    ILLMBackend* backend = providerId.isEmpty() ? nullptr : LLMProviderRegistry::instance().getBackend(providerId);

    QStringList models = m_modelFetcher.result();
    if (models.isEmpty() && backend) {
        CP_WARN.noquote() << QStringLiteral("Async fetch returned empty model list for provider [%1]; falling back to static list")
                                    .arg(backend->name());
        models = backend->availableModels();
    }

    bool pendingFound = false;
    int pendingIndex = -1;
    if (!m_pendingModelId.isEmpty()) {
        for (int i = 0; i < models.size(); ++i) {
            if (models[i] == m_pendingModelId) {
                pendingFound = true;
                pendingIndex = i;
                break;
            }
        }
    }

    {
        const QSignalBlocker blocker(m_modelCombo);
        m_modelCombo->clear();
        for (const QString& m : models) {
            m_modelCombo->addItem(m, m);
        }

        if (pendingFound) {
            m_modelCombo->setCurrentIndex(pendingIndex);
        } else if (m_modelCombo->count() > 0) {
            m_modelCombo->setCurrentIndex(0);
        }
    }

    m_modelCombo->setEnabled(true);

    // Trigger downstream updates for the newly selected model
    if (m_modelCombo->count() > 0 && m_modelCombo->currentIndex() >= 0) {
        if (pendingFound) {
            // Scenario A (Found): Set current index to m_pendingModelId.
            // Crucial: Use QSignalBlocker or simply do not emit modelChanged.
            // The Node already has this value; we are just restoring the View to match the Model.
            const QSignalBlocker blocker(this);
            onModelChanged(m_modelCombo->currentIndex());
        } else {
            // Scenario B (Not Found): Select index 0 (if available) and emit modelChanged (default behavior).
            onModelChanged(m_modelCombo->currentIndex());
        }
    }
}

void UniversalLLMPropertiesWidget::setProvider(const QString& providerId)
{
    if (!m_providerCombo) return;

    const QSignalBlocker blocker(m_providerCombo);
    
    // Find and select the provider
    for (int i = 0; i < m_providerCombo->count(); ++i) {
        if (m_providerCombo->itemData(i).toString() == providerId) {
            m_providerCombo->setCurrentIndex(i);
            // Manually trigger model update since signals are blocked
            onProviderChanged(i);
            break;
        }
    }
}

void UniversalLLMPropertiesWidget::setModel(const QString& modelId)
{
    if (!m_modelCombo) return;

    m_pendingModelId = modelId;

    const QSignalBlocker blocker(m_modelCombo);
    
    // Find and select the model
    for (int i = 0; i < m_modelCombo->count(); ++i) {
        if (m_modelCombo->itemData(i).toString() == modelId) {
            m_modelCombo->setCurrentIndex(i);
            break;
        }
    }
}

void UniversalLLMPropertiesWidget::setSystemPrompt(const QString& text)
{
    if (!m_systemPromptEdit) return;
    if (m_systemPromptEdit->toPlainText() == text) return;
    
    const QSignalBlocker blocker(m_systemPromptEdit);
    m_systemPromptEdit->setPlainText(text);
}

void UniversalLLMPropertiesWidget::setUserPrompt(const QString& text)
{
    if (!m_userPromptEdit) return;
    if (m_userPromptEdit->toPlainText() == text) return;
    
    const QSignalBlocker blocker(m_userPromptEdit);
    m_userPromptEdit->setPlainText(text);
}

void UniversalLLMPropertiesWidget::setTemperature(double value)
{
    if (!m_temperatureSpinBox) return;
    
    const QSignalBlocker blocker(m_temperatureSpinBox);
    m_temperatureSpinBox->setValue(value);
}

void UniversalLLMPropertiesWidget::setMaxTokens(int value)
{
    if (!m_maxTokensSpinBox) return;
    
    const QSignalBlocker blocker(m_maxTokensSpinBox);
    m_maxTokensSpinBox->setValue(value);
}

void UniversalLLMPropertiesWidget::setEnableFallback(bool enable)
{
    if (!m_enableFallbackCheck) return;
    
    const QSignalBlocker blocker(m_enableFallbackCheck);
    m_enableFallbackCheck->setChecked(enable);
    if (m_fallbackStringEdit) {
        m_fallbackStringEdit->setEnabled(enable);
    }
}

void UniversalLLMPropertiesWidget::setFallbackString(const QString& fallback)
{
    if (!m_fallbackStringEdit) return;
    
    const QSignalBlocker blocker(m_fallbackStringEdit);
    m_fallbackStringEdit->setText(fallback);
}

QString UniversalLLMPropertiesWidget::provider() const
{
    return m_providerCombo ? m_providerCombo->currentData().toString() : QString();
}

QString UniversalLLMPropertiesWidget::model() const
{
    return m_modelCombo ? m_modelCombo->currentData().toString() : QString();
}

QString UniversalLLMPropertiesWidget::systemPrompt() const
{
    return m_systemPromptEdit ? m_systemPromptEdit->toPlainText() : QString();
}

QString UniversalLLMPropertiesWidget::userPrompt() const
{
    return m_userPromptEdit ? m_userPromptEdit->toPlainText() : QString();
}

double UniversalLLMPropertiesWidget::temperature() const
{
    return m_temperatureSpinBox ? m_temperatureSpinBox->value() : 0.7;
}

int UniversalLLMPropertiesWidget::maxTokens() const
{
    return m_maxTokensSpinBox ? m_maxTokensSpinBox->value() : 1024;
}

bool UniversalLLMPropertiesWidget::enableFallback() const
{
    return m_enableFallbackCheck ? m_enableFallbackCheck->isChecked() : false;
}

QString UniversalLLMPropertiesWidget::fallbackString() const
{
    return m_fallbackStringEdit ? m_fallbackStringEdit->text() : QStringLiteral("FAIL");
}
