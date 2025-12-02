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

RagQueryPropertiesWidget::RagQueryPropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QFormLayout(this);

    m_maxResultsSpinBox = new QSpinBox(this);
    m_maxResultsSpinBox->setRange(1, 50);
    m_maxResultsSpinBox->setValue(5);

    m_minRelevanceSpinBox = new QDoubleSpinBox(this);
    m_minRelevanceSpinBox->setRange(0.0, 1.0);
    m_minRelevanceSpinBox->setSingleStep(0.05);
    m_minRelevanceSpinBox->setDecimals(2);
    m_minRelevanceSpinBox->setValue(0.5);

    layout->addRow(tr("Max Results"), m_maxResultsSpinBox);
    layout->addRow(tr("Min Relevance"), m_minRelevanceSpinBox);

    connect(m_maxResultsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RagQueryPropertiesWidget::maxResultsChanged);
    connect(m_minRelevanceSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RagQueryPropertiesWidget::minRelevanceChanged);
}

int RagQueryPropertiesWidget::maxResults() const
{
    return m_maxResultsSpinBox ? m_maxResultsSpinBox->value() : 5;
}

double RagQueryPropertiesWidget::minRelevance() const
{
    return m_minRelevanceSpinBox ? m_minRelevanceSpinBox->value() : 0.5;
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
