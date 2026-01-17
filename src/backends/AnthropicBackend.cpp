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

#include "AnthropicBackend.h"
#include "ModelCapsRegistry.h"
#include "core/LLMProviderRegistry.h"
#include "logging_categories.h"
#include "Logger.h"
#include <QtConcurrent>
#include <cpr/cpr.h>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>

QString AnthropicBackend::id() const {
    return QStringLiteral("anthropic");
}

QString AnthropicBackend::name() const {
    return QStringLiteral("Anthropic");
}

QStringList AnthropicBackend::availableModels() const {
    QMutexLocker locker(&m_cacheMutex);
    return m_cachedModels;
}

QStringList AnthropicBackend::availableEmbeddingModels() const {
    return {};
}

QFuture<QStringList> AnthropicBackend::fetchModelList() {
    return QtConcurrent::run([this]() -> QStringList {
        const QString apiKey = LLMProviderRegistry::instance().getCredential(QStringLiteral("anthropic"));
        if (apiKey.isEmpty()) {
            CP_WARN << "AnthropicBackend::fetchModelList: API key not found";
            return availableModels();
        }

        auto response = cpr::Get(
            cpr::Url{"https://api.anthropic.com/v1/models"},
            cpr::Header{
                {"x-api-key", apiKey.toStdString()},
                {"anthropic-version", "2023-06-01"},
                {"content-type", "application/json"}
            },
            cpr::Timeout{std::chrono::seconds(30)}
        );

        if (response.status_code != 200) {
            CP_WARN << "AnthropicBackend::fetchModelList: Failed to fetch models. HTTP Status:" << response.status_code;
            return availableModels();
        }

        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.text));
        if (doc.isNull() || !doc.isObject()) {
            CP_WARN << "AnthropicBackend::fetchModelList: Invalid JSON response";
            return availableModels();
        }

        QJsonObject root = doc.object();
        QJsonArray data = root.value(QStringLiteral("data")).toArray();
        QStringList models;
        for (const QJsonValue& val : data) {
            QString id = val.toObject().value(QStringLiteral("id")).toString();
            if (!id.isEmpty() && ModelCapsRegistry::instance().isSupported(QStringLiteral("anthropic"), id)) {
                models.append(id);
            }
        }

        // 3) Inject Virtual Models
        const auto virtualModels = ModelCapsRegistry::instance().virtualModelsForBackend(this->id());
        QSet<QString> virtualIds;
        for (const auto& vm : virtualModels) {
            models.append(vm.id);
            virtualIds.insert(vm.id);
        }

        // Keep deterministic order
        models.removeDuplicates();
        std::sort(models.begin(), models.end(), [&virtualIds](const QString& a, const QString& b) {
            const bool aVirtual = virtualIds.contains(a);
            const bool bVirtual = virtualIds.contains(b);
            if (aVirtual != bVirtual) {
                return aVirtual; // Virtual models first
            }
            return a.localeAwareCompare(b) < 0;
        });

        QMutexLocker locker(&m_cacheMutex);
        m_cachedModels = models;
        return m_cachedModels;
    });
}

