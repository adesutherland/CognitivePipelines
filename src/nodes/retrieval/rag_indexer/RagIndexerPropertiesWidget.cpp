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
#include "RagIndexerPropertiesWidget.h"
#include "ai/catalog/ModelCatalogService.h"

#include <QFormLayout>
#include <QFont>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QSignalBlocker>
#include <QComboBox>
#include <QStandardItemModel>

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
    if (!entry.driverProfileId.isEmpty()) {
        text += QStringLiteral(" - %1").arg(entry.driverProfileId);
    }
    if (entry.visibility == ModelCatalogVisibility::Hidden && !entry.filterReason.isEmpty()) {
        text += QStringLiteral(" - Investigate: %1").arg(entry.filterReason);
    } else if (!entry.description.isEmpty()) {
        text += QStringLiteral(" - %1").arg(entry.description);
    }
    return text;
}

void addModelEntry(QComboBox* combo, const ModelCatalogEntry& entry)
{
    combo->addItem(modelDisplayText(entry), entry.id);
    if (entry.visibility == ModelCatalogVisibility::Hidden) {
        if (auto* model = qobject_cast<QStandardItemModel*>(combo->model())) {
            if (auto* item = model->item(combo->count() - 1)) {
                item->setForeground(Qt::darkGray);
            }
        }
    }
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
            addModelEntry(combo, entry);
        }
    }
}

} // namespace

