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
#include "RagIndexerNode.h"
#include "RagIndexerPropertiesWidget.h"
#include "core/DocumentLoader.h"
#include "core/TextChunker.h"
#include "core/RagUtils.h"
#include "core/LLMProviderRegistry.h"
#include "backends/ILLMBackend.h"

#include <QtConcurrent>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QDebug>
#include <QUuid>
#include <QElapsedTimer>

RagIndexerNode::RagIndexerNode(QObject* parent)
    : QObject(parent)
{
}

NodeDescriptor RagIndexerNode::GetDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("rag_indexer");
    desc.name = QStringLiteral("RAG Indexer");
    desc.category = QStringLiteral("Persistence");

    // Input pins
    desc.inputPins.insert(QString::fromLatin1(kInputDirectoryPath), 
        PinDefinition{PinDirection::Input, QString::fromLatin1(kInputDirectoryPath), 
                     QStringLiteral("Directory"), QStringLiteral("text")});
    desc.inputPins.insert(QString::fromLatin1(kInputDatabasePath), 
        PinDefinition{PinDirection::Input, QString::fromLatin1(kInputDatabasePath), 
                     QStringLiteral("Database"), QStringLiteral("text")});
    desc.inputPins.insert(QString::fromLatin1(kInputMetadata), 
        PinDefinition{PinDirection::Input, QString::fromLatin1(kInputMetadata), 
                     QStringLiteral("Metadata"), QStringLiteral("text")});

    // Output pins - Database output first (top position)
    desc.outputPins.insert(QString::fromLatin1(kOutputDatabasePath), 
        PinDefinition{PinDirection::Output, QString::fromLatin1(kOutputDatabasePath), 
                     QStringLiteral("Database"), QStringLiteral("text")});
    desc.outputPins.insert(QString::fromLatin1(kOutputCount), 
        PinDefinition{PinDirection::Output, QString::fromLatin1(kOutputCount), 
                     QStringLiteral("Count"), QStringLiteral("text")});

    return desc;
}

QWidget* RagIndexerNode::createConfigurationWidget(QWidget* parent)
{
    auto* widget = new RagIndexerPropertiesWidget(parent);
    
    // Initialize widget with current state
    widget->setDirectoryPath(m_directoryPath);
    widget->setDatabasePath(m_databasePath);
    widget->setIndexMetadata(m_indexMetadata);
    widget->setProviderId(m_providerId);
    widget->setModelId(m_modelId);
    widget->setChunkSize(m_chunkSize);
    widget->setChunkOverlap(m_chunkOverlap);
    widget->setFileFilter(m_fileFilter);
    widget->setChunkingStrategy(m_chunkingStrategy);
    widget->setClearDatabase(m_clearDatabase);

    // Connect widget signals to node slots
    QObject::connect(widget, &RagIndexerPropertiesWidget::directoryPathChanged,
                     this, &RagIndexerNode::setDirectoryPath);
    QObject::connect(widget, &RagIndexerPropertiesWidget::databasePathChanged,
                     this, &RagIndexerNode::setDatabasePath);
    QObject::connect(widget, &RagIndexerPropertiesWidget::indexMetadataChanged,
                     this, &RagIndexerNode::setIndexMetadata);
    QObject::connect(widget, &RagIndexerPropertiesWidget::providerChanged,
                     this, &RagIndexerNode::setProviderId);
    QObject::connect(widget, &RagIndexerPropertiesWidget::modelChanged,
                     this, &RagIndexerNode::setModelId);
    QObject::connect(widget, &RagIndexerPropertiesWidget::chunkSizeChanged,
                     this, &RagIndexerNode::setChunkSize);
    QObject::connect(widget, &RagIndexerPropertiesWidget::chunkOverlapChanged,
                     this, &RagIndexerNode::setChunkOverlap);
    QObject::connect(widget, &RagIndexerPropertiesWidget::fileFilterChanged,
                     this, &RagIndexerNode::setFileFilter);
    QObject::connect(widget, &RagIndexerPropertiesWidget::chunkingStrategyChanged,
                     this, &RagIndexerNode::setChunkingStrategy);
    QObject::connect(widget, &RagIndexerPropertiesWidget::clearDatabaseChanged,
                     this, &RagIndexerNode::setClearDatabase);

    // Connect node signals back to widget for external updates
    QObject::connect(this, &RagIndexerNode::directoryPathChanged,
                     widget, &RagIndexerPropertiesWidget::setDirectoryPath);
    QObject::connect(this, &RagIndexerNode::databasePathChanged,
                     widget, &RagIndexerPropertiesWidget::setDatabasePath);
    QObject::connect(this, &RagIndexerNode::indexMetadataChanged,
                     widget, &RagIndexerPropertiesWidget::setIndexMetadata);
    QObject::connect(this, &RagIndexerNode::providerChanged,
                     widget, &RagIndexerPropertiesWidget::setProviderId);
    QObject::connect(this, &RagIndexerNode::modelChanged,
                     widget, &RagIndexerPropertiesWidget::setModelId);
    QObject::connect(this, &RagIndexerNode::chunkSizeChanged,
                     widget, &RagIndexerPropertiesWidget::setChunkSize);
    QObject::connect(this, &RagIndexerNode::chunkOverlapChanged,
                     widget, &RagIndexerPropertiesWidget::setChunkOverlap);
    QObject::connect(this, &RagIndexerNode::fileFilterChanged,
                     widget, &RagIndexerPropertiesWidget::setFileFilter);
    QObject::connect(this, &RagIndexerNode::chunkingStrategyChanged,
                     widget, &RagIndexerPropertiesWidget::setChunkingStrategy);
    QObject::connect(this, &RagIndexerNode::clearDatabaseChanged,
                     widget, &RagIndexerPropertiesWidget::setClearDatabase);

    return widget;
}

