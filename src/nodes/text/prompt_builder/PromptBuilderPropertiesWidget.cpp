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
#include "PromptBuilderPropertiesWidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QRegularExpression>
#include <QSet>

PromptBuilderPropertiesWidget::PromptBuilderPropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);

    // Label
    auto* label = new QLabel(tr("Template:"), this);
    layout->addWidget(label);

    m_templateEdit = new QTextEdit(this);
    m_templateEdit->setPlaceholderText(tr("Write your prompt template here, e.g., 'Summarize this: {question} {context}'"));
    m_templateEdit->setAcceptRichText(false);
    layout->addWidget(m_templateEdit);

    layout->addStretch();

    // Debounce timer to avoid heavy parsing on every keystroke
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(300); // 300ms debounce

    // Ensure no lingering connections exist between the editor and this widget (defensive cleanup)
    QObject::disconnect(m_templateEdit, nullptr, this, nullptr);

    // When text changes, (re)start the debounce timer.
    connect(m_templateEdit, &QTextEdit::textChanged, this, &PromptBuilderPropertiesWidget::onTextChanged);

    // When the timer fires, parse and emit the canonical update.
    connect(m_debounceTimer, &QTimer::timeout, this, &PromptBuilderPropertiesWidget::onDebounceTimeout);
}

void PromptBuilderPropertiesWidget::setTemplateText(const QString& text)
{
    if (m_templateEdit && m_templateEdit->toPlainText() != text) {
        m_templateEdit->setPlainText(text);
        // For programmatic updates (e.g., load state), emit immediately to keep UI and node in sync.
        if (m_debounceTimer) m_debounceTimer->stop();
        onDebounceTimeout();
    }
}

QString PromptBuilderPropertiesWidget::templateText() const
{
    return m_templateEdit ? m_templateEdit->toPlainText() : QString();
}

void PromptBuilderPropertiesWidget::onTextChanged()
{
    if (m_debounceTimer) {
        m_debounceTimer->start();
    }
}

void PromptBuilderPropertiesWidget::onDebounceTimeout()
{
    const QString text = m_templateEdit ? m_templateEdit->toPlainText() : QString();
    // Parse unique variables of the form {var}
    static const QRegularExpression re(QStringLiteral("\\{([^{}]+)\\}"));
    QStringList vars;
    QSet<QString> seen;
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        const QString var = m.captured(1).trimmed();
        if (!var.isEmpty() && !seen.contains(var)) {
            seen.insert(var);
            vars.append(var);
        }
    }
    // Ensure at least one convenience input exists so users always have an input pin
    if (vars.isEmpty()) {
        vars.append(QStringLiteral("input"));
    }
    emit templateChanged(text, vars);
}
