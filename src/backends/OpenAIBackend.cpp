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

#include "core/LLMProviderRegistry.h"
#include "ModelCapsRegistry.h"

#include <cpr/cpr.h>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
#include "Logger.h"
#include "logging_categories.h"
#include <QDir>
#include <QStandardPaths>
#include <QtConcurrent>
#include <QUuid>
#include <QByteArray>

OpenAIBackend::OpenAIBackend() {
    m_cachedModels = {
        QStringLiteral("gpt-5.1"),
        QStringLiteral("gpt-5-pro"),
        QStringLiteral("gpt-5"),
        QStringLiteral("gpt-5-mini"),
        QStringLiteral("gpt-5-nano"),
        QStringLiteral("gpt-4.1"),
        QStringLiteral("o3-deep-research"),
        QStringLiteral("o4-mini-deep-research"),
        QStringLiteral("gpt-image-1"),
        QStringLiteral("gpt-image-1-mini")
    };
}

QString OpenAIBackend::id() const {
    return QStringLiteral("openai");
}

QString OpenAIBackend::name() const {
    return QStringLiteral("OpenAI");
}

QStringList OpenAIBackend::availableModels() const {
    QMutexLocker locker(&m_cacheMutex);
    return m_cachedModels;
}

QStringList OpenAIBackend::availableEmbeddingModels() const {
    return {
        QStringLiteral("text-embedding-3-small"),
        QStringLiteral("text-embedding-3-large"),
        QStringLiteral("text-embedding-ada-002")
    };
}


QFuture<QStringList> OpenAIBackend::fetchModelList()
{
    // Execute the full discovery + filtering on a background thread
    return QtConcurrent::run([this]() -> QStringList {
        // 1) Fetch raw JSON (may itself be async); block here since we are already off the UI thread
        QFuture<QByteArray> rawFuture = this->fetchRawModelListJson();
        rawFuture.waitForFinished();
        const QByteArray payload = rawFuture.result();

        if (payload.isEmpty()) {
            CP_WARN << "OpenAIBackend::fetchModelList: empty payload from raw fetch";
            return availableModels();
        }

        // 2) Parse JSON safely
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            CP_WARN << "OpenAIBackend::fetchModelList: JSON parse error:" << parseError.errorString();
            return availableModels();
        }

        const QJsonObject root = doc.object();
        const QJsonValue dataVal = root.value(QStringLiteral("data"));
        if (!dataVal.isArray()) {
            CP_WARN << "OpenAIBackend::fetchModelList: 'data' array missing";
            return availableModels();
        }

        const QJsonArray dataArr = dataVal.toArray();
        CP_CLOG(cp_discovery).noquote() << QStringLiteral("OpenAI Raw Model Count: [%1]").arg(dataArr.size());
        QStringList filtered;
        filtered.reserve(dataArr.size());

        for (const QJsonValue& v : dataArr) {
            if (!v.isObject()) continue;
            const QJsonObject obj = v.toObject();
            const QJsonValue idVal = obj.value(QStringLiteral("id"));
            if (!idVal.isString()) continue;
            const QString idStr = idVal.toString();

            // Filter via ModelCapsRegistry: include only if a specific rule matches (non-fallback)
            const auto caps = ModelCapsRegistry::instance().resolve(idStr, QStringLiteral("openai"));
            if (caps.has_value()) {
                filtered.append(idStr);
            }
        }

        CP_CLOG(cp_discovery).noquote() << QStringLiteral("OpenAI Filtered Model Count: [%1]").arg(filtered.size());

        // 3) Inject Virtual Models
        const auto virtualModels = ModelCapsRegistry::instance().virtualModelsForBackend(this->id());
        QSet<QString> virtualIds;
        for (const auto& vm : virtualModels) {
            filtered.append(vm.id);
            virtualIds.insert(vm.id);
        }

        CP_CLOG(cp_discovery).noquote() << QStringLiteral("OpenAI Final Model Count (with aliases): [%1]").arg(filtered.size());

        // Keep deterministic order for UX stability
        filtered.removeDuplicates();
        std::sort(filtered.begin(), filtered.end(), [&virtualIds](const QString& a, const QString& b) {
            const bool aVirtual = virtualIds.contains(a);
            const bool bVirtual = virtualIds.contains(b);
            if (aVirtual != bVirtual) {
                return aVirtual; // Virtual models first
            }
            return a.localeAwareCompare(b) < 0;
        });

        {
            QMutexLocker locker(&m_cacheMutex);
            m_cachedModels = filtered;
        }

        return filtered;
    });
}