QFuture<DataPacket> RagIndexerNode::Execute(const DataPacket& inputs)
{
    return QtConcurrent::run([this, inputs]() -> DataPacket {
        DataPacket output;

        // Verbose logging is opt-in to avoid noisy debug output during
        // normal application use. Set CP_RAG_INDEXER_VERBOSE=1 in the
        // environment to re-enable detailed tracing of indexing steps.
        const bool verbose = qEnvironmentVariableIsSet("CP_RAG_INDEXER_VERBOSE");

        // Timer used to throttle progressUpdated emissions so that very large
        // indexing runs (e.g. tens of thousands of chunks) do not flood the UI.
        QElapsedTimer progressTimer;
        progressTimer.start();

        // Get input parameters - check if input pin has valid/non-empty data, otherwise use properties
        QString dirPath = inputs.value(QString::fromLatin1(kInputDirectoryPath)).toString();
        if (dirPath.trimmed().isEmpty()) {
            dirPath = m_directoryPath;
        }
        
        QString dbPath = inputs.value(QString::fromLatin1(kInputDatabasePath)).toString();
        if (dbPath.trimmed().isEmpty()) {
            dbPath = m_databasePath;
        }
        
        QString metadata = inputs.value(QString::fromLatin1(kInputMetadata)).toString();
        if (metadata.trimmed().isEmpty()) {
            metadata = m_indexMetadata;
        }

        // Validate inputs
        if (dirPath.isEmpty()) {
            qWarning() << "RagIndexerNode: Directory path is empty";
            output.insert(QString::fromLatin1(kOutputCount), QString::number(0));
            return output;
        }
        if (dbPath.isEmpty()) {
            qWarning() << "RagIndexerNode: Database path is empty";
            output.insert(QString::fromLatin1(kOutputCount), QString::number(0));
            return output;
        }
        if (m_providerId.isEmpty()) {
            qWarning() << "RagIndexerNode: Provider ID is empty";
            output.insert(QString::fromLatin1(kOutputCount), QString::number(0));
            return output;
        }
        if (m_modelId.isEmpty()) {
            qWarning() << "RagIndexerNode: Model ID is empty";
            output.insert(QString::fromLatin1(kOutputCount), QString::number(0));
            return output;
        }

        // Resolve credentials and backend via LLMProviderRegistry
        QString apiKey = LLMProviderRegistry::instance().getCredential(m_providerId);
        if (apiKey.isEmpty()) {
            qWarning() << "RagIndexerNode: No API key found for provider:" << m_providerId;
            output.insert(QString::fromLatin1(kOutputCount), QString::number(0));
            return output;
        }

        ILLMBackend* backend = LLMProviderRegistry::instance().getBackend(m_providerId);
        if (!backend) {
            qWarning() << "RagIndexerNode: Backend not found for provider:" << m_providerId;
            output.insert(QString::fromLatin1(kOutputCount), QString::number(0));
            return output;
        }

        if (verbose) {
            qDebug() << "RagIndexerNode: Using provider:" << m_providerId << "with model:" << m_modelId;
        }

        // Setup database connection with unique name
        QString connectionName = QStringLiteral("rag_indexer_") + QUuid::createUuid().toString();
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(dbPath);

        if (!db.open()) {
            qWarning() << "RagIndexerNode: Failed to open database:" << db.lastError().text();
            output.insert(QString::fromLatin1(kOutputCount), QString::number(0));
            // Reset handle before removing the connection to avoid
            // QSqlDatabasePrivate::removeDatabase "still in use" warnings.
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(connectionName);
            return output;
        }

        // Check if tables exist and create schema if needed
        {
            QSqlQuery checkQuery(db);
            
            // Enable foreign keys
            if (!checkQuery.exec(QString::fromLatin1(kRagSchemaPragma))) {
                qWarning() << "RagIndexerNode: Failed to enable foreign keys:" << checkQuery.lastError().text();
                db.close();
                db = QSqlDatabase();
                QSqlDatabase::removeDatabase(connectionName);
                output.insert(QString::fromLatin1(kOutputCount), QString::number(0));
                return output;
            }
            
            // Check if source_files table exists
            bool sourceFilesExists = false;
            if (checkQuery.exec(QStringLiteral("SELECT name FROM sqlite_master WHERE type='table' AND name='source_files'"))) {
                if (checkQuery.next()) {
                    sourceFilesExists = true;
                }
            } else {
                qWarning() << "RagIndexerNode: Failed to check source_files table existence:" << checkQuery.lastError().text();
            }
            
            // Create source_files table if it doesn't exist
            if (!sourceFilesExists) {
                if (!checkQuery.exec(QString::fromLatin1(kRagSchemaSourceFiles))) {
                    qWarning() << "RagIndexerNode: Failed to create source_files table:" << checkQuery.lastError().text();
                    db.close();
                    db = QSqlDatabase();
                    QSqlDatabase::removeDatabase(connectionName);
                    output.insert(QString::fromLatin1(kOutputCount), QString::number(0));
                    return output;
                }
            }
            
            // Check if fragments table exists
            bool fragmentsExists = false;
            if (checkQuery.exec(QStringLiteral("SELECT name FROM sqlite_master WHERE type='table' AND name='fragments'"))) {
                if (checkQuery.next()) {
                    fragmentsExists = true;
                }
            } else {
                qWarning() << "RagIndexerNode: Failed to check fragments table existence:" << checkQuery.lastError().text();
            }
            
            // Create fragments table if it doesn't exist
            if (!fragmentsExists) {
                if (!checkQuery.exec(QString::fromLatin1(kRagSchemaFragments))) {
                    qWarning() << "RagIndexerNode: Failed to create fragments table:" << checkQuery.lastError().text();
                    db.close();
                    db = QSqlDatabase();
                    QSqlDatabase::removeDatabase(connectionName);
                    output.insert(QString::fromLatin1(kOutputCount), QString::number(0));
                    return output;
                }
            }
        } // checkQuery goes out of scope here

        // Clear database if requested (after schema creation to ensure tables exist)
        if (m_clearDatabase) {
            if (!db.transaction()) {
                qWarning() << "RagIndexerNode: Failed to start transaction for clearing database:" << db.lastError().text();
            } else {
                QSqlQuery clearQuery(db);
                bool clearSuccess = true;
                
                // Delete all fragments
                if (!clearQuery.exec(QStringLiteral("DELETE FROM fragments"))) {
                    qWarning() << "RagIndexerNode: Failed to delete fragments:" << clearQuery.lastError().text();
                    clearSuccess = false;
                }
                
                // Delete all source files
                if (clearSuccess) {
                    if (!clearQuery.exec(QStringLiteral("DELETE FROM source_files"))) {
                        qWarning() << "RagIndexerNode: Failed to delete source_files:" << clearQuery.lastError().text();
                        clearSuccess = false;
                    }
                }
                
                // Reset AUTOINCREMENT counters
                if (clearSuccess) {
                    if (!clearQuery.exec(QStringLiteral("DELETE FROM sqlite_sequence WHERE name IN ('fragments', 'source_files')"))) {
                        qWarning() << "RagIndexerNode: Failed to reset AUTOINCREMENT counters:" << clearQuery.lastError().text();
                        clearSuccess = false;
                    }
                }
                
                if (!clearSuccess) {
                    if (!db.rollback()) {
                        qWarning() << "RagIndexerNode: Rollback failed:" << db.lastError().text();
                    }
                    db.close();
                    db = QSqlDatabase();
                    QSqlDatabase::removeDatabase(connectionName);
                    output.insert(QString::fromLatin1(kOutputCount), QString::number(0));
                    return output;
                } else {
                    if (!db.commit()) {
                        qWarning() << "RagIndexerNode: Failed to commit clear transaction:" << db.lastError().text();
                        db.close();
                        db = QSqlDatabase();
                        QSqlDatabase::removeDatabase(connectionName);
                        output.insert(QString::fromLatin1(kOutputCount), QString::number(0));
                        return output;
                    }
                }
            }
        }

        // Parse file filter into QStringList (semicolon-separated patterns like "*.cpp; *.h")
        QStringList nameFilters;
        if (!m_fileFilter.isEmpty()) {
            const QStringList patterns = m_fileFilter.split(QLatin1Char(';'));
            for (const QString& pattern : patterns) {
                const QString trimmed = pattern.trimmed();
                if (!trimmed.isEmpty()) {
                    nameFilters.append(trimmed);
                }
            }
        }
        
        // Scan directory for files (with optional name filters)
        QStringList files = DocumentLoader::scanDirectory(dirPath, nameFilters);
        if (verbose) {
            qDebug() << "RagIndexerNode: Found" << files.size() << "files in" << dirPath
                     << (nameFilters.isEmpty() ? "(no filter)" : QStringLiteral("(filter: %1)").arg(nameFilters.join(", ")));
        }

        if (files.isEmpty()) {
            qWarning() << "RagIndexerNode: No files found in directory";
            db.close();
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(connectionName);
            output.insert(QString::fromLatin1(kOutputCount), QString::number(0));
            output.insert(QString::fromLatin1(kOutputDatabasePath), dbPath);
            return output;
        }

        int totalChunks = 0;

        // Scope for database operations to ensure all QSqlQuery objects are destroyed before removeDatabase
        {
            // Start transaction for bulk insert
            if (!db.transaction()) {
                qWarning() << "RagIndexerNode: Failed to start transaction:" << db.lastError().text();
            }
            
            // Prepare queries for the new two-table schema
            QSqlQuery fileQuery(db);
            fileQuery.prepare(QStringLiteral(
                "INSERT OR REPLACE INTO source_files (file_path, provider, model, last_modified, metadata) "
                "VALUES (:file_path, :provider, :model, :last_modified, :metadata)"));
            
            QSqlQuery fileIdQuery(db);
            fileIdQuery.prepare(QStringLiteral(
                "SELECT id FROM source_files WHERE file_path = :file_path"));
            
            QSqlQuery fragmentQuery(db);
            fragmentQuery.prepare(QStringLiteral(
                "INSERT INTO fragments (file_id, chunk_index, content, embedding) "
                "VALUES (:file_id, :chunk_index, :content, :embedding)"));

            const int totalFiles = files.size();

            // Process each file
            int fileIndex = 0;
            for (const QString& filePath : files) {
                ++fileIndex;
                if (verbose) {
                    qDebug() << "RagIndexerNode: Processing file" << filePath;
                }

                // Read file content
                QString content = DocumentLoader::readTextFile(filePath);
                if (content.isEmpty()) {
                    if (verbose) {
                        qDebug() << "RagIndexerNode: Skipping empty file:" << filePath;
                    }
                    continue;
                }

                // Step 1: Register the source file with provider and model metadata
                fileQuery.bindValue(QStringLiteral(":file_path"), filePath);
                fileQuery.bindValue(QStringLiteral(":provider"), m_providerId);
                fileQuery.bindValue(QStringLiteral(":model"), m_modelId);
                fileQuery.bindValue(QStringLiteral(":last_modified"), QDateTime::currentSecsSinceEpoch());
                fileQuery.bindValue(QStringLiteral(":metadata"), metadata);
                
                if (!fileQuery.exec()) {
                    qWarning() << "RagIndexerNode: Failed to insert source file" << filePath 
                               << ":" << fileQuery.lastError().text();
                    continue;
                }

                // Step 2: Retrieve the file_id
                fileIdQuery.bindValue(QStringLiteral(":file_path"), filePath);
                if (!fileIdQuery.exec() || !fileIdQuery.next()) {
                    qWarning() << "RagIndexerNode: Failed to retrieve file_id for" << filePath 
                               << ":" << fileIdQuery.lastError().text();
                    continue;
                }
                qint64 fileId = fileIdQuery.value(0).toLongLong();

                // Detect file type
                FileType fileType = DocumentLoader::getFileTypeFromExtension(filePath);

                // Chunk the text
                QStringList chunks = TextChunker::split(content, m_chunkSize, m_chunkOverlap, fileType);
                if (verbose) {
                    qDebug() << "RagIndexerNode: Generated" << chunks.size() << "chunks for" << filePath;
                }

                int insertedForFile = 0;

                // Process each chunk
                const int chunkCountForFile = chunks.size();
                for (int i = 0; i < chunks.size(); ++i) {
                    const QString& chunk = chunks[i];

                    // Emit throttled progress updates for Stage Output so the
                    // user can see that indexing is still making progress.
                    if (progressTimer.elapsed() >= 10000) { // ~10 seconds
                        DataPacket progress;
                        progress.insert(QStringLiteral("progress"),
                            QStringLiteral("Indexing file %1 of %2\nChunk %3 of %4")
                                .arg(fileIndex)
                                .arg(totalFiles)
                                .arg(i + 1)
                                .arg(chunkCountForFile));
                        progress.insert(QStringLiteral("file_path"), filePath);
                        progress.insert(QStringLiteral("files_total"), totalFiles);
                        progress.insert(QStringLiteral("file_index"), fileIndex);
                        progress.insert(QStringLiteral("chunk_index"), i + 1);
                        progress.insert(QStringLiteral("chunks_in_file"), chunkCountForFile);
                        progress.insert(QStringLiteral("chunks_total_completed"), totalChunks);

                        emit progressUpdated(progress);
                        progressTimer.restart();
                    }

                    // Generate embedding
                    EmbeddingResult embResult = backend->getEmbedding(apiKey, m_modelId, chunk);
                    
                    if (embResult.hasError) {
                        qWarning() << "RagIndexerNode: Embedding error for chunk" << i << "of" << filePath 
                                   << ":" << embResult.errorMsg;
                        continue;
                    }

                    if (embResult.vector.empty()) {
                        qWarning() << "RagIndexerNode: Empty embedding vector for chunk" << i << "of" << filePath;
                        continue;
                    }

                    // Serialize embedding vector to BLOB
                    QByteArray embeddingBlob(reinterpret_cast<const char*>(embResult.vector.data()), 
                                            embResult.vector.size() * sizeof(float));

                    // Step 3: Insert fragment with file_id reference
                    fragmentQuery.bindValue(QStringLiteral(":file_id"), fileId);
                    fragmentQuery.bindValue(QStringLiteral(":chunk_index"), i);
                    fragmentQuery.bindValue(QStringLiteral(":content"), chunk);
                    fragmentQuery.bindValue(QStringLiteral(":embedding"), embeddingBlob);

                    if (!fragmentQuery.exec()) {
                        qWarning() << "RagIndexerNode: Failed to insert chunk" << i << "of" << filePath 
                                   << ":" << fragmentQuery.lastError().text();
                        continue;
                    }

                    totalChunks++;
                    ++insertedForFile;
                }
                
                if (verbose) {
                    qDebug() << "RagIndexerNode: Inserted" << insertedForFile << "fragments for" << filePath;
                }
            }

            // Commit transaction
            if (!db.commit()) {
                qWarning() << "RagIndexerNode: Failed to commit transaction:" << db.lastError().text();
                db.rollback();
            } else {
                if (verbose) {
                    qDebug() << "RagIndexerNode: Successfully indexed" << totalChunks << "chunks from" 
                             << files.size() << "files";
                }
            }
        } // All QSqlQuery objects (fileQuery, fileIdQuery, fragmentQuery) go out of scope here

        // Close database - now safe since all queries are destroyed
        db.close();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);

        // Set outputs
        output.insert(QString::fromLatin1(kOutputDatabasePath), dbPath);
        output.insert(QString::fromLatin1(kOutputCount), QString::number(totalChunks));

        return output;
    });
}

