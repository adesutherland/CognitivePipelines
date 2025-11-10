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
#include "GoogleLLMConnector.h"
#include "GoogleLLMConnectorPropertiesWidget.h"
#include "llm_api_client.h"

#include <QtConcurrent/QtConcurrent>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>
#include <QVariant>
#include <QHash>

GoogleLLMConnector::GoogleLLMConnector(QObject* parent)
    : QObject(parent)
{
    m_apiClient = std::make_unique<LlmApiClient>();
}

GoogleLLMConnector::~GoogleLLMConnector() = default;

NodeDescriptor GoogleLLMConnector::GetDescriptor() const {
    NodeDescriptor desc;
    desc.id = QStringLiteral("google-llm-connector");
    desc.name = QStringLiteral("Google LLM Connector");
    desc.category = QStringLiteral("Connectors");

    // Inputs: system, prompt
    PinDefinition inSystem;
    inSystem.direction = PinDirection::Input;
    inSystem.id = QStringLiteral("system");
    inSystem.name = QStringLiteral("System");
    inSystem.type = QStringLiteral("text");
    desc.inputPins.insert(inSystem.id, inSystem);

    PinDefinition inPrompt;
    inPrompt.direction = PinDirection::Input;
    inPrompt.id = QStringLiteral("prompt");
    inPrompt.name = QStringLiteral("Prompt");
    inPrompt.type = QStringLiteral("text");
    desc.inputPins.insert(inPrompt.id, inPrompt);

    // Output: response
    PinDefinition outResponse;
    outResponse.direction = PinDirection::Output;
    outResponse.id = QStringLiteral("response");
    outResponse.name = QStringLiteral("Response");
    outResponse.type = QStringLiteral("text");
    desc.outputPins.insert(outResponse.id, outResponse);

    return desc;
}

QWidget* GoogleLLMConnector::createConfigurationWidget(QWidget* parent) {
    auto* w = new GoogleLLMConnectorPropertiesWidget(parent);

    // Initialize UI from our current state
    w->setModelName(m_modelName);
    w->setTemperature(m_temperature);
    w->setMaxTokens(m_maxTokens);

    // Wire UI signals to our slots
    QObject::connect(w, &GoogleLLMConnectorPropertiesWidget::modelNameChanged,
                     this, &GoogleLLMConnector::onModelNameChanged);
    QObject::connect(w, &GoogleLLMConnectorPropertiesWidget::temperatureChanged,
                     this, &GoogleLLMConnector::onTemperatureChanged);
    QObject::connect(w, &GoogleLLMConnectorPropertiesWidget::maxTokensChanged,
                     this, &GoogleLLMConnector::onMaxTokensChanged);

    return w;
}

QVariant GoogleLLMConnector::GetOutputData(const QString& pinId) const {
    return m_lastOutput.value(pinId);
}

QJsonObject GoogleLLMConnector::saveState() const {
    QJsonObject obj;
    obj.insert(QStringLiteral("model"), m_modelName);
    obj.insert(QStringLiteral("temperature"), m_temperature);
    obj.insert(QStringLiteral("max_tokens"), m_maxTokens);
    return obj;
}

void GoogleLLMConnector::loadState(const QJsonObject& state) {
    if (state.contains(QStringLiteral("model"))) {
        m_modelName = state.value(QStringLiteral("model")).toString();
    }
    if (state.contains(QStringLiteral("temperature"))) {
        m_temperature = state.value(QStringLiteral("temperature")).toDouble(0.7);
    }
    if (state.contains(QStringLiteral("max_tokens"))) {
        m_maxTokens = state.value(QStringLiteral("max_tokens")).toInt(1024);
    }
}

