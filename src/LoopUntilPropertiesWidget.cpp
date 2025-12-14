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

#include "LoopUntilPropertiesWidget.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QSpinBox>

LoopUntilPropertiesWidget::LoopUntilPropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);

    auto* label = new QLabel(tr("Max Iterations:"), this);
    layout->addWidget(label);

    m_spin = new QSpinBox(this);
    m_spin->setRange(1, 1000);
    m_spin->setValue(10);
    layout->addWidget(m_spin);

    layout->addStretch();

    connect(m_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v){
        emit maxIterationsChanged(v);
    });
}

void LoopUntilPropertiesWidget::setMaxIterations(int value)
{
    if (!m_spin) return;
    if (m_spin->value() == value) return;
    m_spin->setValue(value);
}

int LoopUntilPropertiesWidget::maxIterations() const
{
    return m_spin ? m_spin->value() : 10;
}
