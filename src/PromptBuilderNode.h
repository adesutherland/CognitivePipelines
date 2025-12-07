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
#include <QStringList>

#include "IToolConnector.h"
#include "CommonDataTypes.h"

class PromptBuilderNode : public QObject, public IToolConnector {
    Q_OBJECT
    Q_INTERFACES(IToolConnector)
public:
    explicit PromptBuilderNode(QObject* parent = nullptr);
    ~PromptBuilderNode() override = default;

    // IToolConnector interface
    NodeDescriptor getDescriptor() const override;
    QWidget* createConfigurationWidget(QWidget* parent) override;
    TokenList execute(const TokenList& incomingTokens) override;
    QJsonObject saveState() const override;
    void loadState(const QJsonObject& data) override;

    // Accessors
    QString templateText() const { return m_template; }

public slots:
    void setTemplateText(const QString& text);
    void onTemplateChanged(const QString& newTemplate, const QStringList& newVariables);

signals:
    void templateTextChanged(const QString& text);
    // Request the delegate to update input pins to match the variable list
    void inputPinsUpdateRequested(const QStringList& newVariables);

public:
    static constexpr const char* kInputId = "input"; // legacy convenience variable
    static constexpr const char* kOutputId = "prompt";

private:
    QString m_template { QStringLiteral("{input}") };
    QStringList m_variables { QStringList{ QString::fromLatin1(kInputId) } };
};
