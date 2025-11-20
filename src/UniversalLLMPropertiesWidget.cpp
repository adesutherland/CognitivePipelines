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

#include <QComboBox>
#include <QTextEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QFormLayout>
#include <QSignalBlocker>

UniversalLLMPropertiesWidget::UniversalLLMPropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* form = new QFormLayout(this);
    form->setContentsMargins(4, 4, 4, 4);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    // Provider combo box
    m_providerCombo = new QComboBox(this);
    
    // Populate providers from LLMProviderRegistry
    const auto backends = LLMProviderRegistry::instance().allBackends();
    for (ILLMBackend* backend : backends) {
        if (backend) {
            m_providerCombo->addItem(backend->name(), backend->id());
        }
    }
    form->addRow(tr("Provider:"), m_providerCombo);

    // Model combo box
    m_modelCombo = new QComboBox(this);
    form->addRow(tr("Model:"), m_modelCombo);

    // System Prompt text edit
    m_systemPromptEdit = new QTextEdit(this);
    m_systemPromptEdit->setPlaceholderText(tr("Enter system instructions..."));
    m_systemPromptEdit->setAcceptRichText(false);
    m_systemPromptEdit->setMaximumHeight(100);
    form->addRow(tr("System Prompt:"), m_systemPromptEdit);

    // User Prompt text edit
    m_userPromptEdit = new QTextEdit(this);
    m_userPromptEdit->setPlaceholderText(tr("Enter user prompt..."));
    m_userPromptEdit->setAcceptRichText(false);
    m_userPromptEdit->setMaximumHeight(100);
    form->addRow(tr("User Prompt:"), m_userPromptEdit);

    // Temperature spin box
    m_temperatureSpinBox = new QDoubleSpinBox(this);
    m_temperatureSpinBox->setRange(0.0, 2.0);
    m_temperatureSpinBox->setSingleStep(0.1);
    m_temperatureSpinBox->setDecimals(2);
    m_temperatureSpinBox->setValue(0.7);
    form->addRow(tr("Temperature:"), m_temperatureSpinBox);

    // Max Tokens spin box
    m_maxTokensSpinBox = new QSpinBox(this);
    m_maxTokensSpinBox->setRange(1, 100000);
    m_maxTokensSpinBox->setValue(1024);
    form->addRow(tr("Max Tokens:"), m_maxTokensSpinBox);

    // Connect signals
    connect(m_providerCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &UniversalLLMPropertiesWidget::onProviderChanged);
    
    connect(m_modelCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        if (m_modelCombo->currentIndex() >= 0) {
            emit modelChanged(m_modelCombo->currentData().toString());
        }
    });

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
    
    // Clear existing models and repopulate
    {
        const QSignalBlocker blocker(m_modelCombo);
        m_modelCombo->clear();

        // Get backend and populate models
        ILLMBackend* backend = LLMProviderRegistry::instance().getBackend(providerId);
        if (backend) {
            const QStringList models = backend->availableModels();
            for (const QString& model : models) {
                m_modelCombo->addItem(model, model);
            }
        }
        
        // Set default model to first item if available
        if (m_modelCombo->count() > 0) {
            m_modelCombo->setCurrentIndex(0);
        }
    } // QSignalBlocker goes out of scope here

    // Emit provider changed signal
    emit providerChanged(providerId);
    
    // Explicitly emit modelChanged signal with the new default model
    // This is crucial because QSignalBlocker prevented the automatic signal
    if (m_modelCombo->count() > 0 && m_modelCombo->currentIndex() >= 0) {
        emit modelChanged(m_modelCombo->currentData().toString());
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
