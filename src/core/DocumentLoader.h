#ifndef DOCUMENTLOADER_H
#define DOCUMENTLOADER_H

#include <QString>
#include <QStringList>
#include "TextChunker.h"

/**
 * @brief Utility class for scanning directories and reading text files.
 *
 * DocumentLoader provides static methods to recursively scan directory
 * structures for specific file types and read their text content.
 * This is part of the Native RAG engine's ingestion phase.
 */
class DocumentLoader
{
public:
    /**
     * @brief Recursively scans a directory for source code and documentation files.
     *
     * @param rootPath The root directory to start scanning from
     * @param nameFilters Optional list of wildcard patterns (e.g., "*.cpp", "*.h")
     *                    If empty, all supported file types are included
     * @return A list of absolute file paths matching the supported extensions
     *
     * Supported extensions (case-insensitive):
     * .cpp, .h, .hpp, .c, .py, .js, .ts, .md, .txt, .json, .xml, .cmake
     */
    static QStringList scanDirectory(const QString& rootPath, const QStringList& nameFilters = QStringList());

    /**
     * @brief Reads the content of a text file.
     *
     * @param filePath The absolute path to the file to read
     * @return The file content as a QString (UTF-8 encoded), or empty string on error
     *
     * If the file cannot be opened or read, a warning is logged via qWarning()
     * and an empty string is returned.
     */
    static QString readTextFile(const QString& filePath);

    /**
     * @brief Maps a file path or name to the appropriate FileType for code-aware chunking.
     *
     * @param filePath The file path or name to examine
     * @return FileType enum value based on the file extension
     *
     * Extension mappings:
     * - .rexx, .rex, .cmd -> CodeRexx
     * - .py -> CodePython
     * - .cpp, .h, .hpp, .c, .cs, .java, .js, .ts, .go -> CodeCpp
     * - All others -> PlainText
     */
    static FileType getFileTypeFromExtension(const QString& filePath);

private:
    // Helper method to check if a file has a supported extension
    static bool hasSupportedExtension(const QString& fileName);
};

#endif // DOCUMENTLOADER_H