RagIndexerPropertiesWidget::RagIndexerPropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    auto* formLayout = new QFormLayout();

    // Directory path with browse button
    m_directoryEdit = new QLineEdit(this);
    m_browseDirectoryBtn = new QPushButton(QStringLiteral("Browse..."), this);
    auto* dirLayout = new QHBoxLayout();
    dirLayout->addWidget(m_directoryEdit);
    dirLayout->addWidget(m_browseDirectoryBtn);
    formLayout->addRow(QStringLiteral("Input Directory:"), dirLayout);

    // Database path with browse button
    m_databaseEdit = new QLineEdit(this);
    m_browseDatabaseBtn = new QPushButton(QStringLiteral("Browse..."), this);
    auto* dbLayout = new QHBoxLayout();
    dbLayout->addWidget(m_databaseEdit);
    dbLayout->addWidget(m_browseDatabaseBtn);
    formLayout->addRow(QStringLiteral("Database File:"), dbLayout);

    // Index metadata
    m_metadataEdit = new QLineEdit(this);
    m_metadataEdit->setPlaceholderText(QStringLiteral("{\"source\": \"user\"}"));
    formLayout->addRow(QStringLiteral("Metadata (JSON):"), m_metadataEdit);

    // Provider combo box
    m_providerCombo = new QComboBox(this);
    
    const auto providers = ModelCatalogService::instance().providers(ModelCatalogKind::Embedding);
    for (const auto& provider : providers) {
        m_providerCombo->addItem(providerDisplayText(provider), provider.id);
    }
    formLayout->addRow(QStringLiteral("Provider:"), m_providerCombo);

    // Model combo box (will be populated when provider is selected)
    m_modelCombo = new QComboBox(this);
    formLayout->addRow(QStringLiteral("Embedding Model:"), m_modelCombo);

    m_showFilteredCheck = new QCheckBox(QStringLiteral("Show filtered models"), this);
    m_showFilteredCheck->setToolTip(QStringLiteral("Show provider models hidden by embedding capability or driver rules."));
    formLayout->addRow(QString(), m_showFilteredCheck);

    auto* testLayout = new QHBoxLayout();
    m_testModelButton = new QPushButton(QStringLiteral("Test Selection"), this);
    m_testStatusLabel = new QLabel(this);
    m_testStatusLabel->setWordWrap(true);
    testLayout->addWidget(m_testModelButton);
    testLayout->addWidget(m_testStatusLabel, 1);
    formLayout->addRow(QString(), testLayout);

    // Chunk size
    m_chunkSizeSpinBox = new QSpinBox(this);
    m_chunkSizeSpinBox->setRange(100, 10000);
    m_chunkSizeSpinBox->setValue(1000);
    m_chunkSizeSpinBox->setSuffix(QStringLiteral(" chars"));
    formLayout->addRow(QStringLiteral("Chunk Size:"), m_chunkSizeSpinBox);

    // Chunk overlap
    m_chunkOverlapSpinBox = new QSpinBox(this);
    m_chunkOverlapSpinBox->setRange(0, 1000);
    m_chunkOverlapSpinBox->setValue(200);
    m_chunkOverlapSpinBox->setSuffix(QStringLiteral(" chars"));
    formLayout->addRow(QStringLiteral("Chunk Overlap:"), m_chunkOverlapSpinBox);

    // File filter
    m_fileFilterEdit = new QLineEdit(this);
    m_fileFilterEdit->setPlaceholderText(QStringLiteral("*.cpp; *.h"));
    formLayout->addRow(QStringLiteral("File Filter:"), m_fileFilterEdit);

    // Chunking strategy
    m_chunkingStrategyCombo = new QComboBox(this);
    m_chunkingStrategyCombo->addItem(QStringLiteral("Auto"));
    m_chunkingStrategyCombo->addItem(QStringLiteral("Plain Text"));
    m_chunkingStrategyCombo->addItem(QStringLiteral("Markdown"));
    m_chunkingStrategyCombo->addItem(QStringLiteral("C++"));
    m_chunkingStrategyCombo->addItem(QStringLiteral("Python"));
    m_chunkingStrategyCombo->addItem(QStringLiteral("Rexx"));
    m_chunkingStrategyCombo->addItem(QStringLiteral("SQL"));
    m_chunkingStrategyCombo->addItem(QStringLiteral("Cobol"));
    formLayout->addRow(QStringLiteral("Chunking Strategy:"), m_chunkingStrategyCombo);

    // Clear database checkbox
    m_clearDatabaseCheckBox = new QCheckBox(QStringLiteral("Clear Database before Indexing"), this);
    m_clearDatabaseCheckBox->setChecked(false);  // Default to false
    formLayout->addRow(QStringLiteral(""), m_clearDatabaseCheckBox);

    mainLayout->addLayout(formLayout);
    mainLayout->addStretch();

    // Connect browse buttons
    connect(m_browseDirectoryBtn, &QPushButton::clicked, this, &RagIndexerPropertiesWidget::onBrowseDirectory);
    connect(m_browseDatabaseBtn, &QPushButton::clicked, this, &RagIndexerPropertiesWidget::onBrowseDatabase);

    // Connect line edits to emit signals on change
    connect(m_directoryEdit, &QLineEdit::textChanged, this, &RagIndexerPropertiesWidget::directoryPathChanged);
    connect(m_databaseEdit, &QLineEdit::textChanged, this, &RagIndexerPropertiesWidget::databasePathChanged);
    connect(m_metadataEdit, &QLineEdit::textChanged, this, &RagIndexerPropertiesWidget::indexMetadataChanged);

    // Connect provider combo box
    connect(m_providerCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &RagIndexerPropertiesWidget::onProviderChanged);
    
    // Connect model combo box
    connect(m_modelCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        if (m_modelCombo->currentIndex() >= 0 && !m_modelCombo->currentData().toString().isEmpty()) {
            if (m_testModelButton) {
                m_testModelButton->setEnabled(true);
            }
            if (m_testStatusLabel && !m_modelTester.isRunning()) {
                m_testStatusLabel->clear();
            }
            emit modelChanged(m_modelCombo->currentData().toString());
        }
    });

    // Connect spin boxes
    connect(m_chunkSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RagIndexerPropertiesWidget::chunkSizeChanged);
    connect(m_chunkOverlapSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RagIndexerPropertiesWidget::chunkOverlapChanged);

    // Connect new controls
    connect(m_fileFilterEdit, &QLineEdit::textChanged, this, &RagIndexerPropertiesWidget::fileFilterChanged);
    connect(m_chunkingStrategyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RagIndexerPropertiesWidget::onStrategyChanged);
    connect(m_clearDatabaseCheckBox, &QCheckBox::toggled, this, &RagIndexerPropertiesWidget::clearDatabaseChanged);
    connect(m_showFilteredCheck, &QCheckBox::toggled, this, &RagIndexerPropertiesWidget::onShowFilteredChanged);
    connect(m_testModelButton, &QPushButton::clicked, this, &RagIndexerPropertiesWidget::onTestModelClicked);
    connect(&m_modelTester, &QFutureWatcher<ModelTestResult>::finished,
            this, &RagIndexerPropertiesWidget::onModelTestFinished);
    connect(&m_modelFetcher, &QFutureWatcher<QList<ModelCatalogEntry>>::finished,
            this, &RagIndexerPropertiesWidget::onModelsFetched);

    // Initialize model list for the first provider
    if (m_providerCombo->count() > 0) {
        onProviderChanged(0);
    }
}

