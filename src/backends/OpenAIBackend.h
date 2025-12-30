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

#include "ILLMBackend.h"
#include <QMutex>
#include <QByteArray>
#include <QStringList>

/**
 * @brief OpenAI backend implementation using the Chat Completions API.
 *
 * This backend communicates with OpenAI's API endpoints and supports
 * models including gpt-4o, o1-preview, and legacy models.
 */
class OpenAIBackend : public ILLMBackend {
public:
    OpenAIBackend();
    ~OpenAIBackend() override = default;

    QString id() const override;
    QString name() const override;
    QStringList availableModels() const override;
    QStringList availableEmbeddingModels() const override;

    // Dynamic discovery API (async)
    QFuture<QStringList> fetchModelList() override;

    LLMResult sendPrompt(
        const QString& apiKey,
        const QString& modelName,
        double temperature,
        int maxTokens,
        const QString& systemPrompt,
        const QString& userPrompt,
        const QString& imagePath = QString()
    ) override;

    EmbeddingResult getEmbedding(
        const QString& apiKey,
        const QString& modelName,
        const QString& text
    ) override;

    QFuture<QString> generateImage(
        const QString& prompt,
        const QString& model,
        const QString& size,
        const QString& quality,
        const QString& style
    ) override;

protected:
    // Test seam: allows tests to override the raw JSON fetcher without real network
    virtual QFuture<QByteArray> fetchRawModelListJson();

private:
    mutable QMutex m_cacheMutex;
    QStringList m_cachedModels;
};
