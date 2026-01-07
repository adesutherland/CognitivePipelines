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
#include "ModelCapsRegistry.h"
#include "Logger.h"
#include <QtConcurrent>
#include <QJsonObject>
#include "logging_categories.h"

UniversalLLMNode::UniversalLLMNode(QObject* parent)
    : QObject(parent)
{
    m_descriptor.id = QStringLiteral("universal-llm");
    m_descriptor.name = QStringLiteral("Universal AI");
    m_descriptor.category = QStringLiteral("AI Services");

    // Input pins (default: prompt + image; system optional for advanced usage)
    PinDefinition systemPin;
    systemPin.direction = PinDirection::Input;
    systemPin.id = QString::fromLatin1(kInputSystemId);
    systemPin.name = QStringLiteral("System");
    systemPin.type = QStringLiteral("text");
    m_descriptor.inputPins.insert(systemPin.id, systemPin);

    PinDefinition promptPin;
    promptPin.direction = PinDirection::Input;
    promptPin.id = QString::fromLatin1(kInputPromptId);
    promptPin.name = QStringLiteral("Prompt");
    promptPin.type = QStringLiteral("text");
    m_descriptor.inputPins.insert(promptPin.id, promptPin);

    PinDefinition imagePin;
    imagePin.direction = PinDirection::Input;
    imagePin.id = QString::fromLatin1(kInputImageId);
    imagePin.name = QStringLiteral("Image");
    imagePin.type = QStringLiteral("image");
    m_descriptor.inputPins.insert(imagePin.id, imagePin);

    PinDefinition responsePin;
    responsePin.direction = PinDirection::Output;
    responsePin.id = QString::fromLatin1(kOutputResponseId);
    responsePin.name = QStringLiteral("Response");
    responsePin.type = QStringLiteral("text");
    m_descriptor.outputPins.insert(responsePin.id, responsePin);

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

NodeDescriptor UniversalLLMNode::getDescriptor() const
{
    return m_descriptor;
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

    // Prompt Builder Protocol: drive capability-based pin changes from the Properties UI.
    // When the model changes, resolve capabilities and request an update on the node.
    connect(widget, &UniversalLLMPropertiesWidget::modelChanged,
            this, [this](const QString& modelId) {
                const auto caps = ModelCapsRegistry::instance().resolve(modelId, m_providerId);
                if (caps.has_value()) {
                    this->updateCapabilities(*caps);
                }
            });
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

TokenList UniversalLLMNode::execute(const TokenList& incomingTokens)
{
    // Merge incoming tokens into a single DataPacket to preserve the prior
    // Execute(DataPacket) contract.
    DataPacket inputs;
    for (const auto& token : incomingTokens) {
        for (auto it = token.data.cbegin(); it != token.data.cend(); ++it) {
            inputs.insert(it.key(), it.value());
        }
    }

    // Retrieve input pins (may override defaults from properties)
    const QString systemInput = inputs.value(QString::fromLatin1(kInputSystemId)).toString();
    const QString promptInput = inputs.value(QString::fromLatin1(kInputPromptId)).toString();
    const QString imageInput = inputs.value(QString::fromLatin1(kInputImageId)).toString();

    // Copy state for use during this execution
    const QString providerId = m_providerId;
    const QString modelId = m_modelId;
    const QString systemDefault = m_systemPrompt;
    const QString userDefault = m_userPrompt;
    const double temperature = m_temperature;
    const int maxTokens = m_maxTokens;

    // Instrumentation: log at the very start of execute() (debug‑gated)
    CP_CLOG(cp_lifecycle).noquote() << "[ModelLifecycle] Node: execute() start"
                       << " providerId=" << providerId
                       << " modelId=" << modelId;

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

        ExecutionToken token; token.data = output; return TokenList{token};
    }

    // Validate model id
    if (modelId.trimmed().isEmpty()) {
        const QString err = QStringLiteral("ERROR: Model id is empty.");
        CP_WARN << "UniversalLLMNode:" << err;
        output.insert(QString::fromLatin1(kOutputResponseId), QVariant(err));
        output.insert(QStringLiteral("__error"), err);

        ExecutionToken token; token.data = output; return TokenList{token};
    }

    // Resolve backend using LLMProviderRegistry
    ILLMBackend* backend = LLMProviderRegistry::instance().getBackend(providerId);
    if (!backend) {
        const QString err = QStringLiteral("ERROR: Backend '%1' not found. Please check provider configuration.").arg(providerId);
        CP_WARN << "UniversalLLMNode:" << err;
        output.insert(QString::fromLatin1(kOutputResponseId), QVariant(err));
        output.insert(QStringLiteral("__error"), err);

        ExecutionToken token; token.data = output; return TokenList{token};
    }

    // Retrieve API key using LLMProviderRegistry
    const QString apiKey = LLMProviderRegistry::instance().getCredential(providerId);
    if (apiKey.isEmpty()) {
        const QString err = QStringLiteral("ERROR: API key not found for provider '%1'. Please configure credentials in accounts.json.").arg(providerId);
        CP_WARN << "UniversalLLMNode:" << err;
        output.insert(QString::fromLatin1(kOutputResponseId), QVariant(err));
        output.insert(QStringLiteral("__error"), err);

        ExecutionToken token; token.data = output; return TokenList{token};
    }

    // Validate model with Registry-first authority to avoid stale backend lists
    // Instrumentation: introspect potential double-quoting on the model id
    const auto firstChar = modelId.isEmpty() ? QStringLiteral("∅") : modelId.left(1);
    const auto lastChar = modelId.isEmpty() ? QStringLiteral("∅") : modelId.right(1);
    CP_CLOG(cp_lifecycle).noquote() << "[ModelLifecycle] Validation: selected modelId='" << modelId
                       << "' len=" << modelId.size()
                       << " first='" << firstChar << "' last='" << lastChar << "'";
    QString validatedModelId = modelId;
    const auto capsFromRegistry = ModelCapsRegistry::instance().resolve(modelId, providerId);
    if (capsFromRegistry.has_value()) {
        // Registry recognizes this model for the provider — trust the selection
        validatedModelId = modelId;
    } else {
        // Do NOT auto-recover the model id. Preserve the user's selection unchanged.
        // Emit a warning for visibility but pass through to backend.
        const QStringList availableModels = backend->availableModels();
        if (!availableModels.contains(modelId)) {
            CP_WARN << "UniversalLLMNode: Model not recognized by Registry and not found in backend list. Passing through selection unchanged: '"
                       << modelId << "' for provider '" << providerId << "'.";
        }
        validatedModelId = modelId;
    }

    // Delegate to backend strategy
    // Instrumentation: log immediately before backend call, showing selected vs validated IDs
    // Instrumentation: also introspect validated id
    const auto vFirstChar = validatedModelId.isEmpty() ? QStringLiteral("∅") : validatedModelId.left(1);
    const auto vLastChar = validatedModelId.isEmpty() ? QStringLiteral("∅") : validatedModelId.right(1);
    CP_CLOG(cp_lifecycle).noquote() << "[ModelLifecycle] Node: pre-backend call"
                       << " providerId=" << providerId
                       << " selectedModelId=" << modelId
                       << " validatedModelId=" << validatedModelId
                       << " | selected(len=" << modelId.size() << ", first='" << firstChar << "', last='" << lastChar
                       << "') validated(len=" << validatedModelId.size() << ", first='" << vFirstChar
                       << "', last='" << vLastChar << "')";
    LLMResult result;
    try {
        result = backend->sendPrompt(apiKey, validatedModelId, temperature, maxTokens, 
                                    systemPrompt, userPrompt, imageInput);
    } catch (const std::exception& e) {
        const QString err = QStringLiteral("ERROR: Exception during backend call: %1").arg(QString::fromUtf8(e.what()));
        CP_WARN << "UniversalLLMNode:" << err;
        output.insert(QString::fromLatin1(kOutputResponseId), QVariant(err));
        output.insert(QStringLiteral("__error"), err);

        ExecutionToken token; token.data = output; return TokenList{token};
    } catch (...) {
        const QString err = QStringLiteral("ERROR: Unknown exception during backend call.");
        CP_WARN << "UniversalLLMNode:" << err;
        output.insert(QString::fromLatin1(kOutputResponseId), QVariant(err));
        output.insert(QStringLiteral("__error"), err);

        ExecutionToken token; token.data = output; return TokenList{token};
    }

    // Handle error case
    if (result.hasError) {
        output.insert(QString::fromLatin1(kOutputResponseId), result.content);
        output.insert(QStringLiteral("__error"), result.errorMsg);
        // Still include raw response for debugging
        output.insert(QStringLiteral("_raw_response"), result.rawResponse);

        ExecutionToken token; token.data = output; return TokenList{token};
    }

    // Map result fields to DataPacket
    // Visible output
    output.insert(QString::fromLatin1(kOutputResponseId), result.content);
    
    // Hidden metadata fields (prefixed with underscore)
    output.insert(QStringLiteral("_usage.input_tokens"), result.usage.inputTokens);
    output.insert(QStringLiteral("_usage.output_tokens"), result.usage.outputTokens);
    output.insert(QStringLiteral("_usage.total_tokens"), result.usage.totalTokens);
    output.insert(QStringLiteral("_raw_response"), result.rawResponse);

    ExecutionToken token;
    token.data = output;
    return TokenList{token};
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
    const QString loadedModel = data.value(QStringLiteral("model")).toString();
    // Instrumentation: introspect loaded model id to catch accidental quoting from persistence
    {
        const auto firstChar = loadedModel.isEmpty() ? QStringLiteral("∅") : loadedModel.left(1);
        const auto lastChar = loadedModel.isEmpty() ? QStringLiteral("∅") : loadedModel.right(1);
        CP_CLOG(cp_lifecycle).noquote() << "[ModelLifecycle] LoadState -> provider='" << m_providerId
                           << "' model='" << loadedModel << "' len=" << loadedModel.size()
                           << " first='" << firstChar << "' last='" << lastChar << "'";
    }
    m_modelId = loadedModel;
    m_systemPrompt = data.value(QStringLiteral("systemPrompt")).toString();
    m_userPrompt = data.value(QStringLiteral("userPrompt")).toString();
    m_temperature = data.value(QStringLiteral("temperature")).toDouble(0.7);
    m_maxTokens = data.value(QStringLiteral("maxTokens")).toInt(1024);
}

void UniversalLLMNode::updateCapabilities(const ModelCapsTypes::ModelCaps& caps)
{
    m_caps = caps;

    const bool hasVision = caps.hasCapability(ModelCapsTypes::Capability::Vision);
    const bool hasImagePin = m_descriptor.inputPins.contains(QString::fromLatin1(kInputImageId));

    bool descriptorChanged = false;

    if (hasVision && !hasImagePin) {
        PinDefinition imagePin;
        imagePin.direction = PinDirection::Input;
        imagePin.id = QString::fromLatin1(kInputImageId);
        imagePin.name = QStringLiteral("Image");
        imagePin.type = QStringLiteral("image");
        m_descriptor.inputPins.insert(imagePin.id, imagePin);
        descriptorChanged = true;
    } else if (!hasVision && hasImagePin) {
        m_descriptor.inputPins.remove(QString::fromLatin1(kInputImageId));
        descriptorChanged = true;
    }

    if (descriptorChanged) {
        emit inputPinsChanged();
    }

    if (caps.constraints.temperature.has_value()) {
        const auto& temp = *caps.constraints.temperature;

        if (temp.defaultValue.has_value()) {
            m_temperature = *temp.defaultValue;
        }

        if (temp.min.has_value() && temp.max.has_value() && *temp.min == *temp.max) {
            m_temperature = *temp.min;
        } else {
            if (temp.min.has_value() && m_temperature < *temp.min) {
                m_temperature = *temp.min;
            }
            if (temp.max.has_value() && m_temperature > *temp.max) {
                m_temperature = *temp.max;
            }
        }
    }
}

// Slots for widget signal connections
void UniversalLLMNode::onProviderChanged(const QString& providerId)
{
    // Instrumentation: log when node receives provider updates from the widget (debug‑gated)
    CP_CLOG(cp_lifecycle).noquote() << "[ModelLifecycle] Node: onProviderChanged -> providerId=" << providerId;
    m_providerId = providerId;
}

void UniversalLLMNode::onModelChanged(const QString& modelId)
{
    // Instrumentation: log when node receives model updates from the widget (debug‑gated)
    const auto firstChar = modelId.isEmpty() ? QStringLiteral("∅") : modelId.left(1);
    const auto lastChar = modelId.isEmpty() ? QStringLiteral("∅") : modelId.right(1);
    CP_CLOG(cp_lifecycle).noquote() << "[ModelLifecycle] Node: onModelChanged -> modelId='" << modelId
                       << "' len=" << modelId.size()
                       << " first='" << firstChar << "' last='" << lastChar << "'";
    m_modelId = modelId;
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
