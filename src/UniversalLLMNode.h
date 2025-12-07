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

#include <QObject>
#include <QWidget>
#include <QString>

#include "IToolConnector.h"
#include "CommonDataTypes.h"

/**
 * @brief Universal LLM Node that delegates to backend strategies.
 *
 * This node replaces provider-specific logic with a single, flexible component
 * that connects the UniversalLLMPropertiesWidget (UI) to ILLMBackend strategies
 * managed by the LLMProviderRegistry.
 */
class UniversalLLMNode : public QObject, public IToolConnector {
    Q_OBJECT
    Q_INTERFACES(IToolConnector)

public:
    explicit UniversalLLMNode(QObject* parent = nullptr);
    ~UniversalLLMNode() override = default;

    // IToolConnector interface (V3 tokens API)
    NodeDescriptor getDescriptor() const override;
    QWidget* createConfigurationWidget(QWidget* parent) override;
    TokenList execute(const TokenList& incomingTokens) override;
    QJsonObject saveState() const override;
    void loadState(const QJsonObject& data) override;

    // Constants for pin IDs
    static constexpr const char* kInputSystemId = "system";
    static constexpr const char* kInputPromptId = "prompt";
    static constexpr const char* kInputImageId = "image";
    static constexpr const char* kOutputResponseId = "response";

public slots:
    // Slots to receive signals from UniversalLLMPropertiesWidget
    void onProviderChanged(const QString& providerId);
    void onModelChanged(const QString& modelId);
    void onSystemPromptChanged(const QString& text);
    void onUserPromptChanged(const QString& text);
    void onTemperatureChanged(double value);
    void onMaxTokensChanged(int value);

private:
    // Node state/configuration
    QString m_providerId;      // e.g., "openai", "google"
    QString m_modelId;         // e.g., "gpt-4o-mini", "gemini-pro"
    QString m_systemPrompt;    // Default system prompt from properties
    QString m_userPrompt;      // Default user prompt from properties
    double m_temperature = 0.7;
    int m_maxTokens = 1024;
};