QFuture<DataPacket> GoogleLLMConnector::Execute(const DataPacket& inputs) {
    // Gather inputs
    const QString systemPrompt = inputs.value(QStringLiteral("system")).toString();
    const QString userPrompt = inputs.value(QStringLiteral("prompt")).toString();

    const QString modelName = m_modelName;
    const double temperature = m_temperature;
    const int maxTokens = m_maxTokens;
    LlmApiClient* client = m_apiClient.get();

    return QtConcurrent::run([systemPrompt, userPrompt, modelName, temperature, maxTokens, client]() -> DataPacket {
        DataPacket output;
        // Clear output pin at start
        output.insert(QStringLiteral("response"), QVariant());

        if (!client) {
            const QString err = QStringLiteral("ERROR: API client unavailable");
            output.insert(QStringLiteral("response"), err);
            output.insert(QStringLiteral("__error"), err);
            return output;
        }

        // Resolve Google API key via new accessor
        const QString apiKey = client->getApiKey(QStringLiteral("google"));
        if (apiKey.isEmpty()) {
            const QString err = QStringLiteral("ERROR: Google API key not found. Set GOOGLE_API_KEY or add accounts.json.");
            output.insert(QStringLiteral("response"), err);
            output.insert(QStringLiteral("__error"), err);
            return output;
        }

        // Perform request using Google provider
        QString raw = client->sendPrompt(LlmApiClient::ApiProvider::Google,
                                         apiKey,
                                         modelName,
                                         temperature,
                                         maxTokens,
                                         systemPrompt,
                                         userPrompt);
        if (raw.isEmpty()) raw = QStringLiteral("ERROR: Empty response from LLM API");

        // Parse JSON per Google schema
        QJsonParseError jerr;
        const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8(), &jerr);
        if (jerr.error != QJsonParseError::NoError || !doc.isObject()) {
            // Treat as plain text error or content
            output.insert(QStringLiteral("response"), raw.trimmed());
            const QString lc = raw.trimmed().toLower();
            if (lc.startsWith(QStringLiteral("http ")) || lc.startsWith(QStringLiteral("network error")) || lc.startsWith(QStringLiteral("error:"))) {
                output.insert(QStringLiteral("__error"), raw.trimmed());
            }
            return output;
        }

        const QJsonObject root = doc.object();
        const QJsonValue errVal = root.value(QStringLiteral("error"));
        if (errVal.isObject()) {
            const QJsonObject errObj = errVal.toObject();
            QString errMsg = errObj.value(QStringLiteral("message")).toString();
            if (errMsg.isEmpty()) errMsg = QStringLiteral("Unknown API error");
            output.insert(QStringLiteral("response"), errMsg);
            output.insert(QStringLiteral("__error"), errMsg);
            return output;
        }

        // Happy path: candidates[0].content.parts[0].text
        const QJsonArray candidates = root.value(QStringLiteral("candidates")).toArray();
        if (!candidates.isEmpty()) {
            const QJsonObject firstCand = candidates.at(0).toObject();
            const QJsonObject content = firstCand.value(QStringLiteral("content")).toObject();
            const QJsonArray parts = content.value(QStringLiteral("parts")).toArray();
            if (!parts.isEmpty()) {
                const QJsonObject firstPart = parts.at(0).toObject();
                const QString text = firstPart.value(QStringLiteral("text")).toString();
                if (!text.isEmpty()) {
                    output.insert(QStringLiteral("response"), text);
                    return output;
                }
            }
        }

        // If structure unexpected
        const QString err = QStringLiteral("Invalid Google JSON response structure");
        output.insert(QStringLiteral("response"), err);
        output.insert(QStringLiteral("__error"), err);
        return output;
    });
}

void GoogleLLMConnector::onPromptFinished(const QString& response) {
    // Parse a raw Google response and store the last output for inspection/testing.
    m_lastOutput.clear();

    QJsonParseError jerr;
    const QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8(), &jerr);
    if (jerr.error != QJsonParseError::NoError || !doc.isObject()) {
        // Treat as plain text
        const QString plain = response.trimmed();
        m_lastOutput.insert(QStringLiteral("response"), plain);
        // Keep a log for debugging
        qWarning() << "GoogleLLMConnector parse error:" << jerr.errorString();
        return;
    }

    const QJsonObject root = doc.object();
    const QJsonValue errVal = root.value(QStringLiteral("error"));
    if (errVal.isObject()) {
        const QJsonObject errObj = errVal.toObject();
        QString errMsg = errObj.value(QStringLiteral("message")).toString();
        if (errMsg.isEmpty()) errMsg = QStringLiteral("Unknown API error");
        m_lastOutput.insert(QStringLiteral("response"), errMsg);
        m_lastOutput.insert(QStringLiteral("__error"), errMsg);
        qWarning() << "Google API error:" << errMsg;
        return;
    }

    // Success path: candidates[0].content.parts[0].text
    const QJsonArray candidatesArr = root.value(QStringLiteral("candidates")).toArray();
    if (!candidatesArr.isEmpty()) {
        const QJsonObject cand0 = candidatesArr.at(0).toObject();
        const QJsonObject content = cand0.value(QStringLiteral("content")).toObject();
        const QJsonArray partsArr = content.value(QStringLiteral("parts")).toArray();
        if (!partsArr.isEmpty()) {
            const QString text = partsArr.at(0).toObject().value(QStringLiteral("text")).toString();
            if (!text.isEmpty()) {
                m_lastOutput.insert(QStringLiteral("response"), text);
                return;
            }
        }
    }

    const QString err = QStringLiteral("Invalid Google JSON response structure");
    m_lastOutput.insert(QStringLiteral("response"), err);
    m_lastOutput.insert(QStringLiteral("__error"), err);
}

void GoogleLLMConnector::onModelNameChanged(const QString& modelName) {
    m_modelName = modelName;
}

void GoogleLLMConnector::onTemperatureChanged(double temp) {
    m_temperature = temp;
}

void GoogleLLMConnector::onMaxTokensChanged(int tokens) {
    m_maxTokens = tokens;
}