// Getters
QString RagIndexerPropertiesWidget::directoryPath() const
{
    return m_directoryEdit->text();
}

QString RagIndexerPropertiesWidget::databasePath() const
{
    return m_databaseEdit->text();
}

QString RagIndexerPropertiesWidget::indexMetadata() const
{
    return m_metadataEdit->text();
}

QString RagIndexerPropertiesWidget::providerId() const
{
    if (m_providerCombo->currentIndex() >= 0) {
        return m_providerCombo->currentData().toString();
    }
    return QString();
}

QString RagIndexerPropertiesWidget::modelId() const
{
    if (m_modelCombo->currentIndex() >= 0) {
        return m_modelCombo->currentData().toString();
    }
    return QString();
}

int RagIndexerPropertiesWidget::chunkSize() const
{
    return m_chunkSizeSpinBox->value();
}

int RagIndexerPropertiesWidget::chunkOverlap() const
{
    return m_chunkOverlapSpinBox->value();
}

QString RagIndexerPropertiesWidget::fileFilter() const
{
    return m_fileFilterEdit->text();
}

QString RagIndexerPropertiesWidget::chunkingStrategy() const
{
    return m_chunkingStrategyCombo->currentText();
}

bool RagIndexerPropertiesWidget::clearDatabase() const
{
    return m_clearDatabaseCheckBox->isChecked();
}

// Setters
void RagIndexerPropertiesWidget::setDirectoryPath(const QString& path)
{
    if (m_directoryEdit->text() != path) {
        m_directoryEdit->blockSignals(true);
        m_directoryEdit->setText(path);
        m_directoryEdit->blockSignals(false);
    }
}

void RagIndexerPropertiesWidget::setDatabasePath(const QString& path)
{
    if (m_databaseEdit->text() != path) {
        m_databaseEdit->blockSignals(true);
        m_databaseEdit->setText(path);
        m_databaseEdit->blockSignals(false);
    }
}

void RagIndexerPropertiesWidget::setIndexMetadata(const QString& metadata)
{
    if (m_metadataEdit->text() != metadata) {
        m_metadataEdit->blockSignals(true);
        m_metadataEdit->setText(metadata);
        m_metadataEdit->blockSignals(false);
    }
}

void RagIndexerPropertiesWidget::setProviderId(const QString& id)
{
    // Find the index with matching data
    for (int i = 0; i < m_providerCombo->count(); ++i) {
        if (m_providerCombo->itemData(i).toString() == id) {
            if (m_providerCombo->currentIndex() != i) {
                m_providerCombo->setCurrentIndex(i);
            }
            return;
        }
    }
}

void RagIndexerPropertiesWidget::setModelId(const QString& id)
{
    m_pendingModelId = id;
    // Find the index with matching data
    for (int i = 0; i < m_modelCombo->count(); ++i) {
        if (m_modelCombo->itemData(i).toString() == id) {
            if (m_modelCombo->currentIndex() != i) {
                const QSignalBlocker blocker(m_modelCombo);
                m_modelCombo->setCurrentIndex(i);
            }
            return;
        }
    }
}

