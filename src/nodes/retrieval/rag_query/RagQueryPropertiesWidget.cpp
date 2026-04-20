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

#include "RagQueryPropertiesWidget.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QPlainTextEdit>
#include <QSignalBlocker>

RagQueryPropertiesWidget::RagQueryPropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    auto* formLayout = new QFormLayout();

    // Database path with browse button
    m_databaseEdit = new QLineEdit(this);
    m_browseDatabaseBtn = new QPushButton(QStringLiteral("Browse..."), this);
    auto* dbLayout = new QHBoxLayout();
    dbLayout->addWidget(m_databaseEdit);
    dbLayout->addWidget(m_browseDatabaseBtn);
    formLayout->addRow(tr("Database File:"), dbLayout);

    // Default query (multi-line)
    m_queryEdit = new QPlainTextEdit(this);
    m_queryEdit->setPlaceholderText(tr("Enter default query text (optional)"));
    formLayout->addRow(tr("Default Query:"), m_queryEdit);

    m_maxResultsSpinBox = new QSpinBox(this);
    m_maxResultsSpinBox->setRange(1, 50);
    m_maxResultsSpinBox->setValue(5);

    m_minRelevanceSpinBox = new QDoubleSpinBox(this);
    m_minRelevanceSpinBox->setRange(0.0, 1.0);
    m_minRelevanceSpinBox->setSingleStep(0.05);
    m_minRelevanceSpinBox->setDecimals(2);
    m_minRelevanceSpinBox->setValue(0.5);

    formLayout->addRow(tr("Max Results"), m_maxResultsSpinBox);
    formLayout->addRow(tr("Min Relevance"), m_minRelevanceSpinBox);

    mainLayout->addLayout(formLayout);
    mainLayout->addStretch();

    connect(m_maxResultsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RagQueryPropertiesWidget::maxResultsChanged);
    connect(m_minRelevanceSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RagQueryPropertiesWidget::minRelevanceChanged);

    // New controls wiring
    connect(m_databaseEdit, &QLineEdit::textChanged,
            this, &RagQueryPropertiesWidget::databasePathChanged);
    connect(m_browseDatabaseBtn, &QPushButton::clicked,
            this, &RagQueryPropertiesWidget::onBrowseDatabase);

    connect(m_queryEdit, &QPlainTextEdit::textChanged, this, [this]() {
        emit queryTextChanged(m_queryEdit->toPlainText());
    });
}

int RagQueryPropertiesWidget::maxResults() const
{
    return m_maxResultsSpinBox ? m_maxResultsSpinBox->value() : 5;
}

double RagQueryPropertiesWidget::minRelevance() const
{
    return m_minRelevanceSpinBox ? m_minRelevanceSpinBox->value() : 0.5;
}

QString RagQueryPropertiesWidget::databasePath() const
{
    return m_databaseEdit ? m_databaseEdit->text() : QString();
}

QString RagQueryPropertiesWidget::queryText() const
{
    return m_queryEdit ? m_queryEdit->toPlainText() : QString();
}

void RagQueryPropertiesWidget::setMaxResults(int value)
{
    if (m_maxResultsSpinBox && m_maxResultsSpinBox->value() != value) {
        m_maxResultsSpinBox->setValue(value);
    }
}

void RagQueryPropertiesWidget::setMinRelevance(double value)
{
    if (m_minRelevanceSpinBox && m_minRelevanceSpinBox->value() != value) {
        m_minRelevanceSpinBox->setValue(value);
    }
}

void RagQueryPropertiesWidget::setDatabasePath(const QString& path)
{
    if (!m_databaseEdit) {
        return;
    }
    if (m_databaseEdit->text() != path) {
        const QSignalBlocker blocker(m_databaseEdit);
        m_databaseEdit->setText(path);
    }
}

void RagQueryPropertiesWidget::setQueryText(const QString& text)
{
    if (!m_queryEdit) {
        return;
    }
    if (m_queryEdit->toPlainText() != text) {
        const QSignalBlocker blocker(m_queryEdit);
        m_queryEdit->setPlainText(text);
    }
}

void RagQueryPropertiesWidget::onBrowseDatabase()
{
    const QString currentPath = m_databaseEdit ? m_databaseEdit->text() : QString();
    QString filter = QStringLiteral("SQLite Databases (*.db *.sqlite);;All Files (*)");
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Select Database File"),
        currentPath,
        filter);

    if (!fileName.isEmpty() && m_databaseEdit) {
        m_databaseEdit->setText(fileName);
    }
}
