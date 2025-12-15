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

#include <cpr/cpr.h>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QtConcurrent>
#include <QUuid>

QString OpenAIBackend::id() const {
    return QStringLiteral("openai");
}

QString OpenAIBackend::name() const {
    return QStringLiteral("OpenAI");
}

QStringList OpenAIBackend::availableModels() const {
    return {
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

QStringList OpenAIBackend::availableEmbeddingModels() const {
    return {
        QStringLiteral("text-embedding-3-small"),
        QStringLiteral("text-embedding-3-large"),
        QStringLiteral("text-embedding-ada-002")
    };
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
    
    const std::string url = "https://api.openai.com/v1/chat/completions";

    // Build messages array [{role: system, content: ...}, {role: user, content: ...}]
    QJsonObject sysMsg;
    sysMsg.insert(QStringLiteral("role"), QStringLiteral("system"));
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
    root.insert(QStringLiteral("model"), modelName);
    root.insert(QStringLiteral("temperature"), temperature);
    root.insert(QStringLiteral("max_completion_tokens"), maxTokens);
    root.insert(QStringLiteral("messages"), messages);

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
        cpr::Timeout{60000}           // 60s total request timeout
    );
    
    if (response.error) {
        result.hasError = true;

        if (response.error.code == cpr::ErrorCode::OPERATION_TIMEDOUT) {
            result.errorMsg = QStringLiteral("OpenAI API Timeout");
            qWarning() << "OpenAIBackend::sendPrompt timeout:" << QString::fromStdString(response.error.message);
        } else {
            const QString msg = QString::fromStdString(response.error.message);
            result.errorMsg = QStringLiteral("OpenAI network error: %1").arg(msg);
            qWarning() << "OpenAIBackend::sendPrompt network error:" << msg;
        }

        result.content = result.errorMsg;
        return result;
    }
    
    // Store raw response
    result.rawResponse = QString::fromStdString(response.text);
    
    if (response.status_code != 200) {
        result.hasError = true;

        qWarning() << "OpenAIBackend::sendPrompt HTTP error" << response.status_code
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

    // Build request payload
    QJsonObject root;
    root.insert(QStringLiteral("input"), text);
    root.insert(QStringLiteral("model"), modelName);

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
        cpr::Timeout{60000}           // 60s total request timeout
    );
    
    if (response.error) {
        result.hasError = true;

        if (response.error.code == cpr::ErrorCode::OPERATION_TIMEDOUT) {
            result.errorMsg = QStringLiteral("OpenAI API Timeout");
            qWarning() << "OpenAIBackend::getEmbedding timeout:" << QString::fromStdString(response.error.message);
        } else {
            const QString msg = QString::fromStdString(response.error.message);
            result.errorMsg = QStringLiteral("OpenAI network error: %1").arg(msg);
            qWarning() << "OpenAIBackend::getEmbedding network error:" << msg;
        }

        return result;
    }
    
    if (response.status_code != 200) {
        result.hasError = true;

        qWarning() << "OpenAIBackend::getEmbedding HTTP error" << response.status_code
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
    const QString& style
) {
    return QtConcurrent::run([prompt, model, size, quality, style]() -> QString {
        const QString apiKey = LLMProviderRegistry::instance().getCredential(QStringLiteral("openai"));
        if (apiKey.trimmed().isEmpty()) {
            const QString msg = QStringLiteral("Missing OpenAI API key");
            qWarning() << "OpenAIBackend::generateImage" << msg;
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
                qWarning() << "OpenAIBackend::generateImage timeout:" << QString::fromStdString(response.error.message);
            } else {
                errorMsg = QString::fromStdString(response.error.message);
                qWarning() << "OpenAIBackend::generateImage network error:" << errorMsg;
                errorMsg = QStringLiteral("OpenAI network error: %1").arg(errorMsg);
            }

            return errorMsg;
        }

        const QString rawResponse = QString::fromStdString(response.text);

        if (response.status_code != 200) {
            QString errorMsg;

            qWarning() << "OpenAIBackend::generateImage HTTP error" << response.status_code
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
            qWarning() << "OpenAIBackend::generateImage missing b64_json field" << rawResponse;
            return QStringLiteral("OpenAI image response missing data");
        }

        const QByteArray imageData = QByteArray::fromBase64(b64Image.toUtf8());
        if (imageData.isEmpty()) {
            return QStringLiteral("Failed to decode image data");
        }

        QString cacheBase = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        if (cacheBase.isEmpty()) {
            cacheBase = QDir::tempPath();
        }

        QDir cacheDir(cacheBase);
        if (!cacheDir.mkpath(QStringLiteral("generated_images"))) {
            qWarning() << "OpenAIBackend::generateImage failed to create cache dir" << cacheBase;
            return QStringLiteral("Failed to create cache directory");
        }

        QDir imagesDir(cacheDir.filePath(QStringLiteral("generated_images")));
        const QString fileName = QStringLiteral("img_%1.png").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces));
        const QString filePath = imagesDir.filePath(fileName);

        QFile outFile(filePath);
        if (!outFile.open(QIODevice::WriteOnly)) {
            qWarning() << "OpenAIBackend::generateImage failed to open file" << filePath
                       << "error" << outFile.errorString();
            return QStringLiteral("Failed to write generated image");
        }

        const qint64 bytesWritten = outFile.write(imageData);
        outFile.close();

        if (bytesWritten != imageData.size()) {
            qWarning() << "OpenAIBackend::generateImage incomplete write" << filePath;
            return QStringLiteral("Failed to save generated image");
        }

        return filePath;
    });
}
