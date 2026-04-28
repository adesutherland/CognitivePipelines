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
#include "retrieval/documents/DocumentLoader.h"
#include "retrieval/chunking/TextChunker.h"
#include "retrieval/storage/RagUtils.h"
#include "ai/registry/LLMProviderRegistry.h"
#include "ai/backends/ILLMBackend.h"
#include "ai/catalog/ModelCatalogService.h"
#include "ModelCapsRegistry.h"

#include <QtConcurrent>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include "Logger.h"
#include <QUuid>
#include <QElapsedTimer>
#include <QVector>

namespace {

struct ChunkLineRange {
    int startLine {0};
    int endLine {0};
};

FileType fileTypeForChunkingStrategy(const QString& strategy, const QString& filePath)
{
    const QString normalized = strategy.trimmed().toLower();
    if (normalized.isEmpty() || normalized == QStringLiteral("auto")) {
        return DocumentLoader::getFileTypeFromExtension(filePath);
    }
    if (normalized == QStringLiteral("plain text")) {
        return FileType::PlainText;
    }
    if (normalized == QStringLiteral("markdown")) {
        return FileType::CodeMarkdown;
    }
    if (normalized == QStringLiteral("c++")) {
        return FileType::CodeCpp;
    }
    if (normalized == QStringLiteral("python")) {
        return FileType::CodePython;
    }
    if (normalized == QStringLiteral("rexx")) {
        return FileType::CodeRexx;
    }
    if (normalized == QStringLiteral("sql")) {
        return FileType::CodeSql;
    }
    if (normalized == QStringLiteral("cobol")) {
        return FileType::CodeCobol;
    }
    return DocumentLoader::getFileTypeFromExtension(filePath);
}

int lineNumberForOffset(const QString& text, int offset)
{
    if (text.isEmpty()) {
        return 0;
    }
    const int safeOffset = qBound(0, offset, text.size() - 1);
    return text.left(safeOffset).count(QLatin1Char('\n')) + 1;
}

QVector<ChunkLineRange> calculateChunkLineRanges(const QString& content, const QStringList& chunks)
{
    QVector<ChunkLineRange> ranges;
    ranges.reserve(chunks.size());

    int searchFrom = 0;
    int previousStartLine = 1;
    int previousEndLine = 1;

    for (const QString& chunk : chunks) {
        ChunkLineRange range;
        if (chunk.isEmpty() || content.isEmpty()) {
            ranges.append(range);
            continue;
        }

        const int lookupStart = qMax(0, searchFrom - 1024);
        int found = content.indexOf(chunk, lookupStart);
        if (found < 0) {
            const QString prefix = chunk.left(qMin(160, chunk.size()));
            if (!prefix.isEmpty()) {
                found = content.indexOf(prefix, lookupStart);
            }
        }

        if (found < 0) {
            range.startLine = previousStartLine;
            range.endLine = previousEndLine;
        } else {
            const int endOffset = qMax(found, found + chunk.size() - 1);
            range.startLine = lineNumberForOffset(content, found);
            range.endLine = lineNumberForOffset(content, endOffset);
            previousStartLine = range.startLine;
            previousEndLine = range.endLine;
            searchFrom = found + 1;
        }

        ranges.append(range);
    }

    return ranges;
}

bool ensureFragmentLineColumns(QSqlDatabase& db)
{
    QSqlQuery pragmaQuery(db);
    bool hasStartLine = false;
    bool hasEndLine = false;
    if (!pragmaQuery.exec(QStringLiteral("PRAGMA table_info(fragments)"))) {
        CP_WARN << "RagIndexerNode: Failed to inspect fragments table columns:" << pragmaQuery.lastError().text();
        return false;
    }

    while (pragmaQuery.next()) {
        const QString column = pragmaQuery.value(1).toString();
        hasStartLine = hasStartLine || column == QStringLiteral("start_line");
        hasEndLine = hasEndLine || column == QStringLiteral("end_line");
    }

    QSqlQuery alterQuery(db);
    if (!hasStartLine
        && !alterQuery.exec(QStringLiteral("ALTER TABLE fragments ADD COLUMN start_line INTEGER"))) {
        CP_WARN << "RagIndexerNode: Failed to add fragments.start_line:" << alterQuery.lastError().text();
        return false;
    }
    if (!hasEndLine
        && !alterQuery.exec(QStringLiteral("ALTER TABLE fragments ADD COLUMN end_line INTEGER"))) {
        CP_WARN << "RagIndexerNode: Failed to add fragments.end_line:" << alterQuery.lastError().text();
        return false;
    }

    return true;
}

} // namespace

