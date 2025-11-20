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
#include "UniversalLLMNode.h"
#include "UniversalLLMPropertiesWidget.h"
#include "core/LLMProviderRegistry.h"
#include "backends/ILLMBackend.h"

#include <QtConcurrent>
#include <QJsonObject>
#include <QDebug>

UniversalLLMNode::UniversalLLMNode(QObject* parent)
    : QObject(parent)
{
    // Initialize with default provider if available
    auto backends = LLMProviderRegistry::instance().allBackends();
    if (!backends.isEmpty()) {
        m_providerId = backends.first()->id();
        auto models = backends.first()->availableModels();
        if (!models.isEmpty()) {
            m_modelId = models.first();
        }
    }
}

NodeDescriptor UniversalLLMNode::GetDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("universal-llm");
    desc.name = QStringLiteral("Universal AI");
    desc.category = QStringLiteral("AI Services");

    // Input pins
    PinDefinition systemPin;
    systemPin.direction = PinDirection::Input;
    systemPin.id = QString::fromLatin1(kInputSystemId);
    systemPin.name = QStringLiteral("System");
    systemPin.type = QStringLiteral("text");
    desc.inputPins.insert(systemPin.id, systemPin);

    PinDefinition promptPin;
    promptPin.direction = PinDirection::Input;
    promptPin.id = QString::fromLatin1(kInputPromptId);
    promptPin.name = QStringLiteral("Prompt");
    promptPin.type = QStringLiteral("text");
    desc.inputPins.insert(promptPin.id, promptPin);

    // Output pin
    PinDefinition responsePin;
    responsePin.direction = PinDirection::Output;
    responsePin.id = QString::fromLatin1(kOutputResponseId);
    responsePin.name = QStringLiteral("Response");
    responsePin.type = QStringLiteral("text");
    desc.outputPins.insert(responsePin.id, responsePin);

    return desc;
}

QWidget* UniversalLLMNode::createConfigurationWidget(QWidget* parent)
{
    auto* widget = new UniversalLLMPropertiesWidget(parent);

    // Initialize widget with current node state (important for loading saved files)
    widget->setProvider(m_providerId);
    widget->setModel(m_modelId);
    widget->setSystemPrompt(m_systemPrompt);
    widget->setUserPrompt(m_userPrompt);
    widget->setTemperature(m_temperature);
    widget->setMaxTokens(m_maxTokens);

    // Connect widget signals to node slots
    connect(widget, &UniversalLLMPropertiesWidget::providerChanged,
            this, &UniversalLLMNode::onProviderChanged);
    connect(widget, &UniversalLLMPropertiesWidget::modelChanged,
            this, &UniversalLLMNode::onModelChanged);
    connect(widget, &UniversalLLMPropertiesWidget::systemPromptChanged,
            this, &UniversalLLMNode::onSystemPromptChanged);
    connect(widget, &UniversalLLMPropertiesWidget::userPromptChanged,
            this, &UniversalLLMNode::onUserPromptChanged);
    connect(widget, &UniversalLLMPropertiesWidget::temperatureChanged,
            this, &UniversalLLMNode::onTemperatureChanged);
    connect(widget, &UniversalLLMPropertiesWidget::maxTokensChanged,
            this, &UniversalLLMNode::onMaxTokensChanged);

    return widget;
}

