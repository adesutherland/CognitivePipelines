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
#include <QFile>
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
        QStringLiteral("gemini-3-pro-preview"),
        QStringLiteral("gemini-3-pro-image-preview"),
        QStringLiteral("gemini-2.5-pro"),
        QStringLiteral("gemini-2.5-flash"),
        QStringLiteral("gemini-2.5-flash-lite"),
        QStringLiteral("imagen-3")
    };
}

QStringList GoogleBackend::availableEmbeddingModels() const {
    return {
        QStringLiteral("text-embedding-004")
    };
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
    
    // Google Generative Language (Gemini) endpoint selection:
    // Preview models use v1beta, stable models use v1.
    // API key is passed as a query parameter, not via Authorization header.
    const bool isPreviewModel = modelName.contains(QStringLiteral("preview"), Qt::CaseInsensitive);
    const std::string apiVersion = isPreviewModel ? "v1beta" : "v1";
    const std::string url = std::string("https://generativelanguage.googleapis.com/")
                            + apiVersion
                            + "/models/"
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
        result.content = result.errorMsg;
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
