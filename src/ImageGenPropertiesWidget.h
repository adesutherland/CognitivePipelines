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

class QComboBox;

// Properties widget for configuring ImageGenNode
class ImageGenPropertiesWidget : public QWidget {
    Q_OBJECT
public:
    explicit ImageGenPropertiesWidget(QWidget* parent = nullptr);
    ~ImageGenPropertiesWidget() override = default;

    QString provider() const;
    QString model() const;
    QString size() const;
    QString quality() const;
    QString style() const;

    void setProvider(const QString& providerName);
    void setModel(const QString& modelName);
    void setSize(const QString& sizeValue);
    void setQuality(const QString& qualityValue);
    void setStyle(const QString& styleValue);

signals:
    void configChanged();

private:
    int setComboValue(QComboBox* combo, const QString& value);

    QComboBox* m_providerCombo {nullptr};
    QComboBox* m_modelCombo {nullptr};
    QComboBox* m_sizeCombo {nullptr};
    QComboBox* m_qualityCombo {nullptr};
    QComboBox* m_styleCombo {nullptr};
};