QJsonObject RagIndexerNode::saveState() const
{
    QJsonObject state;
    state.insert(QStringLiteral("directory_path"), m_directoryPath);
    state.insert(QStringLiteral("database_path"), m_databasePath);
    state.insert(QStringLiteral("index_metadata"), m_indexMetadata);
    state.insert(QStringLiteral("provider_id"), m_providerId);
    state.insert(QStringLiteral("model_id"), m_modelId);
    state.insert(QStringLiteral("chunk_size"), m_chunkSize);
    state.insert(QStringLiteral("chunk_overlap"), m_chunkOverlap);
    state.insert(QStringLiteral("file_filter"), m_fileFilter);
    state.insert(QStringLiteral("chunking_strategy"), m_chunkingStrategy);
    state.insert(QStringLiteral("clear_database"), m_clearDatabase);
    return state;
}

void RagIndexerNode::loadState(const QJsonObject& data)
{
    if (data.contains(QStringLiteral("directory_path"))) {
        m_directoryPath = data[QStringLiteral("directory_path")].toString();
    }
    if (data.contains(QStringLiteral("database_path"))) {
        m_databasePath = data[QStringLiteral("database_path")].toString();
    }
    if (data.contains(QStringLiteral("index_metadata"))) {
        m_indexMetadata = data[QStringLiteral("index_metadata")].toString();
    }
    if (data.contains(QStringLiteral("provider_id"))) {
        m_providerId = data[QStringLiteral("provider_id")].toString();
    }
    if (data.contains(QStringLiteral("model_id"))) {
        m_modelId = data[QStringLiteral("model_id")].toString();
    }
    if (data.contains(QStringLiteral("chunk_size"))) {
        m_chunkSize = data[QStringLiteral("chunk_size")].toInt();
    }
    if (data.contains(QStringLiteral("chunk_overlap"))) {
        m_chunkOverlap = data[QStringLiteral("chunk_overlap")].toInt();
    }
    if (data.contains(QStringLiteral("file_filter"))) {
        m_fileFilter = data[QStringLiteral("file_filter")].toString();
    }
    if (data.contains(QStringLiteral("chunking_strategy"))) {
        m_chunkingStrategy = data[QStringLiteral("chunking_strategy")].toString();
    }
    if (data.contains(QStringLiteral("clear_database"))) {
        m_clearDatabase = data[QStringLiteral("clear_database")].toBool();
    }
}

