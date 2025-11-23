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
