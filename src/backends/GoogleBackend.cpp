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
#include "core/LLMProviderRegistry.h"
#include "ModelCapsRegistry.h"

#include <cpr/cpr.h>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDebug>
#include <QLoggingCategory>
#include "../logging_categories.h"
#include <QtConcurrent>


GoogleBackend::GoogleBackend() {
    m_cachedModels = {
        QStringLiteral("gemini-3-pro-preview"),
        QStringLiteral("gemini-3-pro-image-preview"),
        QStringLiteral("gemini-2.5-pro"),
        QStringLiteral("gemini-2.5-flash"),
        QStringLiteral("gemini-2.5-flash-lite"),
        QStringLiteral("imagen-3")
    };
}

QString GoogleBackend::id() const {
    return QStringLiteral("google");
}

QString GoogleBackend::name() const {
    return QStringLiteral("Google Gemini");
}

QStringList GoogleBackend::availableModels() const {
    QMutexLocker locker(&m_cacheMutex);
    return m_cachedModels;
}

QStringList GoogleBackend::availableEmbeddingModels() const {
    return {
        QStringLiteral("text-embedding-004")
    };
}

QFuture<QStringList> GoogleBackend::fetchModelList()
{
    // Network fetch performed on a background thread
    return QtConcurrent::run([this]() -> QStringList {
        const QString apiKey = LLMProviderRegistry::instance().getCredential(QStringLiteral("google"));
        if (apiKey.trimmed().isEmpty()) {
            qWarning() << "GoogleBackend::fetchModelList: missing API key";
            return availableModels();
        }

        try {
            const std::string url = std::string("https://generativelanguage.googleapis.com/v1beta/models?key=")
                                    + apiKey.toStdString();

            const auto response = cpr::Get(
                cpr::Url{url},
                cpr::Header{{"Accept", "application/json"}},
                cpr::Timeout{60000}
            );

            if (response.status_code != 200) {
                qWarning() << "GoogleBackend::fetchModelList: HTTP" << response.status_code
                           << "-" << QString::fromStdString(response.error.message);
                return availableModels();
            }

            const QByteArray payload = QByteArray::fromStdString(response.text);
            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
            if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
                qWarning() << "GoogleBackend::fetchModelList: JSON parse error:" << parseError.errorString();
                return availableModels();
            }

            const QJsonObject root = doc.object();
            const QJsonValue modelsVal = root.value(QStringLiteral("models"));
            if (!modelsVal.isArray()) {
                qWarning() << "GoogleBackend::fetchModelList: 'models' array missing";
                return availableModels();
            }

            QStringList ids;
            const QJsonArray modelsArr = modelsVal.toArray();
            ids.reserve(modelsArr.size());
            for (const QJsonValue& v : modelsArr) {
                if (!v.isObject()) continue;
                const QJsonObject obj = v.toObject();
                const QJsonValue nameVal = obj.value(QStringLiteral("name"));
                if (!nameVal.isString()) continue;
                QString name = nameVal.toString(); // e.g., "models/gemini-pro"
                const QString prefix = QStringLiteral("models/");
                if (name.startsWith(prefix)) {
                    name = name.mid(prefix.size());
                }
                if (!name.isEmpty()) {
                    ids.append(name);
                }
            }

            // Apply registry-based filtering for Google, consistent with OpenAI
            ids.removeDuplicates();
            QStringList filtered;
            filtered.reserve(ids.size());
            for (const QString& id : ids) {
                if (ModelCapsRegistry::instance().resolve(id, QStringLiteral("google")).has_value()) {
                    filtered.append(id);
                }
            }

            // Deterministic order for UX stability
            filtered.removeDuplicates();
            std::sort(filtered.begin(), filtered.end(), [](const QString& a, const QString& b){ return a.localeAwareCompare(b) < 0; });

            {
                QMutexLocker locker(&m_cacheMutex);
                m_cachedModels = filtered;
            }

            return filtered;
        } catch (const std::exception& ex) {
            qWarning() << "GoogleBackend::fetchModelList: exception:" << ex.what();
            return availableModels();
        }
    });
}

