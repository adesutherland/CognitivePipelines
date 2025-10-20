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
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>

#include "llm_api_client.h"

LLMConnector::LLMConnector(QObject* parent)
    : QObject(parent) {
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

    // UI -> Connector (live updates)
    QObject::connect(w, &LLMConnectorPropertiesWidget::promptChanged,
                     this, &LLMConnector::setPrompt);

    // Connector -> UI (reflect programmatic changes)
    QObject::connect(this, &LLMConnector::promptChanged,
                     w, &LLMConnectorPropertiesWidget::setPromptText);

    return w;
}

QFuture<DataPacket> LLMConnector::Execute(const DataPacket& inputs) {
    // Resolve API key at runtime from accounts.json or environment
    const QString apiKey = getApiKey();
    if (apiKey.isEmpty()) {
        qDebug() << "LLMConnector: API key not found. Provide accounts.json with 'openai' account or set OPENAI_API_KEY.";
        return QFuture<DataPacket>(); // fail fast: do not attempt to call the API
    }

    // Read incoming prompt if present
    const QString incomingPrompt = inputs.value(QString::fromLatin1(kInputPromptId)).toString();

    // Capture copies for background thread from current properties
    const QString panelPrompt = m_prompt;

    // Build combined prompt: panel prompt first, then separator, then incoming prompt (if provided)
    const QString combinedPrompt = [&]() {
        const QString inTrim = incomingPrompt.trimmed();
        const QString panelTrim = panelPrompt.trimmed();
        if (inTrim.isEmpty() && panelTrim.isEmpty()) return QString{};
        if (panelTrim.isEmpty()) return inTrim; // only incoming
        if (inTrim.isEmpty()) return panelPrompt; // only panel
        return panelPrompt + QStringLiteral("\n\n-----\n\n") + incomingPrompt; // both
    }();

    return QtConcurrent::run([apiKey, combinedPrompt]() -> DataPacket {
        DataPacket output;

        if (combinedPrompt.trimmed().isEmpty()) {
            output.insert(QString::fromLatin1(kOutputResponseId), QVariant(QStringLiteral("ERROR: Prompt is empty.")));
            return output;
        }

        LlmApiClient client;
        const std::string response = client.sendPrompt(apiKey.toStdString(), combinedPrompt.toStdString());
        output.insert(QString::fromLatin1(kOutputResponseId), QString::fromStdString(response));
        return output;
    });
}

QJsonObject LLMConnector::saveState() const {
    QJsonObject obj;
    obj.insert(QStringLiteral("prompt"), m_prompt);
    return obj;
}

void LLMConnector::loadState(const QJsonObject& data) {
    if (data.contains(QStringLiteral("prompt"))) {
        setPrompt(data.value(QStringLiteral("prompt")).toString());
    }
}

QString LLMConnector::getApiKey() const {
    const QString fileName = QStringLiteral("accounts.json");

    const QStringList candidates = {
        QDir::current().filePath(fileName),
        QCoreApplication::applicationDirPath() + QLatin1Char('/') + fileName,
        QDir(QCoreApplication::applicationDirPath() + QLatin1String("/..")).filePath(fileName)
    };

    for (const QString& path : candidates) {
        QFile f(path);
        if (!f.exists()) continue;
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QByteArray data = f.readAll();
        f.close();
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) continue;
        const QJsonArray accounts = doc.object().value(QStringLiteral("accounts")).toArray();
        for (const QJsonValue& v : accounts) {
            const QJsonObject acc = v.toObject();
            const QString name = acc.value(QStringLiteral("name")).toString();
            if (name == QStringLiteral("openai")) {
                const QString key = acc.value(QStringLiteral("api_key")).toString();
                if (!key.isEmpty()) return key;
            }
        }
    }

    const QByteArray envKey = qgetenv("OPENAI_API_KEY");
    if (!envKey.isEmpty()) return QString::fromUtf8(envKey);

    return {};
}
