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
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonParseError>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>
#include <QStandardPaths>

#include "llm_api_client.h"

QString LLMConnector::defaultAccountsFilePath() {
    // Use a stable per-user base directory independent of the executable name.
    // Canonical result examples:
    //  - macOS: ~/Library/Application Support/CognitivePipelines/accounts.json
    //  - Linux: ~/.config/CognitivePipelines/accounts.json
    //  - Windows: %APPDATA%/CognitivePipelines/accounts.json
#if defined(Q_OS_MAC)
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation); // Application Support
#else
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
#endif
    if (baseDir.isEmpty()) return QString();
    return QDir(baseDir).filePath(QStringLiteral("CognitivePipelines/accounts.json"));
}

LLMConnector::LLMConnector(QObject* parent)
    : QObject(parent) {
    m_apiClient = std::make_unique<LlmApiClient>();
}

LLMConnector::~LLMConnector() = default;

void LLMConnector::onTemperatureChanged(double temp) {
    m_temperature = temp;
}

void LLMConnector::onMaxTokensChanged(int tokens) {
    m_maxTokens = tokens;
}

void LLMConnector::onModelNameChanged(const QString &modelName) {
    m_modelName = modelName;
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

    // New two-input schema: system and prompt
    PinDefinition inSystem;
    inSystem.direction = PinDirection::Input;
    inSystem.id = QString::fromLatin1(kInputSystemId);
    inSystem.name = QStringLiteral("System");
    inSystem.type = QStringLiteral("text");
    desc.inputPins.insert(inSystem.id, inSystem);

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
    w->setTemperature(m_temperature);
    w->setMaxTokens(m_maxTokens);
    w->setModelName(m_modelName);

    // UI -> Connector (live updates)
    QObject::connect(w, &LLMConnectorPropertiesWidget::promptChanged,
                     this, &LLMConnector::setPrompt);
    QObject::connect(w, &LLMConnectorPropertiesWidget::temperatureChanged,
                     this, &LLMConnector::onTemperatureChanged);
    QObject::connect(w, &LLMConnectorPropertiesWidget::maxTokensChanged,
                     this, &LLMConnector::onMaxTokensChanged);
    QObject::connect(w, &LLMConnectorPropertiesWidget::modelNameChanged,
                     this, &LLMConnector::onModelNameChanged);

    // Connector -> UI (reflect programmatic changes)
    QObject::connect(this, &LLMConnector::promptChanged,
                     w, &LLMConnectorPropertiesWidget::setPromptText);

    return w;
}

QFuture<DataPacket> LLMConnector::Execute(const DataPacket& inputs) {
    // Retrieve both input pins
    const QString systemPrompt = inputs.value(QString::fromLatin1(kInputSystemId)).toString();
    const QString userPrompt = inputs.value(QString::fromLatin1(kInputPromptId)).toString();

    // Copy state to use in background thread
    const QString panelPrompt = m_prompt; // backward-compat: if userPrompt empty, allow panel text as user or system
    const double temperature = m_temperature;
    const int maxTokens = m_maxTokens;
    const QString modelName = m_modelName;
    LlmApiClient* apiClient = m_apiClient.get();

    return QtConcurrent::run([systemPrompt, userPrompt, panelPrompt, temperature, maxTokens, modelName, apiClient]() -> DataPacket {
        DataPacket output;
        // Clear the output pin at the very start to avoid stale data propagation
        output.insert(QString::fromLatin1(kOutputResponseId), QVariant());

        const QString sys = systemPrompt.trimmed();
        const QString usr = userPrompt.trimmed().isEmpty() ? panelPrompt.trimmed() : userPrompt.trimmed();

        if (sys.isEmpty() && usr.isEmpty()) {
            const QString err = QStringLiteral("ERROR: Prompt is empty.");
            output.insert(QString::fromLatin1(kOutputResponseId), QVariant(err));
            output.insert(QStringLiteral("__error"), err);
            return output;
        }

        // Resolve API key using the new provider-name lookup first, with legacy fallback
        LlmApiClient legacyClient;
        QString apiKey = legacyClient.getApiKey(QStringLiteral("openai"));
        if (apiKey.isEmpty()) apiKey = LLMConnector::getApiKey();
        if (apiKey.isEmpty()) {
            const QString err = QStringLiteral("ERROR: API key not found. Set OPENAI_API_KEY or place accounts.json in the standard app config directory (see README).");
            output.insert(QString::fromLatin1(kOutputResponseId), QVariant(err));
            output.insert(QStringLiteral("__error"), err);
            return output;
        }

        // Invoke new provider-aware API and capture raw response/error body
        QString response;
        if (auto clientPtr = apiClient) {
            response = clientPtr->sendPrompt(LlmApiClient::ApiProvider::OpenAI,
                                  apiKey,
                                  modelName,
                                  temperature,
                                  maxTokens,
                                  sys,
                                  usr);
        }
        if (response.isEmpty()) {
            // As a safety, if no response was received, return a generic error
            response = QStringLiteral("ERROR: Empty response from LLM API");
        }

        // Robust JSON parsing path: if response is JSON, extract choices[0].message.content or API error
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            const QJsonObject root = doc.object();

            // Detect API error object and surface its message
            const QJsonValue errVal = root.value(QStringLiteral("error"));
            if (errVal.isObject()) {
                const QJsonObject errObj = errVal.toObject();
                QString errMsg = errObj.value(QStringLiteral("message")).toString();
                if (errMsg.isEmpty()) errMsg = QStringLiteral("Unknown API error");
                qWarning() << "LLMConnector API error:" << errMsg;
                output.insert(QString::fromLatin1(kOutputResponseId), errMsg);
                output.insert(QStringLiteral("__error"), errMsg);
                return output;
            }

            // Happy path: choices[0].message.content
            const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
            if (!choices.isEmpty()) {
                const QJsonObject first = choices.at(0).toObject();
                const QJsonObject message = first.value(QStringLiteral("message")).toObject();
                const QString content = message.value(QStringLiteral("content")).toString();
                if (!content.isEmpty()) {
                    output.insert(QString::fromLatin1(kOutputResponseId), content);
                    return output;
                }
            }
            {
                qWarning() << "LLMConnector: Invalid response structure. Raw JSON:" << response;
                const QString err = QStringLiteral("Invalid JSON response structure");
                output.insert(QString::fromLatin1(kOutputResponseId), err);
                output.insert(QStringLiteral("__error"), err);
                return output;
            }
        }

        if (parseError.error != QJsonParseError::NoError) {
            // Not JSON — treat as plain text; fall through to fallback below
            qWarning() << "LLMConnector: non-JSON response; using plain text.";
            // Do not set output here; fallback below will forward raw response text
        }

        // Fallback: treat response as plain text (already content or error message)
        const QString trimmed = response.trimmed();
        output.insert(QString::fromLatin1(kOutputResponseId), trimmed);
        const QString lc = trimmed.toLower();
        const bool looksError = trimmed.startsWith(QStringLiteral("HTTP "))
                                || trimmed.startsWith(QStringLiteral("Network error"))
                                || trimmed.startsWith(QStringLiteral("ERROR:"))
                                || lc.contains(QStringLiteral("model_not_found"))
                                || lc.contains(QStringLiteral("does not exist"));
        if (looksError) {
            output.insert(QStringLiteral("__error"), trimmed);
        }
        return output;
    });
}

