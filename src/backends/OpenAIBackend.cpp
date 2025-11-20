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
#include "OpenAIBackend.h"

#include <cpr/cpr.h>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

QString OpenAIBackend::id() const {
    return QStringLiteral("openai");
}

QString OpenAIBackend::name() const {
    return QStringLiteral("OpenAI");
}

QStringList OpenAIBackend::availableModels() const {
    return {
        QStringLiteral("gpt-4o"),
        QStringLiteral("gpt-4o-mini"),
        QStringLiteral("o1-preview"),
        QStringLiteral("o1-mini"),
        QStringLiteral("gpt-4-turbo"),
        QStringLiteral("gpt-3.5-turbo")
    };
}

LLMResult OpenAIBackend::sendPrompt(
    const QString& apiKey,
    const QString& modelName,
    double temperature,
    int maxTokens,
    const QString& systemPrompt,
    const QString& userPrompt
) {
    LLMResult result;
    
    const std::string url = "https://api.openai.com/v1/chat/completions";

    // Build messages array [{role: system, content: ...}, {role: user, content: ...}]
    QJsonObject sysMsg;
    sysMsg.insert(QStringLiteral("role"), QStringLiteral("system"));
    sysMsg.insert(QStringLiteral("content"), systemPrompt);

    QJsonObject userMsg;
    userMsg.insert(QStringLiteral("role"), QStringLiteral("user"));
    userMsg.insert(QStringLiteral("content"), userPrompt);

    QJsonArray messages;
    messages.append(sysMsg);
    messages.append(userMsg);

    QJsonObject root;
    root.insert(QStringLiteral("model"), modelName);
    root.insert(QStringLiteral("temperature"), temperature);
    root.insert(QStringLiteral("max_completion_tokens"), maxTokens);
    root.insert(QStringLiteral("messages"), messages);

    const QByteArray jsonBytes = QJsonDocument(root).toJson(QJsonDocument::Compact);

    cpr::Header headers{
        {"Authorization", std::string("Bearer ") + apiKey.toStdString()},
        {"Content-Type", "application/json"}
    };

    // Perform POST synchronously
    auto response = cpr::Post(cpr::Url{url}, headers, cpr::Body{jsonBytes.constData()}, cpr::Timeout{60000});
    
    if (response.error) {
        result.hasError = true;
        result.errorMsg = QStringLiteral("Network error: %1").arg(QString::fromStdString(response.error.message));
        return result;
    }
    
    // Store raw response
    result.rawResponse = QString::fromStdString(response.text);
    
    if (response.status_code != 200) {
        result.hasError = true;
        // Try to parse error from JSON
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(response.text.c_str(), &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains(QStringLiteral("error")) && obj[QStringLiteral("error")].isObject()) {
                QJsonObject errorObj = obj[QStringLiteral("error")].toObject();
                result.errorMsg = errorObj[QStringLiteral("message")].toString(
                    QStringLiteral("HTTP %1").arg(response.status_code));
            } else {
                result.errorMsg = QStringLiteral("HTTP %1").arg(response.status_code);
            }
        } else {
            result.errorMsg = QStringLiteral("HTTP %1").arg(response.status_code);
        }
        return result;
    }
    
    // Parse successful response
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(response.text.c_str(), &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        result.hasError = true;
        result.errorMsg = QStringLiteral("JSON parse error: %1").arg(parseError.errorString());
        return result;
    }
    
    if (!doc.isObject()) {
        result.hasError = true;
        result.errorMsg = QStringLiteral("Invalid JSON: root is not an object");
        return result;
    }
    
    QJsonObject rootObj = doc.object();
    
    // Check for error object in response
    if (rootObj.contains(QStringLiteral("error"))) {
        result.hasError = true;
        if (rootObj[QStringLiteral("error")].isObject()) {
            QJsonObject errorObj = rootObj[QStringLiteral("error")].toObject();
            result.errorMsg = errorObj[QStringLiteral("message")].toString(QStringLiteral("Unknown error"));
        } else {
            result.errorMsg = QStringLiteral("Unknown error");
        }
        return result;
    }
    
    // Extract content from choices[0].message.content
    if (rootObj.contains(QStringLiteral("choices")) && rootObj[QStringLiteral("choices")].isArray()) {
        QJsonArray choices = rootObj[QStringLiteral("choices")].toArray();
        if (!choices.isEmpty() && choices[0].isObject()) {
            QJsonObject choice = choices[0].toObject();
            if (choice.contains(QStringLiteral("message")) && choice[QStringLiteral("message")].isObject()) {
                QJsonObject message = choice[QStringLiteral("message")].toObject();
                result.content = message[QStringLiteral("content")].toString();
            }
        }
    }
    
    // Extract usage statistics
    if (rootObj.contains(QStringLiteral("usage")) && rootObj[QStringLiteral("usage")].isObject()) {
        QJsonObject usage = rootObj[QStringLiteral("usage")].toObject();
        result.usage.inputTokens = usage[QStringLiteral("prompt_tokens")].toInt(0);
        result.usage.outputTokens = usage[QStringLiteral("completion_tokens")].toInt(0);
        result.usage.totalTokens = usage[QStringLiteral("total_tokens")].toInt(0);
    }
    
    return result;
}
