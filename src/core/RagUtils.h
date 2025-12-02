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

#include <QString>
#include <vector>

/**
 * @brief Helper utilities for working with the RAG SQLite index.
 */
class RagUtils
{
public:
    struct IndexConfig {
        QString providerId; ///< Embedding provider identifier (e.g. "openai")
        QString modelId;    ///< Embedding model identifier (e.g. "text-embedding-3-small")
    };

    struct SearchResult {
        qint64 fragmentId {0}; ///< fragments.id
        qint64 fileId {0};     ///< fragments.file_id
        int chunkIndex {0};    ///< fragments.chunk_index
        QString content;       ///< fragments.content
        double score {0.0};    ///< Cosine similarity score in [0,1]
    };

    /**
     * @brief Inspect the RAG index and return the unique embedding configuration.
     *
     * Queries the source_files table for distinct (provider, model) pairs.
     * - If exactly one pair exists, returns it.
     * - If zero rows exist, throws std::runtime_error to signal an empty index.
     * - If more than one distinct pair exists, throws std::runtime_error because
     *   mixed-model RAG is not supported.
     */
    static IndexConfig getIndexConfig(const QString& dbPath);

    /**
     * @brief Compute cosine similarity between two float vectors.
     *
     * Returns dot(a,b) / (||a|| * ||b||).
     * If either vector is empty, the sizes differ, or the magnitude is zero,
     * the function returns 0.0.
     */
    static double cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b);

    /**
     * @brief Brute-force vector similarity search over all fragments.
     *
     * Loads every embedding from the fragments table, computes cosine similarity
     * with @p queryEmbedding, filters results below @p minRelevance, sorts them
     * in descending score order, and returns at most @p limit entries.
     */
    static std::vector<SearchResult> findMostRelevantChunks(
        const QString& dbPath,
        const std::vector<float>& queryEmbedding,
        int limit,
        double minRelevance);
};

/**
 * @brief RAG (Retrieval-Augmented Generation) database schema and utilities.
 *
 * This header defines the SQL schema for storing knowledge fragments in a SQLite database,
 * which forms the foundation of the native C++ RAG engine.
 */

/**
 * @brief SQL schema for the RAG database with normalized multi-table design.
 *
 * The schema consists of two tables:
 * 
 * 1. `source_files` - Tracks file-level metadata and embedding model information
 *    Columns:
 *    - id: INTEGER PRIMARY KEY AUTOINCREMENT - Unique identifier for each source file
 *    - file_path: TEXT UNIQUE - The source document path
 *    - provider: TEXT - Embedding provider (e.g., "openai", "google")
 *    - model: TEXT - Embedding model ID (e.g., "text-embedding-3-small")
 *    - last_modified: INTEGER - Timestamp for future incremental updates
 *    - metadata: TEXT - JSON string for tags and additional metadata
 *
 * 2. `fragments` - Stores text chunks with their embeddings
 *    Columns:
 *    - id: INTEGER PRIMARY KEY AUTOINCREMENT - Unique identifier for each fragment
 *    - file_id: INTEGER - Foreign key to source_files.id
 *    - chunk_index: INTEGER - Order/position within the source file
 *    - content: TEXT - The actual text chunk
 *    - embedding: BLOB - The raw binary representation of the vector (float array)
 *
 * Foreign keys are enabled to maintain referential integrity.
 * 
 * Note: Since QSqlQuery::exec() cannot execute multiple statements at once,
 * these need to be executed separately. See kRagSchemaPragma, kRagSchemaSourceFiles,
 * and kRagSchemaFragments below.
 */
constexpr const char* kRagSchemaPragma = "PRAGMA foreign_keys = ON";

constexpr const char* kRagSchemaSourceFiles = R"(
CREATE TABLE IF NOT EXISTS source_files (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_path TEXT UNIQUE NOT NULL,
    provider TEXT NOT NULL,
    model TEXT NOT NULL,
    last_modified INTEGER,
    metadata TEXT
)
)";

constexpr const char* kRagSchemaFragments = R"(
CREATE TABLE IF NOT EXISTS fragments (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_id INTEGER NOT NULL,
    chunk_index INTEGER NOT NULL,
    content TEXT NOT NULL,
    embedding BLOB,
    FOREIGN KEY (file_id) REFERENCES source_files(id) ON DELETE CASCADE
)
)";

// Legacy combined schema (deprecated - kept for reference)
constexpr const char* kRagSchema = R"(
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS source_files (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_path TEXT UNIQUE NOT NULL,
    provider TEXT NOT NULL,
    model TEXT NOT NULL,
    last_modified INTEGER,
    metadata TEXT
);

CREATE TABLE IF NOT EXISTS fragments (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_id INTEGER NOT NULL,
    chunk_index INTEGER NOT NULL,
    content TEXT NOT NULL,
    embedding BLOB,
    FOREIGN KEY (file_id) REFERENCES source_files(id) ON DELETE CASCADE
);
)";