void RagIndexerPropertiesWidget::setChunkSize(int size)
{
    if (m_chunkSizeSpinBox->value() != size) {
        m_chunkSizeSpinBox->blockSignals(true);
        m_chunkSizeSpinBox->setValue(size);
        m_chunkSizeSpinBox->blockSignals(false);
    }
}

void RagIndexerPropertiesWidget::setChunkOverlap(int overlap)
{
    if (m_chunkOverlapSpinBox->value() != overlap) {
        m_chunkOverlapSpinBox->blockSignals(true);
        m_chunkOverlapSpinBox->setValue(overlap);
        m_chunkOverlapSpinBox->blockSignals(false);
    }
}

void RagIndexerPropertiesWidget::setFileFilter(const QString& filter)
{
    if (m_fileFilterEdit->text() != filter) {
        m_fileFilterEdit->blockSignals(true);
        m_fileFilterEdit->setText(filter);
        m_fileFilterEdit->blockSignals(false);
    }
}

void RagIndexerPropertiesWidget::setChunkingStrategy(const QString& strategy)
{
    // Find the index with matching text
    for (int i = 0; i < m_chunkingStrategyCombo->count(); ++i) {
        if (m_chunkingStrategyCombo->itemText(i) == strategy) {
            if (m_chunkingStrategyCombo->currentIndex() != i) {
                m_chunkingStrategyCombo->blockSignals(true);
                m_chunkingStrategyCombo->setCurrentIndex(i);
                m_chunkingStrategyCombo->blockSignals(false);
                // Update external command enabled state
                onStrategyChanged(i);
            }
            return;
        }
    }
}

void RagIndexerPropertiesWidget::setClearDatabase(bool clear)
{
    if (m_clearDatabaseCheckBox->isChecked() != clear) {
        m_clearDatabaseCheckBox->blockSignals(true);
        m_clearDatabaseCheckBox->setChecked(clear);
        m_clearDatabaseCheckBox->blockSignals(false);
    }
}

// Browse handlers
void RagIndexerPropertiesWidget::onBrowseDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("Select Input Directory"),
        m_directoryEdit->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (!dir.isEmpty()) {
        m_directoryEdit->setText(dir);
    }
}

void RagIndexerPropertiesWidget::onBrowseDatabase()
{
    QString file = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Select Database File"),
        m_databaseEdit->text(),
        QStringLiteral("SQLite Database (*.db *.sqlite);;All Files (*)")
    );

    if (!file.isEmpty()) {
        m_databaseEdit->setText(file);
    }
}

