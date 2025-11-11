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
#include "PythonScriptConnectorPropertiesWidget.h"

#include <QFormLayout>
#include <QLineEdit>
#include <QTextEdit>

PythonScriptConnectorPropertiesWidget::PythonScriptConnectorPropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* form = new QFormLayout(this);
    form->setContentsMargins(4, 4, 4, 4);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_executableEdit = new QLineEdit(this);
    m_executableEdit->setText(QStringLiteral("python3 -u"));
    m_executableEdit->setPlaceholderText(tr("Path or command for Python executable"));

    m_scriptEdit = new QTextEdit(this);
    m_scriptEdit->setAcceptRichText(false);
    m_scriptEdit->setPlaceholderText(tr("Write your Python script here"));

    form->addRow(tr("Executable:"), m_executableEdit);
    form->addRow(tr("Script Content:"), m_scriptEdit);

    // Forward changes to public signals
    connect(m_executableEdit, &QLineEdit::textChanged,
            this, &PythonScriptConnectorPropertiesWidget::executableChanged);

    connect(m_scriptEdit, &QTextEdit::textChanged, this, [this]() {
        emit scriptContentChanged(m_scriptEdit->toPlainText());
    });
}
