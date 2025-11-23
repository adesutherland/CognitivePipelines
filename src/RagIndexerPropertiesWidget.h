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
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QCheckBox>

/**
 * @brief Properties widget for RagIndexerNode configuration.
 * 
 * Provides UI controls for:
 * - Input directory selection (with browse button)
 * - Database file path (with browse button)
 * - Index metadata (JSON string)
 * - Provider selection (via LLMProviderRegistry)
 * - Embedding model selection (dynamically populated)
 * - Chunk size and overlap parameters
 */
class RagIndexerPropertiesWidget : public QWidget {
    Q_OBJECT
public:
    explicit RagIndexerPropertiesWidget(QWidget* parent = nullptr);
    ~RagIndexerPropertiesWidget() override = default;

    // Getters
    QString directoryPath() const;
    QString databasePath() const;
    QString indexMetadata() const;
    QString providerId() const;
    QString modelId() const;
    int chunkSize() const;
    int chunkOverlap() const;
    QString fileFilter() const;
    QString chunkingStrategy() const;
    QString externalCommand() const;
    bool clearDatabase() const;

public slots:
    // Setters (for initializing from node state)
    void setDirectoryPath(const QString& path);
    void setDatabasePath(const QString& path);
    void setIndexMetadata(const QString& metadata);
    void setProviderId(const QString& id);
    void setModelId(const QString& id);
    void setChunkSize(int size);
    void setChunkOverlap(int overlap);
    void setFileFilter(const QString& filter);
    void setChunkingStrategy(const QString& strategy);
    void setExternalCommand(const QString& command);
    void setClearDatabase(bool clear);

signals:
    // Emitted when properties change
    void directoryPathChanged(const QString& path);
    void databasePathChanged(const QString& path);
    void indexMetadataChanged(const QString& metadata);
    void providerChanged(const QString& id);
    void modelChanged(const QString& id);
    void chunkSizeChanged(int size);
    void chunkOverlapChanged(int overlap);
    void fileFilterChanged(const QString& filter);
    void chunkingStrategyChanged(const QString& strategy);
    void externalCommandChanged(const QString& command);
    void clearDatabaseChanged(bool clear);

private slots:
    void onBrowseDirectory();
    void onBrowseDatabase();
    void onProviderChanged(int index);
    void onStrategyChanged(int index);

private:
    QLineEdit* m_directoryEdit {nullptr};
    QLineEdit* m_databaseEdit {nullptr};
    QLineEdit* m_metadataEdit {nullptr};
    QComboBox* m_providerCombo {nullptr};
    QComboBox* m_modelCombo {nullptr};
    QSpinBox* m_chunkSizeSpinBox {nullptr};
    QSpinBox* m_chunkOverlapSpinBox {nullptr};
    QPushButton* m_browseDirectoryBtn {nullptr};
    QPushButton* m_browseDatabaseBtn {nullptr};
    QLineEdit* m_fileFilterEdit {nullptr};
    QComboBox* m_chunkingStrategyCombo {nullptr};
    QLineEdit* m_externalCommandEdit {nullptr};
    QCheckBox* m_clearDatabaseCheckBox {nullptr};
};