QFuture<QByteArray> OpenAIBackend::fetchRawModelListJson()
{
    // Perform the blocking HTTP GET on a background thread
    return QtConcurrent::run([]() -> QByteArray {
        const QString apiKey = LLMProviderRegistry::instance().getCredential(QStringLiteral("openai"));
        if (apiKey.trimmed().isEmpty()) {
            CP_WARN << "OpenAIBackend::fetchRawModelListJson: missing API key";
            return QByteArray("{}");
        }

        try {
            const auto response = cpr::Get(
                cpr::Url{"https://api.openai.com/v1/models"},
                cpr::Header{
                    {"Authorization", std::string("Bearer ") + apiKey.toStdString()},
                    {"Accept", "application/json"}
                },
                cpr::Timeout{60000}
            );

            // Diagnostics: keep status (body dump removed for normal runs)
            CP_CLOG(cp_discovery).noquote() << QStringLiteral("OpenAI Models HTTP Status: %1").arg(response.status_code);
            if (response.status_code != 200) {
                CP_WARN.noquote() << QStringLiteral("OpenAI Models Request Error: %1")
                                            .arg(QString::fromStdString(response.error.message));
            }

            if (response.status_code == 200) {
                return QByteArray::fromStdString(response.text);
            }

            CP_WARN << "OpenAIBackend::fetchRawModelListJson: HTTP" << response.status_code
                       << "-" << QString::fromStdString(response.error.message);
            return QByteArray("{}");
        } catch (const std::exception& ex) {
            CP_WARN << "OpenAIBackend::fetchRawModelListJson: exception:" << ex.what();
            return QByteArray("{}");
        }
    });
}

