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

#include <QWidget>

class QComboBox;
class QTextEdit;
class QDoubleSpinBox;
class QSpinBox;
class QFormLayout;

/**
 * @brief Properties widget for the Universal LLM Node.
 *
 * This widget provides a dynamic UI that allows users to select an AI Provider
 * (OpenAI, Google, etc.) and then updates the Model list based on that selection.
 * It interacts with the LLMProviderRegistry to populate available backends and models.
 */
class UniversalLLMPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    explicit UniversalLLMPropertiesWidget(QWidget* parent = nullptr);
    ~UniversalLLMPropertiesWidget() override = default;

    // Setters for initializing widget state
    void setProvider(const QString& providerId);
    void setModel(const QString& modelId);
    void setSystemPrompt(const QString& text);
    void setUserPrompt(const QString& text);
    void setTemperature(double value);
    void setMaxTokens(int value);

    // Getters for reading current state
    QString provider() const;
    QString model() const;
    QString systemPrompt() const;
    QString userPrompt() const;
    double temperature() const;
    int maxTokens() const;

signals:
    void providerChanged(const QString& providerId);
    void modelChanged(const QString& modelId);
    void systemPromptChanged(const QString& text);
    void userPromptChanged(const QString& text);
    void temperatureChanged(double val);
    void maxTokensChanged(int val);

private slots:
    void onProviderChanged(int index);

private:
    QComboBox* m_providerCombo {nullptr};
    QComboBox* m_modelCombo {nullptr};
    QTextEdit* m_systemPromptEdit {nullptr};
    QTextEdit* m_userPromptEdit {nullptr};
    QDoubleSpinBox* m_temperatureSpinBox {nullptr};
    QSpinBox* m_maxTokensSpinBox {nullptr};
};
