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

#include "LLMProviderRegistry.h"
#include "../backends/ILLMBackend.h"
#include "../backends/OpenAIBackend.h"
#include "../backends/GoogleBackend.h"

#include <QMutexLocker>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

LLMProviderRegistry& LLMProviderRegistry::instance() {
    // Thread-safe singleton using C++11 magic statics
    static LLMProviderRegistry instance;
    
    // Register concrete backends on first initialization
    static bool backendsRegistered = false;
    if (!backendsRegistered) {
        instance.registerBackend(std::make_shared<OpenAIBackend>());
        instance.registerBackend(std::make_shared<GoogleBackend>());
        backendsRegistered = true;
    }
    
    return instance;
}

void LLMProviderRegistry::registerBackend(std::shared_ptr<ILLMBackend> backend) {
    if (!backend) {
        qWarning() << "LLMProviderRegistry::registerBackend: Attempted to register null backend";
        return;
    }

    QMutexLocker locker(&m_mutex);
    const QString id = backend->id();
    if (m_backends.contains(id)) {
        qWarning() << "LLMProviderRegistry::registerBackend: Backend with id" << id << "already registered. Replacing.";
    }
    m_backends[id] = backend;
}

ILLMBackend* LLMProviderRegistry::getBackend(const QString& id) {
    QMutexLocker locker(&m_mutex);
    auto it = m_backends.find(id);
    if (it != m_backends.end()) {
        return it.value().get();
    }
    return nullptr;
}

QList<ILLMBackend*> LLMProviderRegistry::allBackends() {
    QMutexLocker locker(&m_mutex);
    QList<ILLMBackend*> result;
    result.reserve(m_backends.size());
    for (const auto& backend : m_backends) {
        result.append(backend.get());
    }
    return result;
}

QString LLMProviderRegistry::getCredential(const QString& providerId) {
    // First, check for environment variables (preferred for CI/CD and testing)
    if (providerId.compare(QStringLiteral("openai"), Qt::CaseInsensitive) == 0) {
        const QByteArray envKey = qgetenv("OPENAI_API_KEY");
        if (!envKey.isEmpty()) {
            return QString::fromUtf8(envKey);
        }
    } else if (providerId.compare(QStringLiteral("google"), Qt::CaseInsensitive) == 0) {
        const QByteArray envKey = qgetenv("GOOGLE_API_KEY");
        if (!envKey.isEmpty()) {
            return QString::fromUtf8(envKey);
        }
    }

    // Fall back to accounts.json file
    // Reuse the same path logic as LLMConnector::defaultAccountsFilePath()
    // to ensure consistency across the application.

#if defined(Q_OS_MAC)
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation); // Application Support
#else
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
#endif

    if (baseDir.isEmpty()) {
        qWarning() << "LLMProviderRegistry::getCredential: Base directory unavailable (QStandardPaths returned empty).";
        return {};
    }

    const QString path = QDir(baseDir).filePath(QStringLiteral("CognitivePipelines/accounts.json"));

    QFile f(path);
    if (!f.exists()) {
        return {};
    }

    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "LLMProviderRegistry::getCredential: Failed to open" << path << ":" << f.errorString();
        return {};
    }

    const QByteArray data = f.readAll();
    f.close();

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        qWarning() << "LLMProviderRegistry::getCredential: Invalid JSON in" << path;
        return {};
    }

    const QJsonObject root = doc.object();

    // Search for the provider in the accounts array
    // Format: { "accounts": [ { "name": "openai", "api_key": "..." }, ... ] }
    const QJsonArray accounts = root.value(QStringLiteral("accounts")).toArray();
    for (const QJsonValue& v : accounts) {
        const QJsonObject acc = v.toObject();
        const QString name = acc.value(QStringLiteral("name")).toString();
        
        // Match the provider ID (case-insensitive for flexibility)
        if (name.compare(providerId, Qt::CaseInsensitive) == 0) {
            const QString key = acc.value(QStringLiteral("api_key")).toString();
            if (!key.isEmpty()) {
                return key;
            }
        }
    }

    return {};
}