RagIndexerNode::RagIndexerNode(QObject* parent)
    : QObject(parent)
{
    const QString providerId = ModelCatalogService::instance().defaultProvider(ModelCatalogKind::Embedding);
    if (!providerId.isEmpty()) {
        m_providerId = providerId;
        const auto models = ModelCatalogService::instance().fallbackModels(providerId, ModelCatalogKind::Embedding);
        for (const auto& model : models) {
            if (model.visibility != ModelCatalogVisibility::Hidden) {
                m_modelId = model.id;
                break;
            }
        }
    }
}

NodeDescriptor RagIndexerNode::getDescriptor() const
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
    QObject::connect(this, &RagIndexerNode::statusChanged,
                     widget, &RagIndexerPropertiesWidget::setStatusMessage);

    return widget;
}

TokenList RagIndexerNode::execute(const TokenList& incomingTokens)
{
    // Merge incoming tokens into a single DataPacket to preserve the
    // existing Execute(DataPacket) implementation.
    DataPacket inputs;
    for (const auto& token : incomingTokens) {
        for (auto it = token.data.cbegin(); it != token.data.cend(); ++it) {
            inputs.insert(it.key(), it.value());
        }
    }

    QFuture<DataPacket> fut = Execute(inputs);
    const DataPacket out = fut.result();

    ExecutionToken token;
    token.data = out;

    TokenList result;
    result.push_back(std::move(token));
    return result;
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

        output.insert(QString::fromLatin1(kOutputCount), QString::number(0));
        if (!dbPath.isEmpty()) {
            output.insert(QString::fromLatin1(kOutputDatabasePath), dbPath);
        }
        output.insert(QStringLiteral("_provider"), m_providerId);
        output.insert(QStringLiteral("_model"), m_modelId);
        if (const auto resolvedRule = ModelCapsRegistry::instance().resolveWithRule(m_modelId, m_providerId);
            resolvedRule.has_value() && !resolvedRule->driverProfileId.isEmpty()) {
            output.insert(QStringLiteral("_driver"), resolvedRule->driverProfileId);
        }

        auto fail = [this, &output](const QString& message) {
            output.insert(QString::fromLatin1(kOutputCount), QString::number(0));
            output.insert(QStringLiteral("__error"), message);
            emit statusChanged(QStringLiteral("Status: %1").arg(message));
            return output;
        };

        emit statusChanged(QStringLiteral("Status: preparing index run..."));

        // Validate inputs
        if (dirPath.isEmpty()) {
            const QString msg = QStringLiteral("RAG Indexer directory path is empty.");
            CP_WARN << msg;
            return fail(msg);
        }
        if (dbPath.isEmpty()) {
            const QString msg = QStringLiteral("RAG Indexer database path is empty.");
            CP_WARN << msg;
            return fail(msg);
        }
        if (m_providerId.isEmpty()) {
            const QString msg = QStringLiteral("RAG Indexer provider ID is empty.");
            CP_WARN << msg;
            return fail(msg);
        }
        if (m_modelId.isEmpty()) {
            const QString msg = QStringLiteral("RAG Indexer model ID is empty.");
            CP_WARN << msg;
            return fail(msg);
        }

        // Resolve credentials and backend via LLMProviderRegistry
        QString apiKey = LLMProviderRegistry::instance().getCredential(m_providerId);
        if (apiKey.isEmpty() && ModelCatalogService::providerRequiresCredential(m_providerId)) {
            const QString msg = QStringLiteral("No API key found for provider '%1'.").arg(m_providerId);
            CP_WARN << "RagIndexerNode:" << msg;
            return fail(msg);
        }

        ILLMBackend* backend = LLMProviderRegistry::instance().getBackend(m_providerId);
        if (!backend) {
            const QString msg = QStringLiteral("Backend not found for provider '%1'.").arg(m_providerId);
            CP_WARN << "RagIndexerNode:" << msg;
            return fail(msg);
        }

        if (verbose) {
            CP_LOG << "RagIndexerNode: Using provider:" << m_providerId << "with model:" << m_modelId;
        }

        // Setup database connection with unique name
        QString connectionName = QStringLiteral("rag_indexer_") + QUuid::createUuid().toString();
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(dbPath);

        if (!db.open()) {
            const QString msg = QStringLiteral("Failed to open RAG database: %1").arg(db.lastError().text());
            CP_WARN << "RagIndexerNode:" << msg;
            // Reset handle before removing the connection to avoid
            // QSqlDatabasePrivate::removeDatabase "still in use" warnings.
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(connectionName);
            return fail(msg);
        }

        // Check if tables exist and create schema if needed
        {
            QSqlQuery checkQuery(db);
            
            // Enable foreign keys
            if (!checkQuery.exec(QString::fromLatin1(kRagSchemaPragma))) {
                const QString msg = QStringLiteral("Failed to enable RAG database foreign keys: %1")
                                        .arg(checkQuery.lastError().text());
                CP_WARN << "RagIndexerNode:" << msg;
                db.close();
                db = QSqlDatabase();
                QSqlDatabase::removeDatabase(connectionName);
                return fail(msg);
            }
            
            // Check if source_files table exists
            bool sourceFilesExists = false;
            if (checkQuery.exec(QStringLiteral("SELECT name FROM sqlite_master WHERE type='table' AND name='source_files'"))) {
                if (checkQuery.next()) {
                    sourceFilesExists = true;
                }
            } else {
                CP_WARN << "RagIndexerNode: Failed to check source_files table existence:" << checkQuery.lastError().text();
            }
            
            // Create source_files table if it doesn't exist
            if (!sourceFilesExists) {
                if (!checkQuery.exec(QString::fromLatin1(kRagSchemaSourceFiles))) {
                    const QString msg = QStringLiteral("Failed to create source_files table: %1")
                                            .arg(checkQuery.lastError().text());
                    CP_WARN << "RagIndexerNode:" << msg;
                    db.close();
                    db = QSqlDatabase();
                    QSqlDatabase::removeDatabase(connectionName);
                    return fail(msg);
                }
            }
            
            // Check if fragments table exists
            bool fragmentsExists = false;
            if (checkQuery.exec(QStringLiteral("SELECT name FROM sqlite_master WHERE type='table' AND name='fragments'"))) {
                if (checkQuery.next()) {
                    fragmentsExists = true;
                }
            } else {
                CP_WARN << "RagIndexerNode: Failed to check fragments table existence:" << checkQuery.lastError().text();
            }
            
            // Create fragments table if it doesn't exist
            if (!fragmentsExists) {
                if (!checkQuery.exec(QString::fromLatin1(kRagSchemaFragments))) {
                    const QString msg = QStringLiteral("Failed to create fragments table: %1")
                                            .arg(checkQuery.lastError().text());
                    CP_WARN << "RagIndexerNode:" << msg;
                    db.close();
                    db = QSqlDatabase();
                    QSqlDatabase::removeDatabase(connectionName);
                    return fail(msg);
                }
            }

            if (!ensureFragmentLineColumns(db)) {
                const QString msg = QStringLiteral("Failed to migrate fragments table for source line references.");
                db.close();
                db = QSqlDatabase();
                QSqlDatabase::removeDatabase(connectionName);
                return fail(msg);
            }
        } // checkQuery goes out of scope here

        // Clear database if requested (after schema creation to ensure tables exist)
        if (m_clearDatabase) {
            emit statusChanged(QStringLiteral("Status: clearing existing RAG index..."));
            if (!db.transaction()) {
                const QString msg = QStringLiteral("Failed to start transaction for clearing RAG database: %1")
                                        .arg(db.lastError().text());
                CP_WARN << "RagIndexerNode:" << msg;
                db.close();
                db = QSqlDatabase();
                QSqlDatabase::removeDatabase(connectionName);
                return fail(msg);
            } else {
                QSqlQuery clearQuery(db);
                bool clearSuccess = true;
                QString clearError;
                
                // Delete all fragments
                if (!clearQuery.exec(QStringLiteral("DELETE FROM fragments"))) {
                    clearError = QStringLiteral("Failed to delete fragments: %1").arg(clearQuery.lastError().text());
                    CP_WARN << "RagIndexerNode:" << clearError;
                    clearSuccess = false;
                }
                
                // Delete all source files
                if (clearSuccess) {
                    if (!clearQuery.exec(QStringLiteral("DELETE FROM source_files"))) {
                        clearError = QStringLiteral("Failed to delete source_files: %1").arg(clearQuery.lastError().text());
                        CP_WARN << "RagIndexerNode:" << clearError;
                        clearSuccess = false;
                    }
                }
                
                // Reset AUTOINCREMENT counters
                if (clearSuccess) {
                    if (!clearQuery.exec(QStringLiteral("DELETE FROM sqlite_sequence WHERE name IN ('fragments', 'source_files')"))) {
                        clearError = QStringLiteral("Failed to reset AUTOINCREMENT counters: %1").arg(clearQuery.lastError().text());
                        CP_WARN << "RagIndexerNode:" << clearError;
                        clearSuccess = false;
                    }
                }
                
                if (!clearSuccess) {
                    if (!db.rollback()) {
                        CP_WARN << "RagIndexerNode: Rollback failed:" << db.lastError().text();
                    }
                    db.close();
                    db = QSqlDatabase();
                    QSqlDatabase::removeDatabase(connectionName);
                    return fail(clearError.isEmpty() ? QStringLiteral("Failed to clear RAG database.") : clearError);
                } else {
                    if (!db.commit()) {
                        const QString msg = QStringLiteral("Failed to commit clear transaction: %1")
                                                .arg(db.lastError().text());
                        CP_WARN << "RagIndexerNode:" << msg;
                        db.close();
                        db = QSqlDatabase();
                        QSqlDatabase::removeDatabase(connectionName);
                        return fail(msg);
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
            CP_LOG << "RagIndexerNode: Found" << files.size() << "files in" << dirPath
                     << (nameFilters.isEmpty() ? "(no filter)" : QStringLiteral("(filter: %1)").arg(nameFilters.join(", ")));
        }

        if (files.isEmpty()) {
            const QString msg = QStringLiteral("No files found in directory: %1").arg(dirPath);
            CP_WARN << "RagIndexerNode:" << msg;
            db.close();
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(connectionName);
            output.insert(QString::fromLatin1(kOutputCount), QString::number(0));
            output.insert(QString::fromLatin1(kOutputDatabasePath), dbPath);
            emit statusChanged(QStringLiteral("Status: %1").arg(msg));
            return output;
        }

        int totalChunks = 0;
        int embeddingFailures = 0;
        int skippedFiles = 0;
        int databaseInsertFailures = 0;

        // Scope for database operations to ensure all QSqlQuery objects are destroyed before removeDatabase
        {
            // Start transaction for bulk insert
            if (!db.transaction()) {
                const QString msg = QStringLiteral("Failed to start RAG index transaction: %1")
                                        .arg(db.lastError().text());
                CP_WARN << "RagIndexerNode:" << msg;
                db.close();
                db = QSqlDatabase();
                QSqlDatabase::removeDatabase(connectionName);
                return fail(msg);
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
                "INSERT INTO fragments (file_id, chunk_index, start_line, end_line, content, embedding) "
                "VALUES (:file_id, :chunk_index, :start_line, :end_line, :content, :embedding)"));

            const int totalFiles = files.size();

            // Process each file
            int fileIndex = 0;
            for (const QString& filePath : files) {
                ++fileIndex;
                emit statusChanged(QStringLiteral("Status: indexing file %1 of %2: %3")
                                       .arg(fileIndex)
                                       .arg(totalFiles)
                                       .arg(filePath));
                if (verbose) {
                    CP_LOG << "RagIndexerNode: Processing file" << filePath;
                }

                // Read file content
                QString content = DocumentLoader::readTextFile(filePath);
                if (content.isEmpty()) {
                    ++skippedFiles;
                    if (verbose) {
                        CP_LOG << "RagIndexerNode: Skipping empty file:" << filePath;
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
                    CP_WARN << "RagIndexerNode: Failed to insert source file" << filePath 
                               << ":" << fileQuery.lastError().text();
                    ++databaseInsertFailures;
                    continue;
                }

                // Step 2: Retrieve the file_id
                fileIdQuery.bindValue(QStringLiteral(":file_path"), filePath);
                if (!fileIdQuery.exec() || !fileIdQuery.next()) {
                    CP_WARN << "RagIndexerNode: Failed to retrieve file_id for" << filePath 
                               << ":" << fileIdQuery.lastError().text();
                    ++databaseInsertFailures;
                    continue;
                }
                qint64 fileId = fileIdQuery.value(0).toLongLong();

                const FileType fileType = fileTypeForChunkingStrategy(m_chunkingStrategy, filePath);

                // Chunk the text
                QStringList chunks = TextChunker::split(content, m_chunkSize, m_chunkOverlap, fileType);
                const QVector<ChunkLineRange> lineRanges = calculateChunkLineRanges(content, chunks);
                if (verbose) {
                    CP_LOG << "RagIndexerNode: Generated" << chunks.size() << "chunks for" << filePath;
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
                        CP_WARN.noquote() << QStringLiteral("RagIndexerNode: embedding failure provider=%1 model=%2 file=%3 chunk=%4 message=%5")
                                                      .arg(m_providerId, m_modelId, filePath)
                                                      .arg(i)
                                                      .arg(embResult.errorMsg);
                        ++embeddingFailures;
                        continue;
                    }

                    if (embResult.vector.empty()) {
                        CP_WARN.noquote() << QStringLiteral("RagIndexerNode: empty embedding provider=%1 model=%2 file=%3 chunk=%4")
                                                      .arg(m_providerId, m_modelId, filePath)
                                                      .arg(i);
                        ++embeddingFailures;
                        continue;
                    }

                    // Serialize embedding vector to BLOB
                    QByteArray embeddingBlob(reinterpret_cast<const char*>(embResult.vector.data()), 
                                            embResult.vector.size() * sizeof(float));

                    // Step 3: Insert fragment with file_id reference
                    fragmentQuery.bindValue(QStringLiteral(":file_id"), fileId);
                    fragmentQuery.bindValue(QStringLiteral(":chunk_index"), i);
                    const ChunkLineRange lineRange = (i < lineRanges.size()) ? lineRanges.at(i) : ChunkLineRange{};
                    fragmentQuery.bindValue(QStringLiteral(":start_line"),
                                            lineRange.startLine > 0 ? QVariant(lineRange.startLine) : QVariant());
                    fragmentQuery.bindValue(QStringLiteral(":end_line"),
                                            lineRange.endLine > 0 ? QVariant(lineRange.endLine) : QVariant());
                    fragmentQuery.bindValue(QStringLiteral(":content"), chunk);
                    fragmentQuery.bindValue(QStringLiteral(":embedding"), embeddingBlob);

                    if (!fragmentQuery.exec()) {
                        CP_WARN << "RagIndexerNode: Failed to insert chunk" << i << "of" << filePath 
                                   << ":" << fragmentQuery.lastError().text();
                        ++databaseInsertFailures;
                        continue;
                    }

                    totalChunks++;
                    ++insertedForFile;
                }
                
                if (verbose) {
                    CP_LOG << "RagIndexerNode: Inserted" << insertedForFile << "fragments for" << filePath;
                }
            }

            // Commit transaction
            if (!db.commit()) {
                const QString msg = QStringLiteral("Failed to commit RAG index transaction: %1")
                                        .arg(db.lastError().text());
                CP_WARN << "RagIndexerNode:" << msg;
                db.rollback();
                output.insert(QStringLiteral("__error"), msg);
                totalChunks = 0;
            } else {
                if (verbose) {
                    CP_LOG << "RagIndexerNode: Successfully indexed" << totalChunks << "chunks from" 
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
        output.insert(QStringLiteral("embedding_failures"), embeddingFailures);
        output.insert(QStringLiteral("database_insert_failures"), databaseInsertFailures);
        output.insert(QStringLiteral("skipped_files"), skippedFiles);
        output.insert(QStringLiteral("chunking_strategy"), m_chunkingStrategy);

        if (!output.contains(QStringLiteral("__error")) && totalChunks == 0
            && (embeddingFailures > 0 || databaseInsertFailures > 0)) {
            output.insert(QStringLiteral("__error"),
                          QStringLiteral("RAG indexing completed with no inserted chunks. Embedding failures: %1; database failures: %2.")
                              .arg(embeddingFailures)
                              .arg(databaseInsertFailures));
        }

        emit statusChanged(QStringLiteral("Status: indexed %1 chunks from %2 files. Embedding failures: %3; database failures: %4.")
                               .arg(totalChunks)
                               .arg(files.size())
                               .arg(embeddingFailures)
                               .arg(databaseInsertFailures));

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
