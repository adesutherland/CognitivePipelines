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
#include "ProcessConnectorPropertiesWidget.h"

#include <QFormLayout>
#include <QLineEdit>
#include <QSignalBlocker>

ProcessConnectorPropertiesWidget::ProcessConnectorPropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QFormLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    commandLineEdit = new QLineEdit(this);
    commandLineEdit->setPlaceholderText(tr("Enter command line (e.g., /usr/bin/python3 script.py --arg)")); 

    layout->addRow(tr("Command:"), commandLineEdit);

    setLayout(layout);

    // Forward edits to our signal
    QObject::connect(commandLineEdit, &QLineEdit::textChanged,
                     this, &ProcessConnectorPropertiesWidget::commandChanged);
}

QString ProcessConnectorPropertiesWidget::getCommand() const
{
    return commandLineEdit ? commandLineEdit->text() : QString();
}

void ProcessConnectorPropertiesWidget::setCommand(const QString& command)
{
    if (!commandLineEdit)
        return;

    if (commandLineEdit->text() == command)
        return; // No change, avoid spurious emissions

    QSignalBlocker blocker(commandLineEdit);
    commandLineEdit->setText(command);
}