// Property setters
void RagIndexerNode::setDirectoryPath(const QString& path)
{
    if (m_directoryPath != path) {
        m_directoryPath = path;
        emit directoryPathChanged(path);
    }
}

void RagIndexerNode::setDatabasePath(const QString& path)
{
    if (m_databasePath != path) {
        m_databasePath = path;
        emit databasePathChanged(path);
    }
}

void RagIndexerNode::setIndexMetadata(const QString& metadata)
{
    if (m_indexMetadata != metadata) {
        m_indexMetadata = metadata;
        emit indexMetadataChanged(metadata);
    }
}

void RagIndexerNode::setProviderId(const QString& id)
{
    if (m_providerId != id) {
        m_providerId = id;
        emit providerChanged(id);
    }
}

void RagIndexerNode::setModelId(const QString& id)
{
    if (m_modelId != id) {
        m_modelId = id;
        emit modelChanged(id);
    }
}

void RagIndexerNode::setChunkSize(int size)
{
    if (m_chunkSize != size) {
        m_chunkSize = size;
        emit chunkSizeChanged(size);
    }
}

void RagIndexerNode::setChunkOverlap(int overlap)
{
    if (m_chunkOverlap != overlap) {
        m_chunkOverlap = overlap;
        emit chunkOverlapChanged(overlap);
    }
}

void RagIndexerNode::setFileFilter(const QString& filter)
{
    if (m_fileFilter != filter) {
        m_fileFilter = filter;
        emit fileFilterChanged(filter);
    }
}

void RagIndexerNode::setChunkingStrategy(const QString& strategy)
{
    if (m_chunkingStrategy != strategy) {
        m_chunkingStrategy = strategy;
        emit chunkingStrategyChanged(strategy);
    }
}

void RagIndexerNode::setClearDatabase(bool clear)
{
    if (m_clearDatabase != clear) {
        m_clearDatabase = clear;
        emit clearDatabaseChanged(clear);
    }
}