LLMResult AnthropicBackend::sendPrompt(
    const QString& apiKey,
    const QString& modelName,
    double temperature,
    int maxTokens,
    const QString& systemPrompt,
    const QString& userPrompt,
    const LLMMessage& message
) {
    LLMResult result;
    
    // Resolve alias to real ID for the API request
    const QString resolvedModel = ModelCapsRegistry::instance().resolveAlias(modelName, id());
    if (resolvedModel != modelName) {
        CP_CLOG(cp_lifecycle).noquote() << "[ModelLifecycle] Resolving alias" << modelName << "to" << resolvedModel;
    }

    // Resolve model caps for role normalization and capability-driven behavior
    const auto resolved = ModelCapsRegistry::instance().resolveWithRule(resolvedModel, id());
    const auto capsOpt = resolved.has_value() ? std::optional<ModelCapsTypes::ModelCaps>(resolved->caps) : std::nullopt;
    const ModelCapsTypes::RoleMode roleMode = capsOpt.has_value() ? capsOpt->roleMode : ModelCapsTypes::RoleMode::System;

    // Anthropic API requires a non-empty messages array.
    if (userPrompt.trimmed().isEmpty() && message.attachments.isEmpty()) {
        result.hasError = true;
        result.errorMsg = QStringLiteral("User prompt or attachment must be provided for Anthropic API");
        return result;
    }

    // Prepare JSON payload
    QJsonObject root;
    root.insert(QStringLiteral("model"), resolvedModel);

    bool hasPdf = false;
    for (const auto& att : message.attachments) {
        if (att.mimeType == QStringLiteral("application/pdf")) {
            hasPdf = true;
            break;
        }
    }

    // Default to 4096 if not provided, as required by Anthropic
    int finalMaxTokens = (maxTokens > 0) ? maxTokens : 4096;
    root.insert(QStringLiteral("max_tokens"), finalMaxTokens);

    root.insert(QStringLiteral("temperature"), temperature);

    // Anthropic requires system prompts to be in a top-level 'system' field.
    // We use roleMode to decide if this field should be populated.
    if (!systemPrompt.isEmpty() && roleMode == ModelCapsTypes::RoleMode::SystemParameter) {
        root.insert(QStringLiteral("system"), systemPrompt);
    }

    // Build messages array
    QJsonArray messages;
    QJsonObject userMsg;
    userMsg.insert(QStringLiteral("role"), QStringLiteral("user"));

    if (!message.attachments.isEmpty()) {
        QJsonArray contentArray;

        for (const auto& attachment : message.attachments) {
            QString base64Data = QString::fromLatin1(attachment.data.toBase64());

            if (attachment.mimeType == QStringLiteral("application/pdf")) {
                QJsonObject documentBlock;
                documentBlock.insert(QStringLiteral("type"), QStringLiteral("document"));
                QJsonObject source;
                source.insert(QStringLiteral("type"), QStringLiteral("base64"));
                source.insert(QStringLiteral("media_type"), attachment.mimeType);
                source.insert(QStringLiteral("data"), base64Data);
                documentBlock.insert(QStringLiteral("source"), source);
                contentArray.append(documentBlock);
            } else if (attachment.mimeType.startsWith(QStringLiteral("image/"))) {
                QJsonObject imageBlock;
                imageBlock.insert(QStringLiteral("type"), QStringLiteral("image"));
                QJsonObject source;
                source.insert(QStringLiteral("type"), QStringLiteral("base64"));
                source.insert(QStringLiteral("media_type"), attachment.mimeType);
                source.insert(QStringLiteral("data"), base64Data);
                imageBlock.insert(QStringLiteral("source"), source);
                contentArray.append(imageBlock);
            }
        }

        // Text block
        if (!userPrompt.isEmpty()) {
            QJsonObject textBlock;
            textBlock.insert(QStringLiteral("type"), QStringLiteral("text"));
            textBlock.insert(QStringLiteral("text"), userPrompt);
            contentArray.append(textBlock);
        }

        userMsg.insert(QStringLiteral("content"), contentArray);
    } else {
        userMsg.insert(QStringLiteral("content"), userPrompt);
    }

    messages.append(userMsg);
    root.insert(QStringLiteral("messages"), messages);

    QJsonDocument doc(root);
    std::string jsonPayload = doc.toJson(QJsonDocument::Compact).toStdString();

    try {
        cpr::Header header{
            {"x-api-key", apiKey.toStdString()},
            {"anthropic-version", "2023-06-01"},
            {"Content-Type", "application/json"}
        };

        if (hasPdf) {
            header.insert({"anthropic-beta", "pdfs-2024-09-25"});
        }

        if (resolved.has_value()) {
            for (auto it = resolved->caps.customHeaders.begin(); it != resolved->caps.customHeaders.end(); ++it) {
                header.insert({it.key().toStdString(), it.value().toStdString()});
            }
        }

        auto response = cpr::Post(
            cpr::Url{"https://api.anthropic.com/v1/messages"},
            header,
            cpr::Body{jsonPayload},
            cpr::Timeout{std::chrono::seconds(60)}
        );

        result.rawResponse = QString::fromStdString(response.text);

        if (response.status_code == 200) {
            QJsonDocument resDoc = QJsonDocument::fromJson(result.rawResponse.toUtf8());
            QJsonObject resObj = resDoc.object();

            // Extract content[0].text
            QJsonArray contentArray = resObj.value(QStringLiteral("content")).toArray();
            if (!contentArray.isEmpty()) {
                result.content = contentArray.at(0).toObject().value(QStringLiteral("text")).toString();
            }

            // Extract usage
            QJsonObject usageObj = resObj.value(QStringLiteral("usage")).toObject();
            result.usage.inputTokens = usageObj.value(QStringLiteral("input_tokens")).toInt();
            result.usage.outputTokens = usageObj.value(QStringLiteral("output_tokens")).toInt();
            result.usage.totalTokens = result.usage.inputTokens + result.usage.outputTokens;

            result.hasError = false;
        } else {
            result.hasError = true;
            if (result.rawResponse.isEmpty()) {
                result.errorMsg = QStringLiteral("HTTP %1").arg(response.status_code);
            } else {
                // Try to parse Anthropic error message
                QJsonDocument resDoc = QJsonDocument::fromJson(result.rawResponse.toUtf8());
                QJsonObject resObj = resDoc.object();
                QJsonObject errorObj = resObj.value(QStringLiteral("error")).toObject();
                QString msg = errorObj.value(QStringLiteral("message")).toString();

                if (!msg.isEmpty()) {
                    result.errorMsg = QStringLiteral("HTTP %1: %2").arg(QString::number(response.status_code), msg);
                } else {
                    result.errorMsg = QStringLiteral("HTTP %1: %2").arg(QString::number(response.status_code), result.rawResponse);
                }
            }
            result.content = result.errorMsg;
        }
    } catch (const std::exception& e) {
        result.hasError = true;
        result.errorMsg = QString::fromStdString(e.what());
        result.content = result.errorMsg;
    }

    return result;
}

EmbeddingResult AnthropicBackend::getEmbedding(
    const QString& apiKey,
    const QString& modelName,
    const QString& text
) {
    Q_UNUSED(apiKey)
    Q_UNUSED(modelName)
    Q_UNUSED(text)
    
    EmbeddingResult result;
    result.hasError = true;
    result.errorMsg = QStringLiteral("Anthropic embeddings not supported");
    return result;
}

QFuture<QString> AnthropicBackend::generateImage(
    const QString& prompt,
    const QString& model,
    const QString& size,
    const QString& quality,
    const QString& style,
    const QString& targetDir
) {
    Q_UNUSED(prompt)
    Q_UNUSED(model)
    Q_UNUSED(size)
    Q_UNUSED(quality)
    Q_UNUSED(style)
    Q_UNUSED(targetDir)
    
    return QtConcurrent::run([]() {
        return QStringLiteral("Anthropic does not support image generation");
    });
}
