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

#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QSignalBlocker>

DatabaseConnectorPropertiesWidget::DatabaseConnectorPropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);

    // Database File Path label
    auto* pathLabel = new QLabel(tr("Database File Path:"), this);
    layout->addWidget(pathLabel);

    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText(tr("/path/to/database.sqlite"));
    layout->addWidget(m_pathEdit);

    // SQL Query label
    auto* sqlLabel = new QLabel(tr("SQL Query:"), this);
    layout->addWidget(sqlLabel);

    m_sqlEdit = new QTextEdit(this);
    m_sqlEdit->setPlaceholderText(tr("SELECT * FROM table_name"));
    m_sqlEdit->setMaximumHeight(100);
    layout->addWidget(m_sqlEdit);

    layout->addStretch();

    // Forward edits to our signal
    QObject::connect(m_pathEdit, &QLineEdit::textChanged,
                     this, &DatabaseConnectorPropertiesWidget::databasePathChanged);
    QObject::connect(m_sqlEdit, &QTextEdit::textChanged,
                     this, [this]() { emit sqlQueryChanged(m_sqlEdit->toPlainText()); });
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

void DatabaseConnectorPropertiesWidget::setSqlQuery(const QString& query)
{
    if (!m_sqlEdit)
        return;

    if (m_sqlEdit->toPlainText() == query)
        return; // No change

    {
        QSignalBlocker blocker(m_sqlEdit);
        m_sqlEdit->setPlainText(query);
    }
    // Emit after programmatic set so listeners (e.g., node model) can react
    emit sqlQueryChanged(query);
}

QString DatabaseConnectorPropertiesWidget::sqlQuery() const
{
    return m_sqlEdit ? m_sqlEdit->toPlainText() : QString();
}
