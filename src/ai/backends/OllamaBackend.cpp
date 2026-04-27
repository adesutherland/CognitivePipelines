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
#include "OllamaBackend.h"

#include <cpr/cpr.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QtConcurrent>

#include "ModelCapsRegistry.h"
#include "Logger.h"
#include "LoggingCategories.h"
#include "ai/registry/LLMProviderRegistry.h"

namespace {

QString responseErrorMessage(const cpr::Response& response)
{
    if (!response.error.message.empty()) {
        return QString::fromStdString(response.error.message);
    }

    const QByteArray body = QByteArray::fromStdString(response.text);
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
        const QString error = doc.object().value(QStringLiteral("error")).toString();
        if (!error.isEmpty()) {
            return error;
        }
    }

    return QStringLiteral("HTTP %1").arg(response.status_code);
}

std::vector<float> vectorFromJsonArray(const QJsonArray& values)
{
    std::vector<float> vector;
    vector.reserve(static_cast<size_t>(values.size()));
    for (const QJsonValue& value : values) {
        if (value.isDouble()) {
            vector.push_back(static_cast<float>(value.toDouble()));
        }
    }
    return vector;
}

cpr::Header ollamaHeaders(const QString& apiKey, bool includeContentType)
{
    cpr::Header headers{{"Accept", "application/json"}};
    if (includeContentType) {
        headers.insert({"Content-Type", "application/json"});
    }

    if (const auto settings = ModelCapsRegistry::instance().providerSettings(QStringLiteral("ollama"))) {
        for (auto it = settings->headers.constBegin(); it != settings->headers.constEnd(); ++it) {
            headers[ it.key().toStdString() ] = it.value().toStdString();
        }
    }

    const QString effectiveKey = apiKey.trimmed().isEmpty()
                                     ? LLMProviderRegistry::instance().getCredential(QStringLiteral("ollama")).trimmed()
                                     : apiKey.trimmed();
    if (!effectiveKey.isEmpty() && headers.find("Authorization") == headers.end()) {
        headers.insert({"Authorization", std::string("Bearer ") + effectiveKey.toStdString()});
    }

    return headers;
}

} // namespace

OllamaBackend::OllamaBackend()
{
    m_cachedModels = {
        QStringLiteral("llama3.2"),
        QStringLiteral("qwen2.5"),
        QStringLiteral("mistral")
    };
}

QString OllamaBackend::id() const
{
    return QStringLiteral("ollama");
}

QString OllamaBackend::name() const
{
    return QStringLiteral("Ollama (Local)");
}

QStringList OllamaBackend::availableModels() const
{
    QMutexLocker locker(&m_cacheMutex);
    return m_cachedModels;
}

QStringList OllamaBackend::availableEmbeddingModels() const
{
    return {
        QStringLiteral("nomic-embed-text"),
        QStringLiteral("mxbai-embed-large")
    };
}

QString OllamaBackend::baseUrl() const
{
    const QByteArray envUrl = qgetenv("OLLAMA_BASE_URL");
    if (!envUrl.isEmpty()) {
        QString value = QString::fromUtf8(envUrl).trimmed();
        while (value.endsWith(QLatin1Char('/'))) {
            value.chop(1);
        }
        if (!value.isEmpty()) {
            return value;
        }
    }

    if (const auto settings = ModelCapsRegistry::instance().providerSettings(QStringLiteral("ollama"))) {
        QString value = settings->baseUrl.trimmed();
        while (value.endsWith(QLatin1Char('/'))) {
            value.chop(1);
        }
        if (!value.isEmpty()) {
            return value;
        }
    }

    return QStringLiteral("http://127.0.0.1:11434");
}

