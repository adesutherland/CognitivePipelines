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

#include <QObject>
#include <QWidget>
#include <QString>
#include <QPointer>

#include "IToolConnector.h"
#include "CommonDataTypes.h"
#include "TextOutputPropertiesWidget.h"

// Skeleton TextOutput node: consumes text and presents it in a read-only widget
class TextOutputNode : public QObject, public IToolConnector {
    Q_OBJECT
    Q_INTERFACES(IToolConnector)
public:
    explicit TextOutputNode(QObject* parent = nullptr);
    ~TextOutputNode() override = default;

    // IToolConnector interface
    NodeDescriptor getDescriptor() const override;
    QWidget* createConfigurationWidget(QWidget* parent) override;
    TokenList execute(const TokenList& incomingTokens) override;
    QJsonObject saveState() const override;
    void loadState(const QJsonObject& data) override;

    // Clear all output state (internal cache and widget display)
    void clearOutput();

public:
    static constexpr const char* kInputId = "text";

private:
    QPointer<TextOutputPropertiesWidget> m_propertiesWidget; // cached UI widget
    QString m_loadedText; // cached text from loaded state to apply on widget creation
    // Cache the last value received via Execute so that if the widget wasn't yet created,
    // we can display it immediately upon widget creation (fixes first-run fan-out cases).
    QString m_lastText;
    bool m_hasPendingText {false};
};
