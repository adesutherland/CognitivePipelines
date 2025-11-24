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
    m_descriptionLabel->setAlignment(Qt::AlignCenter | Qt::AlignCenter);
    m_descriptionLabel->setFixedWidth(100);
    
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
    
    // Set fixed width on the widget itself to ensure proper height calculation
    setFixedWidth(100);
    
    // Set layout constraint to ensure the widget resizes to fit the label's height
    m_layout->setSizeConstraint(QLayout::SetFixedSize);
    
    // Initially hidden until description is set
    hide();
}

void NodeInfoWidget::setDescription(const QString& text)
{
    if (text.isEmpty()) {
        hide();
        return;
    }

    m_descriptionLabel->setText(text);

    // 1. Invalidate the layout to ensure it recalculates based on the new text
    m_layout->invalidate();
    m_layout->activate();

    // 2. Explicitly resize the widget to the layout's preferred size (height-for-width)
    // This removes ambiguity for the parent QGraphicsProxyWidget
    QSize newSize = m_layout->sizeHint();
    this->resize(newSize);

    // 3. Show and force a repaint
    show();
    m_descriptionLabel->update();
    this->update();

/*

    if (text.isEmpty()) {
        // Hide the widget when there's no description, allowing the node to shrink
        hide();
    } else {
        // Update the label text and show the widget
        m_descriptionLabel->setText(text);
        
        // Force geometry recalculation immediately to ensure proper wrapping
        m_descriptionLabel->adjustSize();
        this->adjustSize();
        show();
        
        // Force repaint after resize (important: repaint AFTER geometry changes)
        //m_descriptionLabel->update();
        //this->update();
        
        show();
    }
    */
}
