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
#include <QFuture>
#include <QString>
#include <QPointer>
#include <QTemporaryFile>

#include "IToolConnector.h"
#include "CommonDataTypes.h"

class PdfToImagePropertiesWidget;

class PdfToImageNode : public QObject, public IToolConnector {
    Q_OBJECT
    Q_INTERFACES(IToolConnector)
public:
    explicit PdfToImageNode(QObject* parent = nullptr);
    ~PdfToImageNode() override = default;

    // IToolConnector interface
    NodeDescriptor getDescriptor() const override;
    QWidget* createConfigurationWidget(QWidget* parent) override;
    TokenList execute(const TokenList& incomingTokens) override;
    QJsonObject saveState() const override;
    void loadState(const QJsonObject& data) override;

signals:
    void splitPagesChanged(bool split);

public slots:
    void onPdfPathChanged(const QString& path);
    void onSplitPagesChanged(bool split);

public:
    static constexpr const char* kPdfPathPinId = "pdf_path";
    static constexpr const char* kImagePathPinId = "image_path";

private:
    QPointer<PdfToImagePropertiesWidget> m_widget;
    QString m_pdfPath;  // PDF path configured via properties widget (Source Mode)
    bool m_splitPages {false};
};
