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
#include "MermaidPropertiesWidget.h"

#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QTextEdit>
#include <QVBoxLayout>

MermaidPropertiesWidget::MermaidPropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);

    auto* formLayout = new QFormLayout();
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setSpacing(8);

    m_scaleSpin = new QDoubleSpinBox(this);
    m_scaleSpin->setRange(0.1, 4.0);
    m_scaleSpin->setSingleStep(0.1);
    m_scaleSpin->setValue(1.0);
    formLayout->addRow(tr("Resolution scale:"), m_scaleSpin);
    connect(m_scaleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MermaidPropertiesWidget::scaleChanged);

    layout->addLayout(formLayout);

    auto* label = new QLabel(tr("Last rendered Mermaid code (read-only):"), this);
    layout->addWidget(label);

    m_codeEdit = new QTextEdit(this);
    m_codeEdit->setReadOnly(true);
    m_codeEdit->setAcceptRichText(false);
    m_codeEdit->setPlaceholderText(tr("No render has been performed yet."));
    m_codeEdit->setLineWrapMode(QTextEdit::NoWrap);
    m_codeEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    layout->addWidget(m_codeEdit);

    layout->addStretch();
}

void MermaidPropertiesWidget::setCode(const QString& code)
{
    if (!m_codeEdit) return;
    if (m_codeEdit->toPlainText() == code) return;
    m_codeEdit->setPlainText(code);
}

double MermaidPropertiesWidget::scale() const
{
    return m_scaleSpin ? m_scaleSpin->value() : 1.0;
}

void MermaidPropertiesWidget::setScale(double value)
{
    if (!m_scaleSpin) return;
    if (value < 0.1) value = 0.1;
    if (value > 4.0) value = 4.0;
    if (m_scaleSpin->value() == value) return;
    QSignalBlocker blocker(m_scaleSpin);
    m_scaleSpin->setValue(value);
}
