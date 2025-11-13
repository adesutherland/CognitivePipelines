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
#include "DatabaseConnectorPropertiesWidget.h"

#include <QFormLayout>
#include <QLineEdit>
#include <QSignalBlocker>

DatabaseConnectorPropertiesWidget::DatabaseConnectorPropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QFormLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText(tr("/path/to/database.sqlite"));

    layout->addRow(tr("Database File Path:"), m_pathEdit);

    setLayout(layout);

    // Forward edits to our signal
    QObject::connect(m_pathEdit, &QLineEdit::textChanged,
                     this, &DatabaseConnectorPropertiesWidget::databasePathChanged);
}

void DatabaseConnectorPropertiesWidget::setDatabasePath(const QString& path)
{
    if (!m_pathEdit)
        return;

    if (m_pathEdit->text() == path)
        return; // No change

    {
        QSignalBlocker blocker(m_pathEdit);
        m_pathEdit->setText(path);
    }
    // Emit after programmatic set so listeners (e.g., node model) can react
    emit databasePathChanged(path);
}

QString DatabaseConnectorPropertiesWidget::databasePath() const
{
    return m_pathEdit ? m_pathEdit->text() : QString();
}
