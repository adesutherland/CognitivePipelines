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
#pragma once

#include <QWidget>

class QLabel;
class QVBoxLayout;

/**
 * @brief NodeInfoWidget displays node metadata (description) as an embedded widget within a node.
 * 
 * This widget is designed to be returned by NodeDelegateModel::embeddedWidget() and will
 * automatically hide itself when the description is empty, allowing the node to shrink.
 */
class NodeInfoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NodeInfoWidget(QWidget* parent = nullptr);
    ~NodeInfoWidget() override = default;

    // Override to provide proper size hint for layout system
    QSize sizeHint() const override;

public slots:
    /**
     * @brief Sets the description text to display.
     * @param text The description text. If empty, the widget will be hidden.
     */
    void setDescription(const QString& text);

private:
    QVBoxLayout* m_layout;
    QLabel* m_descriptionLabel;
};