LLMResult GoogleBackend::sendPrompt(
    const QString& apiKey,
    const QString& modelName,
    double temperature,
    int maxTokens,
    const QString& systemPrompt,
    const QString& userPrompt,
    const QString& imagePath
) {
    LLMResult result;

    // Instrumentation: log the final model ID used for the API request (debug‑gated)
    qCDebug(cp_lifecycle).noquote() << "[ModelLifecycle] GoogleBackend::sendPrompt using model=" << modelName;

    qCDebug(cp_caps) << "[caps-baseline]" << "Ad-hoc capability check for model" << modelName
             << ": Role Mode=system (system prompt as first content entry; no developer role), Vision="
             << (imagePath.trimmed().isEmpty()
                     ? "disabled (no imagePath provided; no model gating)"
                     : "enabled via inline_data (imagePath provided; no model gating)");

    // Google Generative Language (Gemini) endpoint selection:
    // Preview models use v1beta; certain families (e.g., early 1.5, 3.x) may require v1beta.
    // API key is passed as a query parameter, not via Authorization header.
    const bool isPreviewModel = modelName.contains(QStringLiteral("preview"), Qt::CaseInsensitive);
    const bool forceV1beta = modelName.startsWith(QStringLiteral("gemini-1.5-"), Qt::CaseInsensitive)
                           || modelName.startsWith(QStringLiteral("gemini-3-"), Qt::CaseInsensitive);
    const std::string apiVersion = (isPreviewModel || forceV1beta) ? "v1beta" : "v1";
    const std::string url = std::string("https://generativelanguage.googleapis.com/")
                            + apiVersion
                            + "/models/"
                            + modelName.toStdString()
                            + ":generateContent?key="
                            + apiKey.toStdString();

    // Resolve model caps for capability-driven filtering and role normalization
    const auto capsOpt = ModelCapsRegistry::instance().resolve(modelName, QStringLiteral("google"));
    const ModelCapsTypes::RoleMode roleMode = capsOpt.has_value() ? capsOpt->roleMode : ModelCapsTypes::RoleMode::System;
    const bool isReasoning = capsOpt.has_value() && capsOpt->hasCapability(ModelCapsTypes::Capability::Reasoning);
    const bool temperatureSupported = capsOpt.has_value() && capsOpt->constraints.temperature.has_value() && !isReasoning;

    // v1 request schema: contents is an array of content objects, each
    // with its own parts array. We send system and user as separate
    // content entries to keep semantics clear and future‑proof:
    //   contents: [
    //     {"parts":[{"text":"<system>"}]},
    //     {"parts":[{"text":"<user>"}]}
    //   ]
    QJsonArray contents;

    // If RoleMode indicates SystemInstruction, use top-level field instead of a first content entry
    const bool useSystemInstruction = (roleMode == ModelCapsTypes::RoleMode::SystemInstruction);
    QJsonObject systemInstructionObj;
    if (useSystemInstruction && !systemPrompt.trimmed().isEmpty()) {
        QJsonObject sysPart; sysPart.insert(QStringLiteral("text"), systemPrompt);
        systemInstructionObj.insert(QStringLiteral("parts"), QJsonArray{sysPart});
    } else if (!systemPrompt.trimmed().isEmpty()) {
        QJsonObject sysPart; sysPart.insert(QStringLiteral("text"), systemPrompt);
        QJsonObject sysContent; sysContent.insert(QStringLiteral("parts"), QJsonArray{sysPart});
        contents.append(sysContent);
    }

    // Build the user message parts array (text + optional image)
    QJsonArray userParts;
    
    // Always include the text part
    QJsonObject textPart;
    textPart.insert(QStringLiteral("text"), userPrompt);
    userParts.append(textPart);
    
    // Add image part if imagePath is provided
    if (!imagePath.trimmed().isEmpty()) {
        QFile imageFile(imagePath);
        if (!imageFile.open(QIODevice::ReadOnly)) {
            result.hasError = true;
            result.errorMsg = QStringLiteral("Failed to open image file: %1").arg(imagePath);
            return result;
        }
        
        const QByteArray imageData = imageFile.readAll();
        imageFile.close();
        
        if (imageData.isEmpty()) {
            result.hasError = true;
            result.errorMsg = QStringLiteral("Image file is empty: %1").arg(imagePath);
            return result;
        }
        
        // Convert to Base64
        const QString base64String = QString::fromLatin1(imageData.toBase64());
        
        // Detect MIME type from file extension
        QString mimeType = QStringLiteral("image/jpeg"); // default
        const QString lowerPath = imagePath.toLower();
        if (lowerPath.endsWith(QStringLiteral(".png"))) {
            mimeType = QStringLiteral("image/png");
        } else if (lowerPath.endsWith(QStringLiteral(".jpg")) || lowerPath.endsWith(QStringLiteral(".jpeg"))) {
            mimeType = QStringLiteral("image/jpeg");
        } else if (lowerPath.endsWith(QStringLiteral(".gif"))) {
            mimeType = QStringLiteral("image/gif");
        } else if (lowerPath.endsWith(QStringLiteral(".webp"))) {
            mimeType = QStringLiteral("image/webp");
        } else if (lowerPath.endsWith(QStringLiteral(".bmp"))) {
            mimeType = QStringLiteral("image/bmp");
        }
        
        // Create inline_data part per Gemini API schema
        QJsonObject inlineData;
        inlineData.insert(QStringLiteral("mime_type"), mimeType);
        inlineData.insert(QStringLiteral("data"), base64String);
        
        QJsonObject imagePart;
        imagePart.insert(QStringLiteral("inline_data"), inlineData);
        userParts.append(imagePart);
    }
    
    QJsonObject userContent;
    userContent.insert(QStringLiteral("parts"), userParts);
    contents.append(userContent);

    QJsonObject generationConfig;
    if (temperatureSupported) {
        generationConfig.insert(QStringLiteral("temperature"), temperature);
    }
    generationConfig.insert(QStringLiteral("maxOutputTokens"), maxTokens);

    QJsonObject root;
    root.insert(QStringLiteral("contents"), contents);
    root.insert(QStringLiteral("generationConfig"), generationConfig);
    if (!systemInstructionObj.isEmpty()) {
        // Gemini v1/v1beta supports top-level system_instruction when required
        root.insert(QStringLiteral("system_instruction"), systemInstructionObj);
    }

    const QByteArray jsonBytes = QJsonDocument(root).toJson(QJsonDocument::Compact);

    // Do NOT set Authorization header for Google; only Content-Type
    cpr::Header headers{
        {"Content-Type", "application/json"}
    };

    if (capsOpt.has_value()) {
        for (auto it = capsOpt->customHeaders.begin(); it != capsOpt->customHeaders.end(); ++it) {
            headers.insert({it.key().toStdString(), it.value().toStdString()});
        }
    }

    // Perform POST synchronously with explicit timeouts to avoid hanging indefinitely
    auto response = cpr::Post(
        cpr::Url{url},
        headers,
        cpr::Body{jsonBytes.constData()},
        cpr::ConnectTimeout{10000},   // 10s connect timeout
        cpr::Timeout{60000}           // 60s total request timeout
    );
    
    if (response.error) {
        result.hasError = true;

        if (response.error.code == cpr::ErrorCode::OPERATION_TIMEDOUT) {
            result.errorMsg = QStringLiteral("Google Gemini API Timeout");
            qWarning() << "GoogleBackend::sendPrompt timeout:" << QString::fromStdString(response.error.message);
        } else {
            const QString msg = QString::fromStdString(response.error.message);
            result.errorMsg = QStringLiteral("Google Gemini network error: %1").arg(msg);
            qWarning() << "GoogleBackend::sendPrompt network error:" << msg;
        }

        result.content = result.errorMsg;
        return result;
    }
    
    // Store raw response
    result.rawResponse = QString::fromStdString(response.text);
    
    if (response.status_code != 200) {
        result.hasError = true;

        qWarning() << "GoogleBackend::sendPrompt HTTP error" << response.status_code
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
    
    // Extract content from candidates[0].content.parts[0].text
    if (rootObj.contains(QStringLiteral("candidates")) && rootObj[QStringLiteral("candidates")].isArray()) {
        QJsonArray candidates = rootObj[QStringLiteral("candidates")].toArray();
        if (!candidates.isEmpty() && candidates[0].isObject()) {
            QJsonObject candidate = candidates[0].toObject();
            
            // Check finishReason before extracting content
            QString finishReason;
            if (candidate.contains(QStringLiteral("finishReason"))) {
                finishReason = candidate[QStringLiteral("finishReason")].toString();
            }
            
            // Map finishReason codes to error states
            if (finishReason == QStringLiteral("STOP")) {
                // Success case - proceed to extract content
            } else if (finishReason == QStringLiteral("MAX_TOKENS")) {
                result.hasError = true;
                result.errorMsg = QStringLiteral("Generation stopped: Max tokens limit reached.");
            } else if (finishReason == QStringLiteral("SAFETY")) {
                result.hasError = true;
                result.errorMsg = QStringLiteral("Generation blocked: Safety violation detected.");
            } else if (finishReason == QStringLiteral("RECITATION")) {
                result.hasError = true;
                result.errorMsg = QStringLiteral("Generation blocked: Recitation/Copyright violation.");
            } else if (finishReason == QStringLiteral("BLOCKLIST")) {
                result.hasError = true;
                result.errorMsg = QStringLiteral("Generation blocked: Content contains forbidden terms.");
            } else if (finishReason == QStringLiteral("PROHIBITED_CONTENT")) {
                result.hasError = true;
                result.errorMsg = QStringLiteral("Generation blocked: Prohibited content.");
            } else if (finishReason == QStringLiteral("SPII")) {
                result.hasError = true;
                result.errorMsg = QStringLiteral("Generation blocked: Sensitive Personally Identifiable Information detected.");
            } else if (finishReason == QStringLiteral("MALFORMED_FUNCTION_CALL")) {
                result.hasError = true;
                result.errorMsg = QStringLiteral("Generation stopped: Model generated an invalid function call.");
            } else if (finishReason == QStringLiteral("MODEL_ARMOR")) {
                result.hasError = true;
                result.errorMsg = QStringLiteral("Generation blocked: Model Armor intervention.");
            } else if (finishReason == QStringLiteral("FINISH_REASON_UNSPECIFIED") || finishReason == QStringLiteral("OTHER")) {
                result.hasError = true;
                result.errorMsg = QStringLiteral("Generation stopped: Unspecified or unknown reason.");
            } else if (!finishReason.isEmpty()) {
                // Default case for any other unexpected finishReason
                result.hasError = true;
                result.errorMsg = QStringLiteral("Generation stopped: Unknown reason (%1).").arg(finishReason);
            }
            
            // Extract content (may be partial for MAX_TOKENS or empty for other errors)
            if (candidate.contains(QStringLiteral("content")) && candidate[QStringLiteral("content")].isObject()) {
                QJsonObject content = candidate[QStringLiteral("content")].toObject();
                if (content.contains(QStringLiteral("parts")) && content[QStringLiteral("parts")].isArray()) {
                    QJsonArray parts = content[QStringLiteral("parts")].toArray();
                    if (!parts.isEmpty() && parts[0].isObject()) {
                        QJsonObject part = parts[0].toObject();
                        QString extractedText = part[QStringLiteral("text")].toString();
                        
                        if (result.hasError) {
                            // For error cases, append error message to any partial content
                            if (!extractedText.isEmpty()) {
                                result.content = extractedText + QStringLiteral("\n\n[ERROR] ") + result.errorMsg;
                            } else {
                                result.content = result.errorMsg;
                            }
                        } else {
                            // Success case (STOP)
                            result.content = extractedText;
                        }
                    }
                }
            } else if (result.hasError) {
                // No content available for error case
                result.content = result.errorMsg;
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

EmbeddingResult GoogleBackend::getEmbedding(
    const QString& apiKey,
    const QString& modelName,
    const QString& text
) {
    // Suppress unused parameter warnings
    (void)apiKey;
    (void)modelName;
    (void)text;
    
    EmbeddingResult result;
    result.hasError = true;
    result.errorMsg = QStringLiteral("Google embeddings not yet implemented");
    return result;
}

QFuture<QString> GoogleBackend::generateImage(
    const QString& prompt,
    const QString& model,
    const QString& size,
    const QString& quality,
    const QString& style
) {
    Q_UNUSED(prompt)
    Q_UNUSED(model)
    Q_UNUSED(size)
    Q_UNUSED(quality)
    Q_UNUSED(style)

    return QtConcurrent::run([]() -> QString {
        return QStringLiteral("Google image generation not implemented");
    });
}
