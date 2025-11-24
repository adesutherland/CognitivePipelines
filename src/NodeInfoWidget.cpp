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
#include "NodeInfoWidget.h"

#include <QLabel>
#include <QVBoxLayout>

NodeInfoWidget::NodeInfoWidget(QWidget* parent)
    : QWidget(parent)
    , m_layout(new QVBoxLayout(this))
    , m_descriptionLabel(new QLabel(this))
{
    // Set translucent background for seamless integration into the node
    setAttribute(Qt::WA_TranslucentBackground, true);
    
    // Set zero margins for compact layout
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    
    // Configure the description label with fixed width for proper wrapping
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_descriptionLabel->setFixedWidth(150);
    
    // Apply "Clean UI" styling: transparent background, light grey text, italic
    m_descriptionLabel->setStyleSheet(
        "QLabel {"
        "  background-color: transparent;"
        "  border: none;"
        "  color: #e0e0e0;"
        "  font-style: italic;"
        "  padding: 6px;"
        "  font-size: 10pt;"
        "}"
    );
    
    m_layout->addWidget(m_descriptionLabel);
    setLayout(m_layout);
    
    // Set layout constraint to allow dynamic height while maintaining fixed width
    m_layout->setSizeConstraint(QLayout::SetMinimumSize);

    // Make sure it is the right size
    m_descriptionLabel->adjustSize();
    this->adjustSize();

    // Initially hidden until description is set
    hide();
}

void NodeInfoWidget::setDescription(const QString& text)
{
    if (text.isEmpty()) {
        m_descriptionLabel->clear();
        m_descriptionLabel->adjustSize();
        this->adjustSize();
        hide();
    }
    else {
        m_descriptionLabel->setText(text);
        m_descriptionLabel->adjustSize();
        this->adjustSize();
        show();
    }
}

