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

class QLineEdit;
class QDoubleSpinBox;
class QSpinBox;
class QString;

// Properties editor for GoogleLLMConnector
class GoogleLLMConnectorPropertiesWidget : public QWidget {
    Q_OBJECT
public:
    explicit GoogleLLMConnectorPropertiesWidget(QWidget* parent = nullptr);
    ~GoogleLLMConnectorPropertiesWidget() override = default;

    // Getters
    QString getModelName() const;
    double getTemperature() const;
    int getMaxTokens() const;

    // Setters (block signals to avoid redundant emissions)
    void setModelName(const QString& modelName);
    void setTemperature(double temp);
    void setMaxTokens(int tokens);

signals:
    void modelNameChanged(const QString &modelName);
    void temperatureChanged(double temp);
    void maxTokensChanged(int tokens);

private:
    QLineEdit* modelLineEdit {nullptr};
    QDoubleSpinBox* temperatureSpinBox {nullptr};
    QSpinBox* maxTokensSpinBox {nullptr};
};
