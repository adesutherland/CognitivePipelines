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
#include "TextInputPropertiesWidget.h"
#include <QVBoxLayout>
#include <QLabel>

TextInputPropertiesWidget::TextInputPropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);

    // Label
    auto* label = new QLabel(tr("Text:"), this);
    layout->addWidget(label);

    m_textEdit = new QTextEdit(this);
    m_textEdit->setPlaceholderText(tr("Enter text to inject into the pipeline..."));
    m_textEdit->setAcceptRichText(false);
    layout->addWidget(m_textEdit);

    layout->addStretch();

    connect(m_textEdit, &QTextEdit::textChanged, this, [this]() {
        emit textChanged(m_textEdit->toPlainText());
    });
}

void TextInputPropertiesWidget::setText(const QString& text)
{
    if (m_textEdit && m_textEdit->toPlainText() != text) {
        m_textEdit->setPlainText(text);
    }
}

QString TextInputPropertiesWidget::text() const
{
    return m_textEdit ? m_textEdit->toPlainText() : QString();
}
