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

#include "ConditionalRouterPropertiesWidget.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>

ConditionalRouterPropertiesWidget::ConditionalRouterPropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);

    auto* label = new QLabel(tr("Default Condition:"), this);
    layout->addWidget(label);

    m_combo = new QComboBox(this);
    // Display labels (map to internal tokens used by the node)
    m_combo->addItem(tr("False (Default)"), QStringLiteral("false")); // index 0
    m_combo->addItem(tr("True (Default)"), QStringLiteral("true"));   // index 1
    m_combo->addItem(tr("Wait for Signal"), QStringLiteral("wait"));  // index 2
    // Default to index 0 (false) to match node's initial mode
    m_combo->setCurrentIndex(0);
    layout->addWidget(m_combo);

    layout->addStretch();

    connect(m_combo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (!m_combo) {
            return;
        }
        const QString value = m_combo->itemData(index).toString();
        emit defaultConditionChanged(value);
    });
}

void ConditionalRouterPropertiesWidget::setDefaultCondition(const QString& condition)
{
    if (!m_combo) {
        return;
    }

    // Try to find matching item by stored value ("false" / "true" / "wait").
    const int idx = m_combo->findData(condition.trimmed().toLower());
    if (idx >= 0 && idx != m_combo->currentIndex()) {
        m_combo->setCurrentIndex(idx);
    }
}

QString ConditionalRouterPropertiesWidget::defaultCondition() const
{
    if (!m_combo) {
        return QString();
    }
    return m_combo->currentData().toString();
}
