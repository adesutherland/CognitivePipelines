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
#include "ImageGenPropertiesWidget.h"

#include <QComboBox>
#include <QFormLayout>
#include <QSignalBlocker>
#include <QVBoxLayout>

ImageGenPropertiesWidget::ImageGenPropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(4, 4, 4, 4);
    rootLayout->setSpacing(8);

    auto* layout = new QFormLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    // Provider
    m_providerCombo = new QComboBox(this);
    m_providerCombo->addItem(QStringLiteral("OpenAI"));
    layout->addRow(tr("Provider:"), m_providerCombo);

    // Model
    m_modelCombo = new QComboBox(this);
    m_modelCombo->addItem(QStringLiteral("dall-e-3"));
    layout->addRow(tr("Model:"), m_modelCombo);

    // Size
    m_sizeCombo = new QComboBox(this);
    m_sizeCombo->addItems({
        QStringLiteral("1024x1024"),
        QStringLiteral("1024x1792"),
        QStringLiteral("1792x1024")
    });
    layout->addRow(tr("Size:"), m_sizeCombo);

    // Quality
    m_qualityCombo = new QComboBox(this);
    m_qualityCombo->addItems({
        QStringLiteral("standard"),
        QStringLiteral("hd")
    });
    layout->addRow(tr("Quality:"), m_qualityCombo);

    // Style
    m_styleCombo = new QComboBox(this);
    m_styleCombo->addItems({
        QStringLiteral("vivid"),
        QStringLiteral("natural")
    });
    layout->addRow(tr("Style:"), m_styleCombo);

    rootLayout->addLayout(layout);
    rootLayout->addStretch();

    auto connectCombo = [this](QComboBox* combo) {
        if (!combo) return;
        connect(combo, &QComboBox::currentTextChanged,
                this, &ImageGenPropertiesWidget::configChanged);
    };

    connectCombo(m_providerCombo);
    connectCombo(m_modelCombo);
    connectCombo(m_sizeCombo);
    connectCombo(m_qualityCombo);
    connectCombo(m_styleCombo);
}

QString ImageGenPropertiesWidget::provider() const
{
    return m_providerCombo ? m_providerCombo->currentText() : QString();
}

QString ImageGenPropertiesWidget::model() const
{
    return m_modelCombo ? m_modelCombo->currentText() : QString();
}

QString ImageGenPropertiesWidget::size() const
{
    return m_sizeCombo ? m_sizeCombo->currentText() : QString();
}

QString ImageGenPropertiesWidget::quality() const
{
    return m_qualityCombo ? m_qualityCombo->currentText() : QString();
}

QString ImageGenPropertiesWidget::style() const
{
    return m_styleCombo ? m_styleCombo->currentText() : QString();
}

void ImageGenPropertiesWidget::setProvider(const QString& providerName)
{
    if (setComboValue(m_providerCombo, providerName) < 0 && m_providerCombo && m_providerCombo->count() > 0) {
        QSignalBlocker blocker(m_providerCombo);
        m_providerCombo->setCurrentIndex(0);
    }
}

void ImageGenPropertiesWidget::setModel(const QString& modelName)
{
    setComboValue(m_modelCombo, modelName);
}

void ImageGenPropertiesWidget::setSize(const QString& sizeValue)
{
    setComboValue(m_sizeCombo, sizeValue);
}

void ImageGenPropertiesWidget::setQuality(const QString& qualityValue)
{
    setComboValue(m_qualityCombo, qualityValue);
}

void ImageGenPropertiesWidget::setStyle(const QString& styleValue)
{
    setComboValue(m_styleCombo, styleValue);
}

int ImageGenPropertiesWidget::setComboValue(QComboBox* combo, const QString& value)
{
    if (!combo) return -1;

    int index = combo->findText(value, Qt::MatchFixedString | Qt::MatchCaseSensitive);
    if (index < 0) {
        index = combo->findText(value, Qt::MatchFixedString);
    }

    if (index >= 0) {
        QSignalBlocker blocker(combo);
        combo->setCurrentIndex(index);
    }

    return index;
}
