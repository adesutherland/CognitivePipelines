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
#include "core/LLMProviderRegistry.h"
#include "backends/ILLMBackend.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QSignalBlocker>
#include <QComboBox>

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
    
    // Populate providers from LLMProviderRegistry
    const auto backends = LLMProviderRegistry::instance().allBackends();
    for (ILLMBackend* backend : backends) {
        if (backend) {
            m_providerCombo->addItem(backend->name(), backend->id());
        }
    }
    formLayout->addRow(QStringLiteral("Provider:"), m_providerCombo);

    // Model combo box (will be populated when provider is selected)
    m_modelCombo = new QComboBox(this);
    formLayout->addRow(QStringLiteral("Embedding Model:"), m_modelCombo);

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
        if (m_modelCombo->currentIndex() >= 0) {
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

void RagIndexerPropertiesWidget::onProviderChanged(int index)
{
    if (index < 0 || !m_modelCombo) {
        return;
    }

    const QString providerId = m_providerCombo->itemData(index).toString();
    
    // Clear existing models and repopulate
    {
        const QSignalBlocker blocker(m_modelCombo);
        m_modelCombo->clear();

        // Get backend and populate embedding models
        ILLMBackend* backend = LLMProviderRegistry::instance().getBackend(providerId);
        if (backend) {
            const QStringList models = backend->availableEmbeddingModels();
            for (const QString& model : models) {
                m_modelCombo->addItem(model, model);
            }
        }
        
        // Set default model to first item if available
        if (m_modelCombo->count() > 0) {
            m_modelCombo->setCurrentIndex(0);
        }
    } // QSignalBlocker goes out of scope here

    // Emit provider changed signal
    emit providerChanged(providerId);
    
    // Explicitly emit modelChanged signal with the new default model
    // This is crucial because QSignalBlocker prevented the automatic signal
    if (m_modelCombo->count() > 0 && m_modelCombo->currentIndex() >= 0) {
        emit modelChanged(m_modelCombo->currentData().toString());
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
