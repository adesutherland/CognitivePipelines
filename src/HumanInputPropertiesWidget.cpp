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
#include "HumanInputPropertiesWidget.h"
#include <QLabel>

HumanInputPropertiesWidget::HumanInputPropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(4, 4, 4, 4);

    // Add label
    auto* label = new QLabel(QStringLiteral("Default Prompt:"), this);
    vbox->addWidget(label);

    m_textEdit = new QTextEdit(this);
    m_textEdit->setPlaceholderText(QStringLiteral("Enter a default prompt to use when no input is provided..."));
    m_textEdit->setMaximumHeight(150);

    vbox->addWidget(m_textEdit);
    vbox->addStretch();

    // Connect textChanged signal to emit defaultPromptChanged
    connect(m_textEdit, &QTextEdit::textChanged, this, [this]() {
        emit defaultPromptChanged(m_textEdit->toPlainText());
    });
}

QString HumanInputPropertiesWidget::defaultPrompt() const
{
    return m_textEdit ? m_textEdit->toPlainText() : QString();
}

void HumanInputPropertiesWidget::setDefaultPrompt(const QString& text)
{
    if (m_textEdit) {
        // Block signals temporarily to avoid emitting defaultPromptChanged during programmatic set
        m_textEdit->blockSignals(true);
        m_textEdit->setPlainText(text);
        m_textEdit->blockSignals(false);
    }
}
