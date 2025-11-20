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
#pragma once

#include <QString>
#include <QList>
#include <QMap>
#include <QMutex>
#include <memory>

class ILLMBackend;

/**
 * @brief Thread-safe Singleton registry for managing LLM backend providers.
 *
 * This class implements the Registry pattern, providing centralized management
 * of all available LLM backends (OpenAI, Google, Anthropic, etc.) and their
 * credentials.
 */
class LLMProviderRegistry {
public:
    /**
     * @brief Returns the singleton instance of the registry.
     * @return Reference to the singleton instance (thread-safe).
     */
    static LLMProviderRegistry& instance();

    // Delete copy and move constructors/operators (singleton pattern)
    LLMProviderRegistry(const LLMProviderRegistry&) = delete;
    LLMProviderRegistry& operator=(const LLMProviderRegistry&) = delete;
    LLMProviderRegistry(LLMProviderRegistry&&) = delete;
    LLMProviderRegistry& operator=(LLMProviderRegistry&&) = delete;

    /**
     * @brief Registers a new backend provider with the registry.
     * @param backend Shared pointer to the backend implementation.
     */
    void registerBackend(std::shared_ptr<ILLMBackend> backend);

    /**
     * @brief Retrieves a backend by its unique ID.
     * @param id The backend's unique identifier (e.g., "openai", "google").
     * @return Pointer to the backend, or nullptr if not found.
     */
    ILLMBackend* getBackend(const QString& id);

    /**
     * @brief Returns all registered backends.
     * @return List of pointers to all registered backends.
     */
    QList<ILLMBackend*> allBackends();

    /**
     * @brief Retrieves the API key for a given provider from accounts.json.
     *
     * This method reads the accounts.json file using the same path logic as
     * LLMConnector::defaultAccountsFilePath() and searches for an account
     * with a matching provider ID.
     *
     * @param providerId The provider's unique identifier (e.g., "openai", "google").
     * @return The API key if found, or an empty QString if not found.
     */
    QString getCredential(const QString& providerId);

private:
    LLMProviderRegistry() = default;
    ~LLMProviderRegistry() = default;

    QMap<QString, std::shared_ptr<ILLMBackend>> m_backends;
    QMutex m_mutex;
};
