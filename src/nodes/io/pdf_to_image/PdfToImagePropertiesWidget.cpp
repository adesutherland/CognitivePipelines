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
#include "PdfToImagePropertiesWidget.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QFileDialog>

PdfToImagePropertiesWidget::PdfToImagePropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);

    // Label
    auto* label = new QLabel(tr("PDF File:"), this);
    layout->addWidget(label);

    // Path line edit - read-only to display the selected PDF path
    m_pathLineEdit = new QLineEdit(this);
    m_pathLineEdit->setReadOnly(true);
    m_pathLineEdit->setPlaceholderText(tr("No PDF selected"));
    layout->addWidget(m_pathLineEdit);

    // Select button
    m_selectButton = new QPushButton(tr("Select PDF..."), this);
    layout->addWidget(m_selectButton);

    // Split checkbox
    m_splitCheckBox = new QCheckBox(tr("Split pages into separate images"), this);
    layout->addWidget(m_splitCheckBox);

    // Connect checkbox signal
    connect(m_splitCheckBox, &QCheckBox::toggled, this, &PdfToImagePropertiesWidget::splitPagesChanged);

    // Connect button click handler
    connect(m_selectButton, &QPushButton::clicked, this, [this]() {
        QString fileName = QFileDialog::getOpenFileName(
            this,
            tr("Select PDF"),
            QString(),
            tr("PDF Files (*.pdf)")
        );
        
        if (!fileName.isEmpty()) {
            // Update UI immediately
            setPdfPath(fileName);
            // Notify the Node
            emit pdfPathChanged(fileName);
        }
    });

    layout->addStretch();
}

void PdfToImagePropertiesWidget::setPdfPath(const QString& path)
{
    if (m_pathLineEdit) {
        m_pathLineEdit->setText(path);
    }
}

void PdfToImagePropertiesWidget::setSplitPages(bool split)
{
    if (m_splitCheckBox) {
        m_splitCheckBox->setChecked(split);
    }
}

QString PdfToImagePropertiesWidget::pdfPath() const
{
    if (m_pathLineEdit && !m_pathLineEdit->text().isEmpty()) {
        return m_pathLineEdit->text();
    }
    return QString();
}

bool PdfToImagePropertiesWidget::splitPages() const
{
    return m_splitCheckBox && m_splitCheckBox->isChecked();
}