QFuture<QStringList> OllamaBackend::fetchModelList()
{
    return QtConcurrent::run([this]() -> QStringList {
        const QString url = baseUrl() + QStringLiteral("/api/tags");

        const auto response = cpr::Get(
            cpr::Url{url.toStdString()},
            ollamaHeaders(QString(), false),
            cpr::ConnectTimeout{2000},
            cpr::Timeout{10000}
        );

        if (response.error) {
            CP_WARN.noquote() << QStringLiteral("OllamaBackend::fetchModelList failure provider=ollama transport=network message=%1")
                                          .arg(QString::fromStdString(response.error.message));
            return availableModels();
        }

        if (response.status_code != 200) {
            CP_WARN.noquote() << QStringLiteral("OllamaBackend::fetchModelList failure provider=ollama status=%1 message=%2")
                                          .arg(response.status_code)
                                          .arg(responseErrorMessage(response));
            return availableModels();
        }

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.text), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            CP_WARN << "OllamaBackend::fetchModelList: JSON parse error:" << parseError.errorString();
            return availableModels();
        }

        QStringList models;
        const QJsonArray modelArray = doc.object().value(QStringLiteral("models")).toArray();
        models.reserve(modelArray.size());
        for (const QJsonValue& value : modelArray) {
            const QString name = value.toObject().value(QStringLiteral("name")).toString();
            if (!name.trimmed().isEmpty()) {
                models.append(name);
            }
        }

        if (models.isEmpty()) {
            return availableModels();
        }

        models.removeDuplicates();
        std::sort(models.begin(), models.end(), [](const QString& lhs, const QString& rhs) {
            return lhs.localeAwareCompare(rhs) < 0;
        });

        {
            QMutexLocker locker(&m_cacheMutex);
            m_cachedModels = models;
        }

        CP_CLOG(cp_discovery).noquote() << QStringLiteral("Ollama discovered model count: %1").arg(models.size());
        return models;
    });
}

QFuture<QStringList> OllamaBackend::fetchRawModelList()
{
    return fetchModelList();
}

LLMResult OllamaBackend::sendPrompt(
    const QString& apiKey,
    const QString& modelName,
    double temperature,
    int maxTokens,
    const QString& systemPrompt,
    const QString& userPrompt,
    const LLMMessage& message
) {
    LLMResult result;

    if (!message.attachments.isEmpty()) {
        result.hasError = true;
        result.errorMsg = QStringLiteral("Ollama backend does not support attachments yet.");
        result.content = result.errorMsg;
        return result;
    }

    const QString selectedModel = modelName.trimmed().isEmpty()
                                      ? availableModels().value(0, QStringLiteral("llama3.2"))
                                      : modelName.trimmed();

    QJsonArray messages;
    if (!systemPrompt.trimmed().isEmpty()) {
        QJsonObject systemMessage;
        systemMessage.insert(QStringLiteral("role"), QStringLiteral("system"));
        systemMessage.insert(QStringLiteral("content"), systemPrompt);
        messages.append(systemMessage);
    }

    QJsonObject userMessage;
    userMessage.insert(QStringLiteral("role"), QStringLiteral("user"));
    userMessage.insert(QStringLiteral("content"), userPrompt);
    messages.append(userMessage);

    QJsonObject options;
    options.insert(QStringLiteral("temperature"), temperature);
    if (maxTokens > 0) {
        options.insert(QStringLiteral("num_predict"), maxTokens);
    }

    QJsonObject root;
    root.insert(QStringLiteral("model"), selectedModel);
    root.insert(QStringLiteral("stream"), false);
    root.insert(QStringLiteral("messages"), messages);
    root.insert(QStringLiteral("options"), options);

    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Compact);
    const QString url = baseUrl() + QStringLiteral("/api/chat");

    CP_CLOG(cp_lifecycle).noquote() << "[ModelLifecycle] OllamaBackend::sendPrompt using model=" << selectedModel;

    const auto response = cpr::Post(
        cpr::Url{url.toStdString()},
        ollamaHeaders(apiKey, true),
        cpr::Body{payload.constData()},
        cpr::ConnectTimeout{5000},
        cpr::Timeout{120000}
    );

    if (response.error) {
        result.hasError = true;
        result.errorMsg = QStringLiteral("Ollama network error: %1")
                              .arg(QString::fromStdString(response.error.message));
        result.content = result.errorMsg;
        CP_WARN.noquote() << QStringLiteral("OllamaBackend::sendPrompt failure provider=ollama model=%1 transport=network message=%2")
                                      .arg(selectedModel, result.errorMsg);
        return result;
    }

    result.rawResponse = QString::fromStdString(response.text);
    if (response.status_code != 200) {
        result.hasError = true;
        result.errorMsg = responseErrorMessage(response);
        result.content = result.errorMsg;
        CP_WARN.noquote() << QStringLiteral("OllamaBackend::sendPrompt failure provider=ollama model=%1 status=%2 message=%3")
                                      .arg(selectedModel)
                                      .arg(response.status_code)
                                      .arg(result.errorMsg);
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.text), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        result.hasError = true;
        result.errorMsg = QStringLiteral("Ollama JSON parse error: %1").arg(parseError.errorString());
        result.content = result.errorMsg;
        return result;
    }

    const QJsonObject obj = doc.object();
    if (obj.contains(QStringLiteral("error"))) {
        result.hasError = true;
        result.errorMsg = obj.value(QStringLiteral("error")).toString(QStringLiteral("Unknown Ollama error"));
        result.content = result.errorMsg;
        return result;
    }

    result.content = obj.value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toString();
    result.usage.inputTokens = obj.value(QStringLiteral("prompt_eval_count")).toInt(0);
    result.usage.outputTokens = obj.value(QStringLiteral("eval_count")).toInt(0);
    result.usage.totalTokens = result.usage.inputTokens + result.usage.outputTokens;
    return result;
}

