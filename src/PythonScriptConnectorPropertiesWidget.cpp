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

#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>

PythonScriptConnectorPropertiesWidget::PythonScriptConnectorPropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);

    // Executable label
    auto* execLabel = new QLabel(tr("Executable:"), this);
    layout->addWidget(execLabel);

    m_executableEdit = new QLineEdit(this);
    m_executableEdit->setText(QStringLiteral("python3 -u"));
    m_executableEdit->setPlaceholderText(tr("Path or command for Python executable"));
    layout->addWidget(m_executableEdit);

    // Script Content label
    auto* scriptLabel = new QLabel(tr("Script Content:"), this);
    layout->addWidget(scriptLabel);

    m_scriptEdit = new QTextEdit(this);
    m_scriptEdit->setAcceptRichText(false);
    m_scriptEdit->setPlaceholderText(tr("Write your Python script here"));
    layout->addWidget(m_scriptEdit);

    layout->addStretch();

    // Forward changes to public signals
    connect(m_executableEdit, &QLineEdit::textChanged,
            this, &PythonScriptConnectorPropertiesWidget::executableChanged);

    connect(m_scriptEdit, &QTextEdit::textChanged, this, [this]() {
        emit scriptContentChanged(m_scriptEdit->toPlainText());
    });
}
