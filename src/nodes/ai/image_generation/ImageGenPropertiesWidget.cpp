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
#include <QFont>
#include <QFormLayout>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QVBoxLayout>

namespace {

QString providerDisplayText(const ProviderCatalogEntry& entry)
{
    return QStringLiteral("%1 (%2)").arg(entry.name, entry.statusText);
}

void addSectionHeader(QComboBox* combo, const QString& text)
{
    combo->addItem(text, QString());
    if (auto* model = qobject_cast<QStandardItemModel*>(combo->model())) {
        if (auto* item = model->item(combo->count() - 1)) {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        }
    }
}

QString modelDisplayText(const ModelCatalogEntry& entry)
{
    QString text = entry.displayName.isEmpty() ? entry.id : entry.displayName;
    if (text != entry.id) {
        text += QStringLiteral(" [%1]").arg(entry.id);
    }
    if (!entry.driverProfileId.isEmpty()) {
        text += QStringLiteral(" - %1").arg(entry.driverProfileId);
    }
    return text;
}

void addModelGroup(QComboBox* combo,
                   const QList<ModelCatalogEntry>& entries,
                   ModelCatalogVisibility visibility,
                   const QString& header)
{
    bool hasGroup = false;
    for (const auto& entry : entries) {
        if (entry.visibility == visibility) {
            hasGroup = true;
            break;
        }
    }
    if (!hasGroup) {
        return;
    }

    addSectionHeader(combo, header);
    for (const auto& entry : entries) {
        if (entry.visibility == visibility) {
            combo->addItem(modelDisplayText(entry), entry.id);
        }
    }
}

} // namespace

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
    populateProviders();
    layout->addRow(tr("Provider:"), m_providerCombo);

    // Model
    m_modelCombo = new QComboBox(this);
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

    connect(m_providerCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &ImageGenPropertiesWidget::onProviderChanged);
    connect(&m_modelFetcher, &QFutureWatcher<QList<ModelCatalogEntry>>::finished,
            this, &ImageGenPropertiesWidget::onModelsFetched);

    if (m_providerCombo->count() > 0) {
        onProviderChanged(m_providerCombo->currentIndex());
    }
}

QString ImageGenPropertiesWidget::provider() const
{
    if (!m_providerCombo) {
        return {};
    }
    const QString providerId = m_providerCombo->currentData().toString();
    return providerId.isEmpty() ? m_providerCombo->currentText() : providerId;
}

QString ImageGenPropertiesWidget::model() const
{
    if (!m_modelCombo) {
        return {};
    }
    const QString modelId = m_modelCombo->currentData().toString();
    return modelId.isEmpty() ? m_modelCombo->currentText() : modelId;
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
    onProviderChanged(m_providerCombo ? m_providerCombo->currentIndex() : -1);
}

void ImageGenPropertiesWidget::setModel(const QString& modelName)
{
    m_pendingModelId = modelName;
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

    int index = combo->findData(value);
    if (index < 0) {
        index = combo->findText(value, Qt::MatchFixedString | Qt::MatchCaseSensitive);
    }
    if (index < 0) {
        index = combo->findText(value, Qt::MatchFixedString);
    }

    if (index >= 0) {
        QSignalBlocker blocker(combo);
        combo->setCurrentIndex(index);
    }

    return index;
}

void ImageGenPropertiesWidget::populateProviders()
{
    if (!m_providerCombo) {
        return;
    }

    const auto providers = ModelCatalogService::instance().providers(ModelCatalogKind::Image);
    for (const auto& provider : providers) {
        m_providerCombo->addItem(providerDisplayText(provider), provider.id);
        if (!provider.isUsable) {
            if (auto* model = qobject_cast<QStandardItemModel*>(m_providerCombo->model())) {
                if (auto* item = model->item(m_providerCombo->count() - 1)) {
                    item->setForeground(Qt::darkGray);
                }
            }
        }
    }
}

void ImageGenPropertiesWidget::populateModelCombo(const QList<ModelCatalogEntry>& models)
{
    if (!m_modelCombo) {
        return;
    }

    int selectedIndex = -1;
    {
        const QSignalBlocker blocker(m_modelCombo);
        m_modelCombo->clear();
        addModelGroup(m_modelCombo, models, ModelCatalogVisibility::Recommended, tr("Recommended"));
        addModelGroup(m_modelCombo, models, ModelCatalogVisibility::Available, tr("Available"));

        if (!m_pendingModelId.isEmpty()) {
            selectedIndex = m_modelCombo->findData(m_pendingModelId);
        }
        if (selectedIndex < 0) {
            for (int i = 0; i < m_modelCombo->count(); ++i) {
                if (!m_modelCombo->itemData(i).toString().isEmpty()) {
                    selectedIndex = i;
                    break;
                }
            }
        }
        if (selectedIndex >= 0) {
            m_modelCombo->setCurrentIndex(selectedIndex);
        }
    }

    m_modelCombo->setEnabled(selectedIndex >= 0);
    if (selectedIndex >= 0) {
        emit configChanged();
    }
}

void ImageGenPropertiesWidget::onProviderChanged(int index)
{
    if (index < 0 || !m_providerCombo || !m_modelCombo) {
        return;
    }

    const QString providerId = m_providerCombo->itemData(index).toString();
    {
        const QSignalBlocker blocker(m_modelCombo);
        m_modelCombo->clear();
        m_modelCombo->addItem(tr("Fetching..."), QString());
    }
    m_modelCombo->setEnabled(false);
    m_modelFetcher.setFuture(ModelCatalogService::instance().fetchModels(providerId, ModelCatalogKind::Image));
}

void ImageGenPropertiesWidget::onModelsFetched()
{
    const QString providerId = provider();
    m_lastModels = m_modelFetcher.result();
    if (m_lastModels.isEmpty()) {
        m_lastModels = ModelCatalogService::instance().fallbackModels(providerId, ModelCatalogKind::Image);
    }
    populateModelCombo(m_lastModels);
}