QFuture<DataPacket> UniversalLLMNode::Execute(const DataPacket& inputs)
{
    // Retrieve input pins (may override defaults from properties)
    const QString systemInput = inputs.value(QString::fromLatin1(kInputSystemId)).toString();
    const QString promptInput = inputs.value(QString::fromLatin1(kInputPromptId)).toString();

    // Copy state to use in background thread
    const QString providerId = m_providerId;
    const QString modelId = m_modelId;
    const QString systemDefault = m_systemPrompt;
    const QString userDefault = m_userPrompt;
    const double temperature = m_temperature;
    const int maxTokens = m_maxTokens;

    return QtConcurrent::run([systemInput, promptInput, providerId, modelId,
                              systemDefault, userDefault, temperature, maxTokens]() -> DataPacket {
        DataPacket output;
        // Clear the output pin at the start
        output.insert(QString::fromLatin1(kOutputResponseId), QVariant());

        // Use input pins if provided, otherwise fall back to defaults
        const QString systemPrompt = systemInput.trimmed().isEmpty() 
                                     ? systemDefault.trimmed() 
                                     : systemInput.trimmed();
        const QString userPrompt = promptInput.trimmed().isEmpty() 
                                   ? userDefault.trimmed() 
                                   : promptInput.trimmed();

        // Validate inputs
        if (systemPrompt.isEmpty() && userPrompt.isEmpty()) {
            const QString err = QStringLiteral("ERROR: Both system and user prompts are empty.");
            output.insert(QString::fromLatin1(kOutputResponseId), QVariant(err));
            output.insert(QStringLiteral("__error"), err);
            return output;
        }

        // Resolve backend using LLMProviderRegistry
        ILLMBackend* backend = LLMProviderRegistry::instance().getBackend(providerId);
        if (!backend) {
            const QString err = QStringLiteral("ERROR: Backend '%1' not found. Please check provider configuration.").arg(providerId);
            qWarning() << "UniversalLLMNode:" << err;
            output.insert(QString::fromLatin1(kOutputResponseId), QVariant(err));
            output.insert(QStringLiteral("__error"), err);
            return output;
        }

        // Retrieve API key using LLMProviderRegistry
        const QString apiKey = LLMProviderRegistry::instance().getCredential(providerId);
        if (apiKey.isEmpty()) {
            const QString err = QStringLiteral("ERROR: API key not found for provider '%1'. Please configure credentials in accounts.json.").arg(providerId);
            qWarning() << "UniversalLLMNode:" << err;
            output.insert(QString::fromLatin1(kOutputResponseId), QVariant(err));
            output.insert(QStringLiteral("__error"), err);
            return output;
        }

        // Delegate to backend strategy
        LLMResult result;
        try {
            result = backend->sendPrompt(apiKey, modelId, temperature, maxTokens, 
                                        systemPrompt, userPrompt);
        } catch (const std::exception& e) {
            const QString err = QStringLiteral("ERROR: Exception during backend call: %1").arg(QString::fromUtf8(e.what()));
            qWarning() << "UniversalLLMNode:" << err;
            output.insert(QString::fromLatin1(kOutputResponseId), QVariant(err));
            output.insert(QStringLiteral("__error"), err);
            return output;
        } catch (...) {
            const QString err = QStringLiteral("ERROR: Unknown exception during backend call.");
            qWarning() << "UniversalLLMNode:" << err;
            output.insert(QString::fromLatin1(kOutputResponseId), QVariant(err));
            output.insert(QStringLiteral("__error"), err);
            return output;
        }

        // Handle error case
        if (result.hasError) {
            output.insert(QString::fromLatin1(kOutputResponseId), result.errorMsg);
            output.insert(QStringLiteral("__error"), result.errorMsg);
            // Still include raw response for debugging
            output.insert(QStringLiteral("_raw_response"), result.rawResponse);
            return output;
        }

        // Map result fields to DataPacket
        // Visible output
        output.insert(QString::fromLatin1(kOutputResponseId), result.content);
        
        // Hidden metadata fields (prefixed with underscore)
        output.insert(QStringLiteral("_usage.input_tokens"), result.usage.inputTokens);
        output.insert(QStringLiteral("_usage.output_tokens"), result.usage.outputTokens);
        output.insert(QStringLiteral("_usage.total_tokens"), result.usage.totalTokens);
        output.insert(QStringLiteral("_raw_response"), result.rawResponse);

        return output;
    });
}

QJsonObject UniversalLLMNode::saveState() const
{
    QJsonObject obj;
    obj[QStringLiteral("provider")] = m_providerId;
    obj[QStringLiteral("model")] = m_modelId;
    obj[QStringLiteral("systemPrompt")] = m_systemPrompt;
    obj[QStringLiteral("userPrompt")] = m_userPrompt;
    obj[QStringLiteral("temperature")] = m_temperature;
    obj[QStringLiteral("maxTokens")] = m_maxTokens;
    return obj;
}

void UniversalLLMNode::loadState(const QJsonObject& data)
{
    m_providerId = data.value(QStringLiteral("provider")).toString();
    m_modelId = data.value(QStringLiteral("model")).toString();
    m_systemPrompt = data.value(QStringLiteral("systemPrompt")).toString();
    m_userPrompt = data.value(QStringLiteral("userPrompt")).toString();
    m_temperature = data.value(QStringLiteral("temperature")).toDouble(0.7);
    m_maxTokens = data.value(QStringLiteral("maxTokens")).toInt(1024);
}

// Slots for widget signal connections
void UniversalLLMNode::onProviderChanged(const QString& providerId)
{
    m_providerId = providerId;
    qDebug() << "UniversalLLMNode: Provider changed to" << providerId;
}

void UniversalLLMNode::onModelChanged(const QString& modelId)
{
    m_modelId = modelId;
    qDebug() << "UniversalLLMNode: Model changed to" << modelId;
}

void UniversalLLMNode::onSystemPromptChanged(const QString& text)
{
    m_systemPrompt = text;
}

void UniversalLLMNode::onUserPromptChanged(const QString& text)
{
    m_userPrompt = text;
}

void UniversalLLMNode::onTemperatureChanged(double value)
{
    m_temperature = value;
}

void UniversalLLMNode::onMaxTokensChanged(int value)
{
    m_maxTokens = value;
}
