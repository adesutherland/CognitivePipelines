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
#include "GoogleLLMConnectorPropertiesWidget.h"

#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QSignalBlocker>

GoogleLLMConnectorPropertiesWidget::GoogleLLMConnectorPropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* form = new QFormLayout(this);
    form->setContentsMargins(4, 4, 4, 4);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    // Model field (first row)
    modelLineEdit = new QLineEdit(this);
    modelLineEdit->setPlaceholderText(tr("e.g., gemini-1.5-pro-latest"));
    form->addRow(tr("Model:"), modelLineEdit);

    // Temperature control
    temperatureSpinBox = new QDoubleSpinBox(this);
    temperatureSpinBox->setRange(0.0, 2.0);
    temperatureSpinBox->setSingleStep(0.1);
    temperatureSpinBox->setDecimals(2);
    temperatureSpinBox->setValue(0.7);
    form->addRow(tr("Temperature:"), temperatureSpinBox);

    // Max Tokens control
    maxTokensSpinBox = new QSpinBox(this);
    maxTokensSpinBox->setRange(1, 16384);
    maxTokensSpinBox->setValue(1024);
    form->addRow(tr("Max Tokens:"), maxTokensSpinBox);

    // Wire UI changes to our signals (live updates)
    connect(modelLineEdit, &QLineEdit::textChanged, this, &GoogleLLMConnectorPropertiesWidget::modelNameChanged);
    connect(temperatureSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &GoogleLLMConnectorPropertiesWidget::temperatureChanged);
    connect(maxTokensSpinBox, qOverload<int>(&QSpinBox::valueChanged), this, &GoogleLLMConnectorPropertiesWidget::maxTokensChanged);
}

QString GoogleLLMConnectorPropertiesWidget::getModelName() const
{
    return modelLineEdit ? modelLineEdit->text() : QString();
}

void GoogleLLMConnectorPropertiesWidget::setModelName(const QString& modelName)
{
    if (!modelLineEdit) return;
    if (modelLineEdit->text() == modelName) return;
    const QSignalBlocker blocker(modelLineEdit);
    modelLineEdit->setText(modelName);
}

double GoogleLLMConnectorPropertiesWidget::getTemperature() const
{
    return temperatureSpinBox ? temperatureSpinBox->value() : 0.0;
}

void GoogleLLMConnectorPropertiesWidget::setTemperature(double temp)
{
    if (!temperatureSpinBox) return;
    const QSignalBlocker blocker(temperatureSpinBox);
    temperatureSpinBox->setValue(temp);
}

int GoogleLLMConnectorPropertiesWidget::getMaxTokens() const
{
    return maxTokensSpinBox ? maxTokensSpinBox->value() : 0;
}

void GoogleLLMConnectorPropertiesWidget::setMaxTokens(int tokens)
{
    if (!maxTokensSpinBox) return;
    const QSignalBlocker blocker(maxTokensSpinBox);
    maxTokensSpinBox->setValue(tokens);
}
