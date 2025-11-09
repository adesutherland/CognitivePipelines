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

#include <string>

// Qt
#include <QString>
#include <QJsonObject>

// A simple LLM API client using cpr to send a hardcoded chat completion request.
// This class sends a prompt to a standard chat completion endpoint and returns
// the first message content from the response, or an error description string on failure.
class LlmApiClient {
public:
    // Multi‑provider support
    enum class ApiProvider { OpenAI, Google };

    // New provider-aware API using robust QJson request building.
    // Note: this variant currently performs the request synchronously and
    // intentionally does not return a value (API surface placeholder for future async refactor).
    // The legacy API below still returns the response text for callers that need it.
    QString sendPrompt(ApiProvider provider,
                    const QString &apiKey,
                    const QString &model,
                    double temperature,
                    int maxTokens,
                    const QString &systemPrompt,
                    const QString &userPrompt);

    // New API key accessor that reads a specific provider key from accounts.json
    // (e.g., providerKey = "openai_api_key").
    QString getApiKey(const QString &providerKey) const;

    // Legacy single-provider API kept for compatibility with existing callers/tests.
    // Sends the given promptText to the LLM using the provided apiKey and returns result or error.
    std::string sendPrompt(const std::string& apiKey, const std::string& promptText);

private:
    // Very small helper to extract the first choice.message.content from a JSON string
    // without introducing a full JSON dependency. This is a simplified extractor and
    // should be replaced with a robust JSON parser for production use.
    static std::string extractFirstMessageContent(const std::string& jsonBody);
};
