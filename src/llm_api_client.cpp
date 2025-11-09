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
#include "llm_api_client.h"

#include <cpr/cpr.h>
#include <sstream>

// Qt
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDebug>
#include "LLMConnector.h"

namespace {
// naive string search helper
static size_t find_after(const std::string& s, const std::string& needle, size_t pos = 0) {
    auto p = s.find(needle, pos);
    return p == std::string::npos ? std::string::npos : p + needle.size();
}
}

// New provider-aware API using robust QJson request building
QString LlmApiClient::sendPrompt(ApiProvider provider,
                              const QString &apiKey,
                              const QString &model,
                              double temperature,
                              int maxTokens,
                              const QString &systemPrompt,
                              const QString &userPrompt) {
    switch (provider) {
        case ApiProvider::OpenAI: {
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
            root.insert(QStringLiteral("model"), model);
            root.insert(QStringLiteral("temperature"), temperature);
            root.insert(QStringLiteral("max_tokens"), maxTokens);
            root.insert(QStringLiteral("messages"), messages);

            const QByteArray jsonBytes = QJsonDocument(root).toJson(QJsonDocument::Compact);

            cpr::Header headers{
                {"Authorization", std::string("Bearer ") + apiKey.toStdString()},
                {"Content-Type", "application/json"}
            };

            // Perform POST synchronously and return raw body on success or error to allow caller to parse JSON errors
            auto response = cpr::Post(cpr::Url{url}, headers, cpr::Body{jsonBytes.constData()}, cpr::Timeout{60000});
            if (response.error) {
                return QStringLiteral("Network error: %1").arg(QString::fromStdString(response.error.message));
            }
            if (response.status_code != 200) {
                // Return raw error body if available so the caller can parse {"error":{...}}
                if (!response.text.empty()) {
                    return QString::fromStdString(response.text);
                }
                return QStringLiteral("HTTP %1").arg(response.status_code);
            }
            // Success: return raw JSON body for the caller to parse
            return QString::fromStdString(response.text);
        }
        case ApiProvider::Google: {
            qWarning("Google provider not yet implemented");
            return QStringLiteral("Google provider not yet implemented");
        }
    }
    return QString();
}

std::string LlmApiClient::sendPrompt(const std::string& apiKey, const std::string& promptText) {
    // Endpoint: OpenAI-compatible chat completions
    const std::string url = "https://api.openai.com/v1/chat/completions";

    // Build JSON payload using Qt JSON (remove hand-rolled string construction)
    QJsonObject userMsg;
    userMsg.insert(QStringLiteral("role"), QStringLiteral("user"));
    userMsg.insert(QStringLiteral("content"), QString::fromStdString(promptText));

    QJsonArray messages;
    messages.append(userMsg);

    QJsonObject root;
    root.insert(QStringLiteral("model"), QStringLiteral("gpt-4o-mini"));
    root.insert(QStringLiteral("messages"), messages);

    const QByteArray jsonBytes = QJsonDocument(root).toJson(QJsonDocument::Compact);

    // Headers
    cpr::Header headers{
        {"Authorization", std::string("Bearer ") + apiKey},
        {"Content-Type", "application/json"}
    };

    // Perform POST request
    auto response = cpr::Post(cpr::Url{url}, headers, cpr::Body{jsonBytes.constData()}, cpr::Timeout{60000});

    if (response.error) {
        std::ostringstream err;
        err << "Network error: " << response.error.message;
        return err.str();
    }

    if (response.status_code != 200) {
        std::ostringstream err;
        err << "HTTP " << response.status_code << ": "
            << (response.text.size() > 512 ? response.text.substr(0, 512) + "..." : response.text);
        return err.str();
    }

    // Extract first choice.message.content from JSON body (keep brittle parsing for now)
    const std::string content = extractFirstMessageContent(response.text);
    if (content.empty()) {
        return "Failed to parse response: message content not found.";
    }
    return content;
}

std::string LlmApiClient::extractFirstMessageContent(const std::string& jsonBody) {
    // This is a very simplified extractor tailored to typical OpenAI responses.
    // It is not a full JSON parser.
    // We search in order: "choices" -> first object -> "message" -> "content"

    // Find "choices"
    size_t pos = jsonBody.find("\"choices\"");
    if (pos == std::string::npos) return {};

    // Find first opening bracket [ after choices
    pos = jsonBody.find('[', pos);
    if (pos == std::string::npos) return {};

    // Find first opening brace { after [
    pos = jsonBody.find('{', pos);
    if (pos == std::string::npos) return {};

    // Find "message"
    pos = jsonBody.find("\"message\"", pos);
    if (pos == std::string::npos) return {};

    // Find "content"
    pos = jsonBody.find("\"content\"", pos);
    if (pos == std::string::npos) return {};

    // Find colon then opening quote
    pos = jsonBody.find(':', pos);
    if (pos == std::string::npos) return {};

    // Skip whitespace
    while (pos < jsonBody.size() && (jsonBody[pos] == ':' || jsonBody[pos] == ' ' || jsonBody[pos] == '\t' || jsonBody[pos] == '\n' || jsonBody[pos] == '\r')) {
        ++pos;
    }
    if (pos >= jsonBody.size() || jsonBody[pos] != '"') return {};

    // Capture until next unescaped quote
    ++pos; // move past opening quote
    std::string value;
    bool escape = false;
    for (; pos < jsonBody.size(); ++pos) {
        char c = jsonBody[pos];
        if (escape) {
            switch (c) {
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                default: value.push_back(c); break;
            }
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else if (c == '"') {
            break; // end of string
        } else {
            value.push_back(c);
        }
    }

    return value;
}


QString LlmApiClient::getApiKey(const QString &providerKey) const {
    // 1) Environment variable takes precedence
    const QByteArray envKey = qgetenv("OPENAI_API_KEY");
    if (!envKey.isEmpty()) return QString::fromUtf8(envKey);

    // 2) Single canonical location shared with the app/tests
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

    // 2a) Direct key lookup at the root, e.g., { "openai_api_key": "..." }
    if (root.contains(providerKey)) {
        const QString key = root.value(providerKey).toString();
        if (!key.isEmpty()) return key;
    }

    // 2b) accounts[] shape: { accounts: [{ name: "<provider>", api_key: "..." }] }
    const QJsonArray accounts = root.value(QStringLiteral("accounts")).toArray();
    if (!accounts.isEmpty()) {
        for (const QJsonValue &v : accounts) {
            const QJsonObject acc = v.toObject();
            const QString name = acc.value(QStringLiteral("name")).toString();
            if (name == providerKey) {
                const QString key = acc.value(QStringLiteral("api_key")).toString();
                if (!key.isEmpty()) return key;
            }
        }
    }

    qWarning() << "API key not found in file at:" << path << "(checked keys '" << providerKey << "' and accounts[].api_key)";
    return {};
}
