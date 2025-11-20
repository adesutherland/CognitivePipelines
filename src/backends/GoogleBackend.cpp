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
#include "GoogleBackend.h"

#include <cpr/cpr.h>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

QString GoogleBackend::id() const {
    return QStringLiteral("google");
}

QString GoogleBackend::name() const {
    return QStringLiteral("Google Gemini");
}

QStringList GoogleBackend::availableModels() const {
    return {
        QStringLiteral("gemini-2.5-flash"),
        QStringLiteral("gemini-2.5-pro"),
        QStringLiteral("gemini-2.5-flash-lite"),
        QStringLiteral("gemini-1.5-pro"),
        QStringLiteral("gemini-1.5-flash")
    };
}

LLMResult GoogleBackend::sendPrompt(
    const QString& apiKey,
    const QString& modelName,
    double temperature,
    int maxTokens,
    const QString& systemPrompt,
    const QString& userPrompt
) {
    LLMResult result;
    
    // Google Generative Language (Gemini) stable v1 endpoint.
    // API key is passed as a query parameter, not via Authorization header.
    const std::string url = std::string("https://generativelanguage.googleapis.com/v1/models/")
                            + modelName.toStdString()
                            + ":generateContent?key="
                            + apiKey.toStdString();

    // v1 request schema: contents is an array of content objects, each
    // with its own parts array. We send system and user as separate
    // content entries to keep semantics clear and future‑proof:
    //   contents: [
    //     {"parts":[{"text":"<system>"}]},
    //     {"parts":[{"text":"<user>"}]}
    //   ]
    QJsonArray contents;

    if (!systemPrompt.trimmed().isEmpty()) {
        QJsonObject sysPart;
        sysPart.insert(QStringLiteral("text"), systemPrompt);
        QJsonObject sysContent;
        sysContent.insert(QStringLiteral("parts"), QJsonArray{sysPart});
        contents.append(sysContent);
    }

    QJsonObject userPart;
    userPart.insert(QStringLiteral("text"), userPrompt);
    QJsonObject userContent;
    userContent.insert(QStringLiteral("parts"), QJsonArray{userPart});
    contents.append(userContent);

    QJsonObject generationConfig;
    generationConfig.insert(QStringLiteral("temperature"), temperature);
    generationConfig.insert(QStringLiteral("maxOutputTokens"), maxTokens);

    QJsonObject root;
    root.insert(QStringLiteral("contents"), contents);
    root.insert(QStringLiteral("generationConfig"), generationConfig);

    const QByteArray jsonBytes = QJsonDocument(root).toJson(QJsonDocument::Compact);

    // Do NOT set Authorization header for Google; only Content-Type
    cpr::Header headers{
        {"Content-Type", "application/json"}
    };

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
    
    // Extract content from candidates[0].content.parts[0].text
    if (rootObj.contains(QStringLiteral("candidates")) && rootObj[QStringLiteral("candidates")].isArray()) {
        QJsonArray candidates = rootObj[QStringLiteral("candidates")].toArray();
        if (!candidates.isEmpty() && candidates[0].isObject()) {
            QJsonObject candidate = candidates[0].toObject();
            if (candidate.contains(QStringLiteral("content")) && candidate[QStringLiteral("content")].isObject()) {
                QJsonObject content = candidate[QStringLiteral("content")].toObject();
                if (content.contains(QStringLiteral("parts")) && content[QStringLiteral("parts")].isArray()) {
                    QJsonArray parts = content[QStringLiteral("parts")].toArray();
                    if (!parts.isEmpty() && parts[0].isObject()) {
                        QJsonObject part = parts[0].toObject();
                        result.content = part[QStringLiteral("text")].toString();
                    }
                }
            }
        }
    }
    
    // Extract usage statistics from usageMetadata
    if (rootObj.contains(QStringLiteral("usageMetadata")) && rootObj[QStringLiteral("usageMetadata")].isObject()) {
        QJsonObject usageMetadata = rootObj[QStringLiteral("usageMetadata")].toObject();
        result.usage.inputTokens = usageMetadata[QStringLiteral("promptTokenCount")].toInt(0);
        result.usage.outputTokens = usageMetadata[QStringLiteral("candidatesTokenCount")].toInt(0);
        result.usage.totalTokens = usageMetadata[QStringLiteral("totalTokenCount")].toInt(0);
    }
    
    return result;
}