void RagIndexerPropertiesWidget::populateModelCombo(const QList<ModelCatalogEntry>& models)
{
    if (!m_modelCombo) {
        return;
    }

    const QString desiredModelId = !m_pendingModelId.isEmpty()
                                       ? m_pendingModelId
                                       : m_modelCombo->currentData().toString();
    bool pendingIsHidden = false;
    for (const auto& model : models) {
        if (model.id == desiredModelId && model.visibility == ModelCatalogVisibility::Hidden) {
            pendingIsHidden = true;
            break;
        }
    }

    const bool includeHidden = (m_showFilteredCheck && m_showFilteredCheck->isChecked()) || pendingIsHidden;
    int selectedIndex = -1;
    bool selectedFound = false;
    {
        const QSignalBlocker blocker(m_modelCombo);
        m_modelCombo->clear();

        addModelGroup(m_modelCombo, models, ModelCatalogVisibility::Recommended, QStringLiteral("Recommended"));
        addModelGroup(m_modelCombo, models, ModelCatalogVisibility::Available, QStringLiteral("Available"));
        if (includeHidden) {
            addModelGroup(m_modelCombo, models, ModelCatalogVisibility::Hidden, QStringLiteral("Investigate"));
        }

        if (!desiredModelId.isEmpty()) {
            selectedIndex = m_modelCombo->findData(desiredModelId);
            selectedFound = selectedIndex >= 0;
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
    if (m_testModelButton) {
        m_testModelButton->setEnabled(selectedIndex >= 0);
    }

    if (selectedIndex >= 0
        && m_modelCombo->currentIndex() >= 0
        && !m_modelCombo->currentData().toString().isEmpty()) {
        if (selectedFound && !m_pendingModelId.isEmpty()) {
            const QSignalBlocker blocker(this);
            emit modelChanged(m_modelCombo->currentData().toString());
        } else {
            emit modelChanged(m_modelCombo->currentData().toString());
        }
    }
}

void RagIndexerPropertiesWidget::onProviderChanged(int index)
{
    if (index < 0 || !m_modelCombo) {
        return;
    }

    const QString providerId = m_providerCombo->itemData(index).toString();
    if (m_testStatusLabel) {
        m_testStatusLabel->clear();
    }
    if (m_testModelButton) {
        m_testModelButton->setEnabled(false);
    }
    m_pendingModelId.clear();
    {
        const QSignalBlocker blocker(m_modelCombo);
        m_modelCombo->clear();
        m_modelCombo->addItem(QStringLiteral("Fetching..."));
    }
    m_modelCombo->setEnabled(false);
    m_modelFetcher.setFuture(ModelCatalogService::instance().fetchModels(providerId, ModelCatalogKind::Embedding));

    // Emit provider changed signal
    emit providerChanged(providerId);
}

void RagIndexerPropertiesWidget::onModelsFetched()
{
    const QString selectedProviderId = providerId();
    m_lastModels = m_modelFetcher.result();
    if (m_lastModels.isEmpty()) {
        m_lastModels = ModelCatalogService::instance().fallbackModels(selectedProviderId, ModelCatalogKind::Embedding);
    }
    populateModelCombo(m_lastModels);
}

void RagIndexerPropertiesWidget::onShowFilteredChanged(bool checked)
{
    Q_UNUSED(checked)
    m_pendingModelId = m_modelCombo ? m_modelCombo->currentData().toString() : QString();
    populateModelCombo(m_lastModels);
}

void RagIndexerPropertiesWidget::onTestModelClicked()
{
    if (m_modelTester.isRunning()) {
        return;
    }

    const QString selectedProviderId = this->providerId();
    const QString selectedModelId = this->modelId();
    if (selectedProviderId.isEmpty() || selectedModelId.isEmpty()) {
        return;
    }

    if (m_testModelButton) {
        m_testModelButton->setEnabled(false);
    }
    if (m_testStatusLabel) {
        m_testStatusLabel->setText(QStringLiteral("Testing..."));
    }

    m_modelTester.setFuture(ModelCatalogService::instance().testModel(selectedProviderId,
                                                                      selectedModelId,
                                                                      ModelCatalogKind::Embedding));
}

void RagIndexerPropertiesWidget::onModelTestFinished()
{
    const ModelTestResult result = m_modelTester.result();
    if (m_testModelButton) {
        m_testModelButton->setEnabled(m_modelCombo && !m_modelCombo->currentData().toString().isEmpty());
    }
    if (m_testStatusLabel) {
        const QString driver = result.driverProfileId.isEmpty()
                                   ? QString()
                                   : QStringLiteral(" [%1]").arg(result.driverProfileId);
        m_testStatusLabel->setText(result.success
                                       ? QStringLiteral("OK%1: %2").arg(driver, result.message)
                                       : QStringLiteral("Failed%1: %2").arg(driver, result.message));
    }
}

void RagIndexerPropertiesWidget::onStrategyChanged(int index)
{
    if (!m_chunkingStrategyCombo) {
        return;
    }
    
    // Emit signal for strategy change
    const QString strategy = m_chunkingStrategyCombo->itemText(index);
    emit chunkingStrategyChanged(strategy);
}
