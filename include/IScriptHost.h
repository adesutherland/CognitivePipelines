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

#include "CommonDataTypes.h"
#include <QString>
#include <QVariant>
#include <functional>
#include <memory>
#include <map>
#include <vector>

/**
 * @brief Interface for the host environment that a script engine interacts with.
 * Provides callbacks for I/O, logging, and error reporting.
 */
class IScriptHost {
public:
    virtual ~IScriptHost() = default;

    /**
     * @brief Logs a message to the UI console or host logs.
     */
    virtual void log(const QString& message) = 0;

    /**
     * @brief Retrieves input data by key from the host environment.
     */
    virtual QVariant getInput(const QString& key) = 0;

    /**
     * @brief Sets output data for downstream nodes.
     */
    virtual void setOutput(const QString& key, const QVariant& value) = 0;

    /**
     * @brief Signals a runtime error during script execution.
     */
    virtual void setError(const QString& message) = 0;
};

/**
 * @brief Interface for a script engine implementation (e.g., QuickJS, Python).
 */
class IScriptEngine {
public:
    virtual ~IScriptEngine() = default;

    /**
     * @brief Executes the given script within the provided host context.
     * @return true if execution was successful, false otherwise.
     */
    virtual bool execute(const QString& script, IScriptHost* host) = 0;

    /**
     * @brief Returns a unique identifier for this engine (e.g., "quickjs", "python").
     */
    virtual QString getEngineId() const = 0;
};

/**
 * @brief Factory type for creating script engine instances.
 */
using ScriptEngineFactory = std::function<std::unique_ptr<IScriptEngine>()>;

/**
 * @brief Singleton registry for managing available script engines.
 * Supports static registration during application startup.
 */
class ScriptEngineRegistry {
public:
    static ScriptEngineRegistry& instance() {
        static ScriptEngineRegistry registry;
        return registry;
    }

    /**
     * @brief Registers a new script engine factory.
     */
    void registerEngine(const QString& id, ScriptEngineFactory factory) {
        m_factories[id] = std::move(factory);
    }

    /**
     * @brief Creates an instance of a registered script engine.
     * @return std::unique_ptr<IScriptEngine> or nullptr if id is not found.
     */
    std::unique_ptr<IScriptEngine> createEngine(const QString& id) const {
        auto it = m_factories.find(id);
        if (it != m_factories.end()) {
            return it->second();
        }
        return nullptr;
    }

    /**
     * @brief Returns a list of all registered engine IDs.
     */
    std::vector<QString> registeredEngineIds() const {
        std::vector<QString> ids;
        for (const auto& pair : m_factories) {
            ids.push_back(pair.first);
        }
        return ids;
    }

private:
    ScriptEngineRegistry() = default;
    std::map<QString, ScriptEngineFactory> m_factories;
};