QJsonObject LLMConnector::saveState() const {
    QJsonObject obj;
    obj.insert(QStringLiteral("prompt"), m_prompt);
    obj.insert(QStringLiteral("temperature"), m_temperature);
    obj.insert(QStringLiteral("max_tokens"), m_maxTokens);
    obj.insert(QStringLiteral("model"), m_modelName);
    return obj;
}

void LLMConnector::loadState(const QJsonObject& data) {
    if (data.contains(QStringLiteral("prompt"))) {
        setPrompt(data.value(QStringLiteral("prompt")).toString());
    }
    if (data.contains(QStringLiteral("temperature"))) {
        m_temperature = data.value(QStringLiteral("temperature")).toDouble(0.7);
    }
    if (data.contains(QStringLiteral("max_tokens"))) {
        m_maxTokens = data.value(QStringLiteral("max_tokens")).toInt(1024);
    }
    if (data.contains(QStringLiteral("model"))) {
        m_modelName = data.value(QStringLiteral("model")).toString(m_modelName);
    }
}

QString LLMConnector::getApiKey() {
    // 1) Environment variable takes precedence
    const QByteArray envKey = qgetenv("OPENAI_API_KEY");
    if (!envKey.isEmpty()) return QString::fromUtf8(envKey);

    // 2) Single canonical location: defaultAccountsFilePath()
    const QString path = LLMConnector::defaultAccountsFilePath();
    if (path.isEmpty()) {
        qWarning() << "API key file base path unavailable (QStandardPaths returned empty).";
        return {};
    }

    QFile f(path);
    if (!f.exists()) {
        qWarning() << "API key file not found at:" << path;
        return {};
    }
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open API key file at" << path << ":" << f.errorString();
        return {};
    }
    const QByteArray data = f.readAll();
    f.close();

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        qWarning() << "Invalid JSON in API key file at:" << path;
        return {};
    }
    const QJsonObject root = doc.object();

    // Support both shapes:
    // a) { "openai_api_key": "..." }
    const QString directKey = root.value(QStringLiteral("openai_api_key")).toString();
    if (!directKey.isEmpty()) return directKey;

    // b) { "accounts": [ { "name": "default_openai", "api_key": "..." }, ... ] }
    const QJsonArray accounts = root.value(QStringLiteral("accounts")).toArray();
    for (const QJsonValue& v : accounts) {
        const QJsonObject acc = v.toObject();
        const QString name = acc.value(QStringLiteral("name")).toString();
        if (name == QStringLiteral("openai") || name == QStringLiteral("default_openai") || name == QStringLiteral("OpenAI")) {
            const QString key = acc.value(QStringLiteral("api_key")).toString();
            if (!key.isEmpty()) return key;
        }
    }

    qWarning() << "API key not found in file at:" << path << "(checked keys 'openai_api_key' and accounts[].api_key)";
    return {};
}
