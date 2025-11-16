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
#include <QTextEdit>
#include <QFormLayout>
#include <QStringList>
#include <QTimer>

// Property editor widget for PromptBuilderNode
class PromptBuilderPropertiesWidget : public QWidget {
    Q_OBJECT
public:
    explicit PromptBuilderPropertiesWidget(QWidget* parent = nullptr);
    ~PromptBuilderPropertiesWidget() override = default;

    // Initialize / update UI values from external state
    void setTemplateText(const QString& text);

    // Read current values
    QString templateText() const;

signals:
    // Emitted whenever the template text changes in the editor. Provides the
    // full template and the extracted unique variable list (order of first occurrence).
    void templateChanged(const QString& newTemplate, const QStringList& newVariables);

private:
    QTextEdit* m_templateEdit {nullptr};
    QTimer* m_debounceTimer {nullptr};

private slots:
    void onTextChanged();
    void onDebounceTimeout();
};