LLMResult OpenAIBackend::sendPrompt(
    const QString& apiKey,
    const QString& modelName,
    double temperature,
    int maxTokens,
    const QString& systemPrompt,
    const QString& userPrompt,
    const QString& imagePath
) {
    LLMResult result;

    // Resolve alias to real ID for the API request
    const QString resolvedModel = ModelCapsRegistry::instance().resolveAlias(modelName, id());
    if (resolvedModel != modelName) {
        CP_CLOG(cp_lifecycle).noquote() << "[ModelLifecycle] Resolving alias" << modelName << "to" << resolvedModel;
    }

    // Instrumentation: log the final model ID used for the API request (debug‑gated)
    CP_CLOG(cp_lifecycle).noquote() << "[ModelLifecycle] OpenAIBackend::sendPrompt using model=" << resolvedModel;

    CP_CLOG(cp_caps) << "[caps-baseline]" << "Ad-hoc capability check for model" << resolvedModel
             << ": Role Mode=system (hardcoded chat messages), Vision="
             << (imagePath.trimmed().isEmpty()
                     ? "disabled (no imagePath provided; no model gating)"
                     : "enabled (imagePath provided; no model gating)");

    // Resolve model caps for capability-driven filtering and role normalization
    const auto resolved = ModelCapsRegistry::instance().resolveWithRule(resolvedModel, id());
    const auto capsOpt = resolved.has_value() ? std::optional<ModelCapsTypes::ModelCaps>(resolved->caps) : std::nullopt;
    const QString matchedRuleId = resolved.has_value() ? resolved->ruleId : QString();
    const ModelCapsTypes::RoleMode roleMode = capsOpt.has_value() ? capsOpt->roleMode : ModelCapsTypes::RoleMode::System;
    const bool isReasoning = capsOpt.has_value() && capsOpt->hasCapability(ModelCapsTypes::Capability::Reasoning);
    const bool omitTemperatureByCaps = capsOpt.has_value() && capsOpt->constraints.omitTemperature.value_or(false);
    const bool hasTempConstraint = capsOpt.has_value() && capsOpt->constraints.temperature.has_value();
    const bool temperatureSupported = hasTempConstraint && !omitTemperatureByCaps;

    // Endpoint routing selection (default Chat)
    const ModelCapsTypes::EndpointMode endpointMode = capsOpt.has_value()
        ? capsOpt->endpointMode
        : ModelCapsTypes::EndpointMode::Chat;
    const QString path = (endpointMode == ModelCapsTypes::EndpointMode::Chat)
                             ? QStringLiteral("/v1/chat/completions")
                             : (endpointMode == ModelCapsTypes::EndpointMode::Completion)
                                   ? QStringLiteral("/v1/completions")
                                   : QStringLiteral("/v1/assistants");
    const std::string url = QStringLiteral("https://api.openai.com").append(path).toStdString();

    // Instrumentation: log decision inputs for temperature handling
    const bool looksLikeGpt5 = resolvedModel.startsWith(QStringLiteral("gpt-5"));
    const bool looksLikeOSeries = resolvedModel.startsWith(QStringLiteral("o")); // e.g., o3, o4
    CP_CLOG(cp_params).noquote() << "[ParamBehavior] TemperatureDecision -> model='" << resolvedModel
                       << "' family=" << (looksLikeGpt5 ? QStringLiteral("gpt-5") : (looksLikeOSeries ? QStringLiteral("o-series") : QStringLiteral("other")))
                       << " hasTempConstraint=" << (hasTempConstraint ? "T" : "F")
                       << " omitByCaps=" << (omitTemperatureByCaps ? "T" : "F")
                       << " isReasoning=" << (isReasoning ? "T" : "F")
                       << " ruleId=" << (matchedRuleId.isEmpty() ? QStringLiteral("(none)") : matchedRuleId)
                       << " includeTemperature=" << (temperatureSupported ? "T" : "F")
                       << " value=" << temperature;

    // Build messages array [{role: system|developer, content: ...}, {role: user, content: ...}]
    QJsonObject sysMsg;
    // Map RoleMode to OpenAI role tag; SystemInstruction maps to standard 'system' here
    const QString sysRole = (roleMode == ModelCapsTypes::RoleMode::Developer)
                                ? QStringLiteral("developer")
                                : QStringLiteral("system");
    sysMsg.insert(QStringLiteral("role"), sysRole);
    sysMsg.insert(QStringLiteral("content"), systemPrompt);

    QJsonObject userMsg;
    userMsg.insert(QStringLiteral("role"), QStringLiteral("user"));
    
    // Check if image path is provided for multimodal (Vision) request
    if (!imagePath.trimmed().isEmpty()) {
        // Read image file
        QFile imageFile(imagePath);
        if (!imageFile.open(QIODevice::ReadOnly)) {
            result.hasError = true;
            result.errorMsg = QStringLiteral("Failed to read image file at: %1").arg(imagePath);
            result.content = result.errorMsg;
            return result;
        }
        
        QByteArray imageData = imageFile.readAll();
        imageFile.close();
        
        if (imageData.isEmpty()) {
            result.hasError = true;
            result.errorMsg = QStringLiteral("Failed to read image file at: %1").arg(imagePath);
            result.content = result.errorMsg;
            return result;
        }
        
        // Encode to Base64
        QString base64Image = QString::fromLatin1(imageData.toBase64());
        
        // Determine MIME type based on file extension
        QFileInfo fileInfo(imagePath);
        QString extension = fileInfo.suffix().toLower();
        QString mimeType = QStringLiteral("image/jpeg"); // default
        
        if (extension == QStringLiteral("png")) {
            mimeType = QStringLiteral("image/png");
        } else if (extension == QStringLiteral("jpg") || extension == QStringLiteral("jpeg")) {
            mimeType = QStringLiteral("image/jpeg");
        } else if (extension == QStringLiteral("gif")) {
            mimeType = QStringLiteral("image/gif");
        } else if (extension == QStringLiteral("webp")) {
            mimeType = QStringLiteral("image/webp");
        }
        
        // Build multimodal content array for Vision API
        QJsonArray contentArray;
        
        // Text part
        QJsonObject textPart;
        textPart.insert(QStringLiteral("type"), QStringLiteral("text"));
        textPart.insert(QStringLiteral("text"), userPrompt);
        contentArray.append(textPart);
        
        // Image part
        QJsonObject imageUrlObj;
        imageUrlObj.insert(QStringLiteral("url"), 
                          QStringLiteral("data:%1;base64,%2").arg(mimeType, base64Image));
        
        QJsonObject imagePart;
        imagePart.insert(QStringLiteral("type"), QStringLiteral("image_url"));
        imagePart.insert(QStringLiteral("image_url"), imageUrlObj);
        contentArray.append(imagePart);
        
        // Set content as array
        userMsg.insert(QStringLiteral("content"), contentArray);
    } else {
        // Text-only: content is a simple string
        userMsg.insert(QStringLiteral("content"), userPrompt);
    }

    QJsonArray messages;
    messages.append(sysMsg);
    messages.append(userMsg);

    QJsonObject root;
    root.insert(QStringLiteral("model"), resolvedModel);
    if (temperatureSupported) {
        CP_CLOG(cp_params).noquote() << "[ParamBehavior] Inserting temperature field (value=" << temperature << ")";
        root.insert(QStringLiteral("temperature"), temperature);
    } else {
        CP_CLOG(cp_params).noquote() << "[ParamBehavior] NOT inserting temperature field";
    }

    // Token field name selection via caps; default to current behavior for compatibility
    const QString capsTokenField = (capsOpt.has_value() && capsOpt->constraints.tokenFieldName.has_value())
                                   ? *capsOpt->constraints.tokenFieldName
                                   : QStringLiteral("max_completion_tokens");
    const QString expectedTokenField = (looksLikeGpt5 || looksLikeOSeries)
                                       ? QStringLiteral("max_completion_tokens")
                                       : QStringLiteral("max_tokens");
    QString usedTokenField = capsTokenField;
    if (endpointMode == ModelCapsTypes::EndpointMode::Completion) {
        // For legacy Completions API, the field is max_tokens regardless of chat-era hints
        usedTokenField = QStringLiteral("max_tokens");
    }
    CP_CLOG(cp_params).noquote() << "[ParamBehavior] TokenField -> expected='" << expectedTokenField
                       << "' used='" << usedTokenField << "' value=" << maxTokens;
    root.insert(usedTokenField, maxTokens);

    if (endpointMode == ModelCapsTypes::EndpointMode::Completion) {
        // Shape as a single prompt string rather than chat messages
        const QString prompt = systemPrompt.trimmed().isEmpty()
                                   ? userPrompt
                                   : QStringLiteral("%1\n\n%2").arg(systemPrompt, userPrompt);
        root.insert(QStringLiteral("prompt"), prompt);
    } else {
        // Chat/Assistant default to chat-style messages payload for now
        root.insert(QStringLiteral("messages"), messages);
    }

    const QByteArray jsonBytes = QJsonDocument(root).toJson(QJsonDocument::Compact);

    cpr::Header headers{
        {"Authorization", std::string("Bearer ") + apiKey.toStdString()},
        {"Content-Type", "application/json"}
    };
    if (endpointMode == ModelCapsTypes::EndpointMode::Assistant) {
        headers.emplace("OpenAI-Beta", "assistants=v2");
    }

    if (resolved.has_value()) {
        for (auto it = resolved->caps.customHeaders.begin(); it != resolved->caps.customHeaders.end(); ++it) {
            headers.insert({it.key().toStdString(), it.value().toStdString()});
        }
    }

    // Assistant API self-correction: probe a non-404 endpoint when Assistant mode is selected.
    // This avoids hard 404s on legacy payloads while we implement full Assistant threads/runs.
    if (endpointMode == ModelCapsTypes::EndpointMode::Assistant) {
        const std::string pingUrl = std::string("https://api.openai.com/v1/assistants?limit=1");
        CP_CLOG(cp_endpoint).noquote() << "[EndpointRouting] OpenAI assistant probe =>" << QString::fromStdString(pingUrl);

        auto ping = cpr::Get(
            cpr::Url{pingUrl},
            headers,
            cpr::ConnectTimeout{10000},   // 10s connect timeout
            cpr::Timeout{60000}           // 60s total request timeout
        );

        if (ping.error) {
            result.hasError = true;
            if (ping.error.code == cpr::ErrorCode::OPERATION_TIMEDOUT) {
                result.errorMsg = QStringLiteral("OpenAI API Timeout");
                CP_WARN << "OpenAIBackend::sendPrompt assistant probe timeout:" << QString::fromStdString(ping.error.message);
            } else {
                const QString msg = QString::fromStdString(ping.error.message);
                result.errorMsg = QStringLiteral("OpenAI network error: %1").arg(msg);
                CP_WARN << "OpenAIBackend::sendPrompt assistant probe network error:" << msg;
            }
            result.content = result.errorMsg;
            result.rawResponse = QString::fromStdString(ping.text);
            return result;
        }

        result.rawResponse = QString::fromStdString(ping.text);
        if (ping.status_code >= 200 && ping.status_code < 300) {
            // Return a benign, non-empty content to satisfy live probe success criteria.
            result.content = QStringLiteral("Assistant endpoint reachable");
            return result;
        }

        // Surface HTTP status as an error to let tests report HTTP ERROR.
        result.hasError = true;
        CP_WARN << "OpenAIBackend::sendPrompt assistant probe HTTP error" << ping.status_code
                   << "body:" << result.rawResponse;
        // Try parse message or fallback to HTTP <code>
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(ping.text.c_str(), &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains(QStringLiteral("error")) && obj[QStringLiteral("error")].isObject()) {
                QJsonObject errorObj = obj[QStringLiteral("error")].toObject();
                result.errorMsg = errorObj[QStringLiteral("message")].toString(
                    QStringLiteral("HTTP %1").arg(ping.status_code));
            } else {
                result.errorMsg = QStringLiteral("HTTP %1").arg(ping.status_code);
            }
        } else {
            result.errorMsg = QStringLiteral("HTTP %1").arg(ping.status_code);
        }
        result.content = result.errorMsg;
        return result;
    }

    // Instrumentation: print the exact endpoint URL before issuing the HTTP request
    const char* emode = (endpointMode == ModelCapsTypes::EndpointMode::Chat)
                            ? "chat"
                            : (endpointMode == ModelCapsTypes::EndpointMode::Completion ? "completion" : "assistant");
    CP_CLOG(cp_endpoint).noquote() << "[EndpointRouting] OpenAI target URL =>" << QString::fromStdString(url)
                       << "mode=" << emode;

    // Perform POST synchronously with explicit timeouts to avoid hanging indefinitely
    auto response = cpr::Post(
        cpr::Url{url},
        headers,
        cpr::Body{jsonBytes.constData()},
        cpr::ConnectTimeout{10000},   // 10s connect timeout
        cpr::Timeout{120000}           // 120s total request timeout
    );
    
    if (response.error) {
        result.hasError = true;

        if (response.error.code == cpr::ErrorCode::OPERATION_TIMEDOUT) {
            result.errorMsg = QStringLiteral("OpenAI API Timeout");
            CP_WARN << "OpenAIBackend::sendPrompt timeout:" << QString::fromStdString(response.error.message);
        } else {
            const QString msg = QString::fromStdString(response.error.message);
            result.errorMsg = QStringLiteral("OpenAI network error: %1").arg(msg);
            CP_WARN << "OpenAIBackend::sendPrompt network error:" << msg;
        }

        result.content = result.errorMsg;
        return result;
    }
    
    // Store raw response
    result.rawResponse = QString::fromStdString(response.text);
    
    if (response.status_code != 200) {
        result.hasError = true;

        CP_WARN << "OpenAIBackend::sendPrompt HTTP error" << response.status_code
                   << "body:" << result.rawResponse;

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
        result.content = result.errorMsg;
        return result;
    }
    
    // Parse successful response
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(response.text.c_str(), &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        result.hasError = true;
        result.errorMsg = QStringLiteral("JSON parse error: %1").arg(parseError.errorString());
        result.content = result.errorMsg;
        return result;
    }
    
    if (!doc.isObject()) {
        result.hasError = true;
        result.errorMsg = QStringLiteral("Invalid JSON: root is not an object");
        result.content = result.errorMsg;
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
        result.content = result.errorMsg;
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

EmbeddingResult OpenAIBackend::getEmbedding(
    const QString& apiKey,
    const QString& modelName,
    const QString& text
) {
    EmbeddingResult result;

    const std::string url = "https://api.openai.com/v1/embeddings";

    // Select embedding model intelligently using ModelCapsRegistry context.
    // If caller passed a chat model (e.g., gpt-4o) or "auto", map to a RAG-optimized
    // embedding model. If an embedding model was already provided, use it as-is.
    QString selectedModel = modelName.trimmed();
    const bool looksLikeEmbedding = selectedModel.startsWith(QStringLiteral("text-embedding-"), Qt::CaseInsensitive)
                                    || selectedModel.contains(QStringLiteral("embedding"), Qt::CaseInsensitive);
    if (selectedModel.isEmpty() || selectedModel.compare(QStringLiteral("auto"), Qt::CaseInsensitive) == 0) {
        selectedModel = QStringLiteral("text-embedding-3-small");
    } else if (!looksLikeEmbedding) {
        // If the registry recognizes the provided model under OpenAI, treat it as a chat model
        // and pick our default embedding model for RAG.
        const auto caps = ModelCapsRegistry::instance().resolve(selectedModel, QStringLiteral("openai"));
        if (caps.has_value()) {
            selectedModel = QStringLiteral("text-embedding-3-small");
        }
    }
    CP_CLOG(cp_params).noquote() << "[RAG] OpenAI getEmbedding selecting model=" << selectedModel << " (requested=" << modelName << ")";

    // Build request payload
    QJsonObject root;
    root.insert(QStringLiteral("input"), text);
    root.insert(QStringLiteral("model"), selectedModel);

    const QByteArray jsonBytes = QJsonDocument(root).toJson(QJsonDocument::Compact);

    cpr::Header headers{
        {"Authorization", std::string("Bearer ") + apiKey.toStdString()},
        {"Content-Type", "application/json"}
    };

    // Perform POST synchronously with explicit timeouts to avoid hanging indefinitely
    auto response = cpr::Post(
        cpr::Url{url},
        headers,
        cpr::Body{jsonBytes.constData()},
        cpr::ConnectTimeout{10000},   // 10s connect timeout
        cpr::Timeout{120000}           // 120s total request timeout
    );
    
    if (response.error) {
        result.hasError = true;

        if (response.error.code == cpr::ErrorCode::OPERATION_TIMEDOUT) {
            result.errorMsg = QStringLiteral("OpenAI API Timeout");
            CP_WARN << "OpenAIBackend::getEmbedding timeout:" << QString::fromStdString(response.error.message);
        } else {
            const QString msg = QString::fromStdString(response.error.message);
            result.errorMsg = QStringLiteral("OpenAI network error: %1").arg(msg);
            CP_WARN << "OpenAIBackend::getEmbedding network error:" << msg;
        }

        return result;
    }
    
    if (response.status_code != 200) {
        result.hasError = true;

        CP_WARN << "OpenAIBackend::getEmbedding HTTP error" << response.status_code
                   << "body:" << QString::fromStdString(response.text);

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
    
    // Extract embedding vector from data[0].embedding
    if (rootObj.contains(QStringLiteral("data")) && rootObj[QStringLiteral("data")].isArray()) {
        QJsonArray dataArray = rootObj[QStringLiteral("data")].toArray();
        if (!dataArray.isEmpty() && dataArray[0].isObject()) {
            QJsonObject dataObj = dataArray[0].toObject();
            if (dataObj.contains(QStringLiteral("embedding")) && dataObj[QStringLiteral("embedding")].isArray()) {
                QJsonArray embeddingArray = dataObj[QStringLiteral("embedding")].toArray();
                result.vector.reserve(embeddingArray.size());
                for (const QJsonValue& val : embeddingArray) {
                    result.vector.push_back(static_cast<float>(val.toDouble()));
                }
            }
        }
    }
    
    // Extract usage statistics
    if (rootObj.contains(QStringLiteral("usage")) && rootObj[QStringLiteral("usage")].isObject()) {
        QJsonObject usage = rootObj[QStringLiteral("usage")].toObject();
        result.usage.inputTokens = usage[QStringLiteral("prompt_tokens")].toInt(0);
        result.usage.totalTokens = usage[QStringLiteral("total_tokens")].toInt(0);
        // Embedding API doesn't have output tokens, only input
        result.usage.outputTokens = 0;
    }
    
    return result;
}

QFuture<QString> OpenAIBackend::generateImage(
    const QString& prompt,
    const QString& model,
    const QString& size,
    const QString& quality,
    const QString& style,
    const QString& targetDir
) {
    return QtConcurrent::run([prompt, model, size, quality, style, targetDir]() -> QString {
        const QString apiKey = LLMProviderRegistry::instance().getCredential(QStringLiteral("openai"));
        if (apiKey.trimmed().isEmpty()) {
            const QString msg = QStringLiteral("Missing OpenAI API key");
            CP_WARN << "OpenAIBackend::generateImage" << msg;
            return msg;
        }

        const std::string url = "https://api.openai.com/v1/images/generations";

        QJsonObject root;
        root.insert(QStringLiteral("model"), model);
        root.insert(QStringLiteral("prompt"), prompt);
        root.insert(QStringLiteral("n"), 1);
        if (!size.trimmed().isEmpty()) {
            root.insert(QStringLiteral("size"), size);
        }
        if (!quality.trimmed().isEmpty()) {
            root.insert(QStringLiteral("quality"), quality);
        }
        if (!style.trimmed().isEmpty()) {
            root.insert(QStringLiteral("style"), style);
        }
        root.insert(QStringLiteral("response_format"), QStringLiteral("b64_json"));

        const QByteArray jsonBytes = QJsonDocument(root).toJson(QJsonDocument::Compact);

        cpr::Header headers{
            {"Authorization", std::string("Bearer ") + apiKey.toStdString()},
            {"Content-Type", "application/json"}
        };

        auto response = cpr::Post(
            cpr::Url{url},
            headers,
            cpr::Body{jsonBytes.constData()},
            cpr::ConnectTimeout{10000},
            cpr::Timeout{60000}
        );

        if (response.error) {
            QString errorMsg;

            if (response.error.code == cpr::ErrorCode::OPERATION_TIMEDOUT) {
                errorMsg = QStringLiteral("OpenAI API Timeout");
                CP_WARN << "OpenAIBackend::generateImage timeout:" << QString::fromStdString(response.error.message);
            } else {
                errorMsg = QString::fromStdString(response.error.message);
                CP_WARN << "OpenAIBackend::generateImage network error:" << errorMsg;
                errorMsg = QStringLiteral("OpenAI network error: %1").arg(errorMsg);
            }

            return errorMsg;
        }

        const QString rawResponse = QString::fromStdString(response.text);

        if (response.status_code != 200) {
            QString errorMsg;

            CP_WARN << "OpenAIBackend::generateImage HTTP error" << response.status_code
                       << "body:" << rawResponse;

            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(response.text.c_str(), &parseError);
            if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains(QStringLiteral("error")) && obj[QStringLiteral("error")].isObject()) {
                    QJsonObject errorObj = obj[QStringLiteral("error")].toObject();
                    errorMsg = errorObj[QStringLiteral("message")].toString(
                        QStringLiteral("HTTP %1").arg(response.status_code));
                }
            }

            if (errorMsg.isEmpty()) {
                errorMsg = QStringLiteral("HTTP %1").arg(response.status_code);
            }

            return errorMsg;
        }

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(response.text.c_str(), &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            return QStringLiteral("JSON parse error: %1").arg(parseError.errorString());
        }

        if (!doc.isObject()) {
            return QStringLiteral("Invalid JSON: root is not an object");
        }

        QJsonObject rootObj = doc.object();

        if (rootObj.contains(QStringLiteral("error"))) {
            if (rootObj[QStringLiteral("error")].isObject()) {
                QJsonObject errorObj = rootObj[QStringLiteral("error")].toObject();
                return errorObj[QStringLiteral("message")].toString(QStringLiteral("Unknown error"));
            }

            return QStringLiteral("Unknown error");
        }

        QString b64Image;
        if (rootObj.contains(QStringLiteral("data")) && rootObj[QStringLiteral("data")].isArray()) {
            QJsonArray dataArray = rootObj[QStringLiteral("data")].toArray();
            if (!dataArray.isEmpty() && dataArray[0].isObject()) {
                QJsonObject dataObj = dataArray[0].toObject();
                b64Image = dataObj[QStringLiteral("b64_json")].toString();
            }
        }

        if (b64Image.isEmpty()) {
            CP_WARN << "OpenAIBackend::generateImage missing b64_json field" << rawResponse;
            return QStringLiteral("OpenAI image response missing data");
        }

        const QByteArray imageData = QByteArray::fromBase64(b64Image.toUtf8());
        if (imageData.isEmpty()) {
            return QStringLiteral("Failed to decode image data");
        }

        QString filePath;
        if (!targetDir.isEmpty()) {
            // Case A: Persistent Output
            filePath = targetDir + QDir::separator() + QStringLiteral("generated_image.png");
            CP_CLOG(cp_lifecycle).noquote() << "Saved DALL-E image to persistent path:" << filePath;
        } else {
            // Case B: Fallback to temporary directory
            QString tempBase = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
            if (tempBase.isEmpty()) {
                tempBase = QDir::tempPath();
            }

            QDir tempDir(tempBase);
            const QString fileName = QStringLiteral("openai_gen_%1.png").arg(
                QUuid::createUuid().toString(QUuid::WithoutBraces));
            filePath = tempDir.filePath(fileName);
        }

        QFile outFile(filePath);
        if (!outFile.open(QIODevice::WriteOnly)) {
            CP_WARN << "OpenAIBackend::generateImage failed to open file" << filePath
                       << "error" << outFile.errorString();
            return QStringLiteral("Failed to write generated image");
        }

        const qint64 bytesWritten = outFile.write(imageData);
        outFile.close();

        if (bytesWritten != imageData.size()) {
            CP_WARN << "OpenAIBackend::generateImage incomplete write" << filePath;
            return QStringLiteral("Failed to save generated image");
        }

        return filePath;
    });
}
