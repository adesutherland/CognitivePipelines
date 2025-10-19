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
#include "LLMConnectorPropertiesWidget.h"

#include <QVBoxLayout>

LLMConnectorPropertiesWidget::LLMConnectorPropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* form = new QFormLayout(this);
    form->setContentsMargins(4, 4, 4, 4);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_promptEdit = new QTextEdit(this);
    m_promptEdit->setPlaceholderText(tr("Enter system message or prompt..."));
    m_promptEdit->setAcceptRichText(false);

    m_apiKeyEdit = new QLineEdit(this);
    m_apiKeyEdit->setPlaceholderText(tr("Enter API key"));
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);

    form->addRow(tr("Prompt"), m_promptEdit);
    form->addRow(tr("API Key"), m_apiKeyEdit);

    // Wire UI changes to our signals (live updates)
    connect(m_promptEdit, &QTextEdit::textChanged, this, [this]() {
        emit promptChanged(m_promptEdit->toPlainText());
    });
    connect(m_apiKeyEdit, &QLineEdit::textChanged, this, &LLMConnectorPropertiesWidget::apiKeyChanged);
}

void LLMConnectorPropertiesWidget::setPromptText(const QString& text)
{
    if (m_promptEdit->toPlainText() != text)
        m_promptEdit->setPlainText(text);
}

void LLMConnectorPropertiesWidget::setApiKeyText(const QString& text)
{
    if (m_apiKeyEdit->text() != text)
        m_apiKeyEdit->setText(text);
}

QString LLMConnectorPropertiesWidget::promptText() const
{
    return m_promptEdit->toPlainText();
}

QString LLMConnectorPropertiesWidget::apiKeyText() const
{
    return m_apiKeyEdit->text();
}
