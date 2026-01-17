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

#include "RetryLoopPropertiesWidget.h"
#include "RetryLoopNode.h"

#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>

RetryLoopPropertiesWidget::RetryLoopPropertiesWidget(RetryLoopNode* node, QWidget* parent)
    : QWidget(parent)
    , m_node(node)
{
    auto* layout = new QFormLayout(this);

    m_failureLineEdit = new QLineEdit(this);
    m_failureLineEdit->setText(m_node->getFailureString());
    layout->addRow(tr("Failure Condition String"), m_failureLineEdit);

    m_maxRetriesSpinBox = new QSpinBox(this);
    m_maxRetriesSpinBox->setRange(1, 20);
    m_maxRetriesSpinBox->setValue(m_node->getMaxRetries());
    layout->addRow(tr("Max Retries"), m_maxRetriesSpinBox);

    connect(m_failureLineEdit, &QLineEdit::textChanged, m_node, &RetryLoopNode::setFailureString);
    connect(m_maxRetriesSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), m_node, &RetryLoopNode::setMaxRetries);

    // Optional: Update UI if node state changes externally
    connect(m_node, &RetryLoopNode::failureStringChanged, m_failureLineEdit, &QLineEdit::setText);
    connect(m_node, &RetryLoopNode::maxRetriesChanged, m_maxRetriesSpinBox, &QSpinBox::setValue);
}
