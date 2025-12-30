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
#include "../backends/AnthropicBackend.h"

#include <QMutexLocker>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QTextStream>
#include <QDebug>

LLMProviderRegistry& LLMProviderRegistry::instance() {
    // Thread-safe singleton using C++11 magic statics
    static LLMProviderRegistry instance;
    
    // Register concrete backends on first initialization
    static bool backendsRegistered = false;
    if (!backendsRegistered) {
        instance.registerBackend(std::make_shared<OpenAIBackend>());
        instance.registerBackend(std::make_shared<GoogleBackend>());
        instance.registerBackend(std::make_shared<AnthropicBackend>());
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

void LLMProviderRegistry::setAnthropicKey(const QString& key) {
    QMutexLocker locker(&m_mutex);
    m_anthropicApiKey = key;
}

bool LLMProviderRegistry::saveCredentials(const QMap<QString, QString>& credentials) {
    // Determine target path (canonical)
#if defined(Q_OS_MAC)
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
#else
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
#endif

    if (baseDir.isEmpty()) {
        qWarning() << "LLMProviderRegistry::saveCredentials: Base directory unavailable.";
        return false;
    }

    const QString filePath = QDir(baseDir).filePath(QStringLiteral("CognitivePipelines/accounts.json"));

    // Ensure parent directory exists
    const QFileInfo fi(filePath);
    QDir().mkpath(fi.dir().absolutePath());

    // Load existing JSON to preserve other fields if any
    QJsonObject root;
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject()) {
            root = doc.object();
        }
        file.close();
    }

    // Update accounts array
    QJsonArray accounts = root.value(QStringLiteral("accounts")).toArray();

    for (auto it = credentials.begin(); it != credentials.end(); ++it) {
        const QString providerId = it.key();
        const QString key = it.value();

        bool found = false;
        for (int i = 0; i < accounts.size(); ++i) {
            QJsonObject acc = accounts[i].toObject();
            if (acc.value(QStringLiteral("name")).toString().compare(providerId, Qt::CaseInsensitive) == 0) {
                acc.insert(QStringLiteral("api_key"), key);
                accounts.replace(i, acc);
                found = true;
                break;
            }
        }

        if (!found) {
            QJsonObject acc;
            acc.insert(QStringLiteral("name"), providerId);
            acc.insert(QStringLiteral("api_key"), key);
            accounts.append(acc);
        }
    }

    root.insert(QStringLiteral("accounts"), accounts);

    // Atomic save
    QSaveFile sf(filePath);
    if (!sf.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "LLMProviderRegistry::saveCredentials: Could not open for writing:" << filePath;
        return false;
    }

    sf.write(QJsonDocument(root).toJson());
    if (!sf.commit()) {
        qWarning() << "LLMProviderRegistry::saveCredentials: Failed to finalize save:" << filePath;
        return false;
    }

#ifdef Q_OS_UNIX
    QFile::setPermissions(filePath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
#endif

    // Also update in-memory Anthropic key if present
    if (credentials.contains(QStringLiteral("anthropic"))) {
        setAnthropicKey(credentials.value(QStringLiteral("anthropic")));
    }

    return true;
}

QString LLMProviderRegistry::getCredential(const QString& providerId) {
    // First, check for environment variables (preferred for CI/CD and testing)
    const auto readEnv = [](std::initializer_list<const char*> names) -> QString {
        for (const char* name : names) {
            const QByteArray val = qgetenv(name);
            if (!val.isEmpty()) {
                return QString::fromUtf8(val);
            }
        }
        return {};
    };

    if (providerId.compare(QStringLiteral("openai"), Qt::CaseInsensitive) == 0) {
        const QString envKey = readEnv({ "OPENAI_API_KEY" });
        if (!envKey.isEmpty()) {
            return envKey;
        }
    } else if (providerId.compare(QStringLiteral("google"), Qt::CaseInsensitive) == 0) {
        const QString envKey = readEnv({ "GOOGLE_API_KEY", "GOOGLE_GENAI_API_KEY", "GOOGLE_AI_API_KEY" });
        if (!envKey.isEmpty()) {
            return envKey;
        }
    } else if (providerId.compare(QStringLiteral("anthropic"), Qt::CaseInsensitive) == 0) {
        const QString envKey = readEnv({ "ANTHROPIC_API_KEY" });
        if (!envKey.isEmpty()) {
            return envKey;
        }

        QMutexLocker locker(&m_mutex);
        if (!m_anthropicApiKey.isEmpty()) {
            return m_anthropicApiKey;
        }
    }

    // Fall back to accounts.json file
    // Reuse the same path logic as LLMConnector::defaultAccountsFilePath()
    // to ensure consistency across the application, but also scan a few
    // likely locations used in CI (current working dir / repo root).

    QStringList candidatePaths;

#if defined(Q_OS_MAC)
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation); // Application Support
#else
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
#endif

    if (!baseDir.isEmpty()) {
        candidatePaths << QDir(baseDir).filePath(QStringLiteral("CognitivePipelines/accounts.json"));
    } else {
        qWarning() << "LLMProviderRegistry::getCredential: Base directory unavailable (QStandardPaths returned empty).";
    }

    candidatePaths << QDir::current().filePath(QStringLiteral("accounts.json"));

    const QString appDir = QCoreApplication::applicationDirPath();
    if (!appDir.isEmpty()) {
        const QDir dir(appDir);
        candidatePaths << dir.filePath(QStringLiteral("accounts.json"));
        candidatePaths << dir.filePath(QStringLiteral("../accounts.json"));
        candidatePaths << dir.filePath(QStringLiteral("../../accounts.json"));
    }

    candidatePaths.removeDuplicates();

    for (const QString& path : candidatePaths) {
        QFile f(path);
        if (!f.exists()) {
            continue;
        }

        if (!f.open(QIODevice::ReadOnly)) {
            qWarning() << "LLMProviderRegistry::getCredential: Failed to open" << path << ":" << f.errorString();
            continue;
        }

        const QByteArray data = f.readAll();
        f.close();

        const QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            qWarning() << "LLMProviderRegistry::getCredential: Invalid JSON in" << path;
            continue;
        }

        const QJsonObject root = doc.object();
        const QJsonArray accounts = root.value(QStringLiteral("accounts")).toArray();
        for (const QJsonValue& v : accounts) {
            const QJsonObject acc = v.toObject();
            const QString name = acc.value(QStringLiteral("name")).toString();

            if (name.compare(providerId, Qt::CaseInsensitive) == 0) {
                const QString key = acc.value(QStringLiteral("api_key")).toString();
                if (!key.isEmpty()) {
                    return key;
                }
            }
        }
    }

    return {};
}
