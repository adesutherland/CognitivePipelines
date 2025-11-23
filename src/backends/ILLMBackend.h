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
#include <QStringList>
#include <vector>

/**
 * @brief Token usage statistics returned by LLM backends.
 */
struct LLMUsage {
    int inputTokens = 0;
    int outputTokens = 0;
    int totalTokens = 0;
};

/**
 * @brief Normalized result structure returned by all LLM backends.
 *
 * This struct encapsulates the response content, token usage statistics,
 * the raw JSON response for debugging, and error information.
 */
struct LLMResult {
    QString content;        ///< The actual AI response/answer text
    LLMUsage usage;         ///< Token usage statistics
    QString rawResponse;    ///< The original full JSON for debugging
    bool hasError = false;  ///< Whether an error occurred
    QString errorMsg;       ///< Error message if hasError is true
};

/**
 * @brief Result structure returned by embedding API calls.
 *
 * This struct encapsulates the vector embedding, token usage statistics,
 * and error information for text-to-vector conversion operations.
 */
struct EmbeddingResult {
    std::vector<float> vector;  ///< The embedding vector (typically 1536 or 3072 dimensions for OpenAI)
    LLMUsage usage;             ///< Token usage statistics
    bool hasError = false;      ///< Whether an error occurred
    QString errorMsg;           ///< Error message if hasError is true
};

/**
 * @brief Abstract base class (Strategy Pattern) for all LLM backend providers.
 *
 * Each concrete implementation (OpenAI, Google, Anthropic, etc.) will implement
 * this interface, allowing the application to work with any LLM provider uniformly.
 */
class ILLMBackend {
public:
    virtual ~ILLMBackend() = default;

    /**
     * @brief Returns the unique internal ID for this backend (e.g., "openai", "google").
     * @return The backend's unique identifier.
     */
    virtual QString id() const = 0;

    /**
     * @brief Returns the human-readable name for this backend (e.g., "OpenAI", "Google Gemini").
     * @return The backend's display name.
     */
    virtual QString name() const = 0;

    /**
     * @brief Returns the list of models supported by this backend.
     * @return A list of model identifiers (e.g., ["gpt-4", "gpt-3.5-turbo"]).
     */
    virtual QStringList availableModels() const = 0;

    /**
     * @brief Returns the list of embedding models supported by this backend.
     * @return A list of embedding model identifiers (e.g., ["text-embedding-3-small", "text-embedding-3-large"]).
     */
    virtual QStringList availableEmbeddingModels() const = 0;

    /**
     * @brief Sends a prompt to the backend and returns the normalized response.
     *
     * This is a synchronous method that should be called from a background thread
     * (e.g., via QtConcurrent::run) to avoid blocking the UI.
     *
     * @param apiKey The API key for authentication with the backend.
     * @param modelName The model to use (e.g., "gpt-4o-mini").
     * @param temperature The temperature parameter for response randomness (0.0 - 2.0).
     * @param maxTokens The maximum number of tokens to generate.
     * @param systemPrompt The system prompt (instructions/context for the AI).
     * @param userPrompt The user's prompt/query.
     * @param imagePath Optional file path to an image for multimodal requests.
     * @return LLMResult containing the normalized response, usage statistics, and error information.
     */
    virtual LLMResult sendPrompt(
        const QString& apiKey,
        const QString& modelName,
        double temperature,
        int maxTokens,
        const QString& systemPrompt,
        const QString& userPrompt,
        const QString& imagePath = QString()
    ) = 0;

    /**
     * @brief Converts text into a vector embedding for RAG (Retrieval-Augmented Generation).
     *
     * This is a synchronous method that should be called from a background thread
     * (e.g., via QtConcurrent::run) to avoid blocking the UI.
     *
     * @param apiKey The API key for authentication with the backend.
     * @param modelName The embedding model to use (e.g., "text-embedding-3-small").
     * @param text The text to convert into a vector embedding.
     * @return EmbeddingResult containing the vector, usage statistics, and error information.
     */
    virtual EmbeddingResult getEmbedding(
        const QString& apiKey,
        const QString& modelName,
        const QString& text
    ) = 0;
};
