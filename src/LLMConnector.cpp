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

#include "LLMConnector.h"
#include "LLMConnectorPropertiesWidget.h"

#include <QtConcurrent/QtConcurrent>
#include <QPointer>
#include <QProcessEnvironment>

#include "llm_api_client.h"

LLMConnector::LLMConnector(QObject* parent)
    : QObject(parent) {
}

void LLMConnector::setApiKey(const QString& key) {
    if (m_apiKey == key) return;
    m_apiKey = key;
    emit apiKeyChanged(m_apiKey);
}

void LLMConnector::setPrompt(const QString& prompt) {
    if (m_prompt == prompt) return;
    m_prompt = prompt;
    emit promptChanged(m_prompt);
}

NodeDescriptor LLMConnector::GetDescriptor() const {
    NodeDescriptor desc;
    desc.id = QStringLiteral("llm-connector");
    desc.name = QStringLiteral("LLM Connector");
    desc.category = QStringLiteral("Generative AI");

    PinDefinition inPrompt;
    inPrompt.direction = PinDirection::Input;
    inPrompt.id = QString::fromLatin1(kInputPromptId);
    inPrompt.name = QStringLiteral("Prompt");
    inPrompt.type = QStringLiteral("text");
    desc.inputPins.insert(inPrompt.id, inPrompt);

    PinDefinition outResponse;
    outResponse.direction = PinDirection::Output;
    outResponse.id = QString::fromLatin1(kOutputResponseId);
    outResponse.name = QStringLiteral("Response");
    outResponse.type = QStringLiteral("text");
    desc.outputPins.insert(outResponse.id, outResponse);

    return desc;
}

QWidget* LLMConnector::createConfigurationWidget(QWidget* parent) {
    auto* w = new LLMConnectorPropertiesWidget(parent);
    // Initialize from current state
    w->setPromptText(m_prompt);
    w->setApiKeyText(m_apiKey);

    // UI -> Connector (live updates)
    QObject::connect(w, &LLMConnectorPropertiesWidget::promptChanged,
                     this, &LLMConnector::setPrompt);
    QObject::connect(w, &LLMConnectorPropertiesWidget::apiKeyChanged,
                     this, &LLMConnector::setApiKey);

    // Connector -> UI (reflect programmatic changes)
    QObject::connect(this, &LLMConnector::promptChanged,
                     w, &LLMConnectorPropertiesWidget::setPromptText);
    QObject::connect(this, &LLMConnector::apiKeyChanged,
                     w, &LLMConnectorPropertiesWidget::setApiKeyText);

    return w;
}

QFuture<DataPacket> LLMConnector::Execute(const DataPacket& inputs) {
    Q_UNUSED(inputs);

    // Capture copies for background thread from current properties
    const QString apiKey = m_apiKey;
    const QString prompt = m_prompt;

    return QtConcurrent::run([apiKey, prompt]() -> DataPacket {
        DataPacket output;

        if (apiKey.isEmpty()) {
            output.insert(QString::fromLatin1(kOutputResponseId), QVariant(QStringLiteral("ERROR: API key not set.")));
            return output;
        }
        if (prompt.trimmed().isEmpty()) {
            output.insert(QString::fromLatin1(kOutputResponseId), QVariant(QStringLiteral("ERROR: Prompt is empty.")));
            return output;
        }

        LlmApiClient client;
        const std::string response = client.sendPrompt(apiKey.toStdString(), prompt.toStdString());
        output.insert(QString::fromLatin1(kOutputResponseId), QString::fromStdString(response));
        return output;
    });
}