EmbeddingResult OllamaBackend::getEmbedding(
    const QString& apiKey,
    const QString& modelName,
    const QString& text
) {
    EmbeddingResult result;
    const QString selectedModel = modelName.trimmed().isEmpty()
                                      ? QStringLiteral("nomic-embed-text")
                                      : modelName.trimmed();

    QJsonObject root;
    root.insert(QStringLiteral("model"), selectedModel);
    root.insert(QStringLiteral("input"), text);

    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Compact);
    const QString embedUrl = baseUrl() + QStringLiteral("/api/embed");

    auto response = cpr::Post(
        cpr::Url{embedUrl.toStdString()},
        ollamaHeaders(apiKey, true),
        cpr::Body{payload.constData()},
        cpr::ConnectTimeout{5000},
        cpr::Timeout{120000}
    );

    if (response.status_code == 404 && !response.error) {
        QJsonObject legacyRoot;
        legacyRoot.insert(QStringLiteral("model"), selectedModel);
        legacyRoot.insert(QStringLiteral("prompt"), text);
        const QByteArray legacyPayload = QJsonDocument(legacyRoot).toJson(QJsonDocument::Compact);
        const QString legacyUrl = baseUrl() + QStringLiteral("/api/embeddings");
        response = cpr::Post(
            cpr::Url{legacyUrl.toStdString()},
            ollamaHeaders(apiKey, true),
            cpr::Body{legacyPayload.constData()},
            cpr::ConnectTimeout{5000},
            cpr::Timeout{120000}
        );
    }

    if (response.error) {
        result.hasError = true;
        result.errorMsg = QStringLiteral("Ollama embedding network error: %1")
                              .arg(QString::fromStdString(response.error.message));
        CP_WARN.noquote() << QStringLiteral("OllamaBackend::getEmbedding failure provider=ollama model=%1 transport=network message=%2")
                                      .arg(selectedModel, result.errorMsg);
        return result;
    }

    if (response.status_code != 200) {
        result.hasError = true;
        result.errorMsg = responseErrorMessage(response);
        CP_WARN.noquote() << QStringLiteral("OllamaBackend::getEmbedding failure provider=ollama model=%1 status=%2 message=%3")
                                      .arg(selectedModel)
                                      .arg(response.status_code)
                                      .arg(result.errorMsg);
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.text), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        result.hasError = true;
        result.errorMsg = QStringLiteral("Ollama embedding JSON parse error: %1").arg(parseError.errorString());
        return result;
    }

    const QJsonObject obj = doc.object();
    if (obj.contains(QStringLiteral("error"))) {
        result.hasError = true;
        result.errorMsg = obj.value(QStringLiteral("error")).toString(QStringLiteral("Unknown Ollama embedding error"));
        return result;
    }

    if (obj.value(QStringLiteral("embeddings")).isArray()) {
        const QJsonArray embeddings = obj.value(QStringLiteral("embeddings")).toArray();
        if (!embeddings.isEmpty() && embeddings.first().isArray()) {
            result.vector = vectorFromJsonArray(embeddings.first().toArray());
        }
    } else if (obj.value(QStringLiteral("embedding")).isArray()) {
        result.vector = vectorFromJsonArray(obj.value(QStringLiteral("embedding")).toArray());
    }

    if (result.vector.empty()) {
        result.hasError = true;
        result.errorMsg = QStringLiteral("Ollama embedding response did not contain a vector");
    }

    return result;
}

QFuture<QString> OllamaBackend::generateImage(
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

    return QtConcurrent::run([]() -> QString {
        return QStringLiteral("Ollama image generation is not supported");
    });
}
