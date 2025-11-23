#include <gtest/gtest.h>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QSet>
#include "core/DocumentLoader.h"

/**
 * Test suite for DocumentLoader class
 */
class DocumentLoaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure the temporary directory is valid
        ASSERT_TRUE(tempDir.isValid()) << "Failed to create temporary directory";
    }

    QTemporaryDir tempDir;

    // Helper method to create a file with given content
    bool createFile(const QString& relativePath, const QString& content) {
        QString fullPath = tempDir.path() + "/" + relativePath;
        
        // Create parent directories if needed
        QFileInfo fileInfo(fullPath);
        QDir dir;
        if (!dir.mkpath(fileInfo.absolutePath())) {
            return false;
        }
        
        QFile file(fullPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }
        
        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);
        stream << content;
        file.close();
        
        return true;
    }
};

/**
 * Test 1: Directory Traversal
 * Creates a nested directory structure with valid and invalid files
 * and verifies that scanDirectory returns only the valid ones.
 */
TEST_F(DocumentLoaderTest, ScanDirectory_ReturnsOnlyValidFiles) {
    // Create a nested directory structure
    // Valid files (should be found):
    ASSERT_TRUE(createFile("main.cpp", "int main() {}"));
    ASSERT_TRUE(createFile("readme.md", "# README"));
    ASSERT_TRUE(createFile("config.json", "{}"));
    ASSERT_TRUE(createFile("script.py", "print('hello')"));
    ASSERT_TRUE(createFile("header.h", "#ifndef HEADER_H"));
    ASSERT_TRUE(createFile("header2.hpp", "#ifndef HEADER2_HPP"));
    ASSERT_TRUE(createFile("source.c", "void func() {}"));
    ASSERT_TRUE(createFile("index.js", "console.log()"));
    ASSERT_TRUE(createFile("app.ts", "const x: number = 5;"));
    ASSERT_TRUE(createFile("notes.txt", "Some notes"));
    ASSERT_TRUE(createFile("config.xml", "<config/>"));
    ASSERT_TRUE(createFile("CMakeLists.cmake", "cmake_minimum_required"));
    
    // Create nested directories with valid files
    ASSERT_TRUE(createFile("subdir/nested.cpp", "// nested"));
    ASSERT_TRUE(createFile("subdir/deep/very_deep.h", "// very deep"));
    ASSERT_TRUE(createFile("another/path/file.md", "# Deep markdown"));
    
    // Invalid files (should NOT be found):
    ASSERT_TRUE(createFile("image.png", "fake png data"));
    ASSERT_TRUE(createFile("binary.bin", "binary data"));
    ASSERT_TRUE(createFile("archive.zip", "zip data"));
    ASSERT_TRUE(createFile("subdir/photo.jpg", "jpg data"));
    ASSERT_TRUE(createFile("document.pdf", "pdf data"));
    
    // Scan the temporary directory
    QStringList result = DocumentLoader::scanDirectory(tempDir.path());
    
    // Expected valid files (15 total)
    QSet<QString> expectedFileNames = {
        "main.cpp", "readme.md", "config.json", "script.py",
        "header.h", "header2.hpp", "source.c", "index.js",
        "app.ts", "notes.txt", "config.xml", "CMakeLists.cmake",
        "nested.cpp", "very_deep.h", "file.md"
    };
    
    // Verify we found the correct number of files
    EXPECT_EQ(result.size(), 15) << "Should find exactly 15 valid files";
    
    // Verify all expected files are present
    QSet<QString> foundFileNames;
    for (const QString& path : result) {
        QFileInfo info(path);
        foundFileNames.insert(info.fileName());
    }
    
    EXPECT_EQ(foundFileNames, expectedFileNames) << "Should find all valid files and only valid files";
    
    // Verify no invalid files were included
    for (const QString& path : result) {
        QString fileName = QFileInfo(path).fileName();
        EXPECT_FALSE(fileName.contains(".png")) << "Should not include .png files";
        EXPECT_FALSE(fileName.contains(".bin")) << "Should not include .bin files";
        EXPECT_FALSE(fileName.contains(".zip")) << "Should not include .zip files";
        EXPECT_FALSE(fileName.contains(".jpg")) << "Should not include .jpg files";
        EXPECT_FALSE(fileName.contains(".pdf")) << "Should not include .pdf files";
    }
    
    // Verify that nested files are found with absolute paths
    bool foundNested = false;
    bool foundVeryDeep = false;
    for (const QString& path : result) {
        if (path.contains("nested.cpp")) {
            foundNested = true;
            EXPECT_TRUE(path.contains("subdir")) << "nested.cpp should be in subdir";
        }
        if (path.contains("very_deep.h")) {
            foundVeryDeep = true;
            EXPECT_TRUE(path.contains("subdir/deep") || path.contains("subdir\\deep")) 
                << "very_deep.h should be in subdir/deep";
        }
    }
    EXPECT_TRUE(foundNested) << "Should find nested.cpp in subdirectory";
    EXPECT_TRUE(foundVeryDeep) << "Should find very_deep.h in deeply nested directory";
}

/**
 * Test 2: Reading Text Files
 * Creates a temporary file with UTF-8 content including special characters
 * and verifies that readTextFile returns the exact content.
 */
TEST_F(DocumentLoaderTest, ReadTextFile_ReturnsExactContent) {
    // Create content with UTF-8 characters including emojis and accented chars
    QString expectedContent = QStringLiteral(
        "Hello World! 🌍\n"
        "Special chars: café, naïve, résumé\n"
        "Math symbols: α, β, γ, ∑, ∫\n"
        "Emojis: 😀 🎉 🚀 💻\n"
        "Chinese: 你好世界\n"
        "Japanese: こんにちは\n"
        "Arabic: مرحبا\n"
        "Line with tab:\there\n"
        "Final line without newline"
    );
    
    // Create the file
    QString testFile = "utf8_test.txt";
    ASSERT_TRUE(createFile(testFile, expectedContent));
    
    // Read the file
    QString fullPath = tempDir.path() + "/" + testFile;
    QString actualContent = DocumentLoader::readTextFile(fullPath);
    
    // Verify content matches exactly
    EXPECT_EQ(actualContent, expectedContent) << "Content should match exactly, including UTF-8 characters";
    
    // Verify specific substrings to ensure proper encoding
    EXPECT_TRUE(actualContent.contains(QStringLiteral("🌍"))) << "Should contain earth emoji";
    EXPECT_TRUE(actualContent.contains(QStringLiteral("café"))) << "Should contain accented 'é'";
    EXPECT_TRUE(actualContent.contains(QStringLiteral("α"))) << "Should contain Greek alpha";
    EXPECT_TRUE(actualContent.contains(QStringLiteral("你好世界"))) << "Should contain Chinese characters";
    EXPECT_TRUE(actualContent.contains(QStringLiteral("😀"))) << "Should contain grinning emoji";
}

/**
 * Test 3: Reading Non-existent File
 * Verifies that readTextFile returns empty string for non-existent files
 */
TEST_F(DocumentLoaderTest, ReadTextFile_NonExistentFile_ReturnsEmptyString) {
    QString nonExistentPath = tempDir.path() + "/does_not_exist.txt";
    QString result = DocumentLoader::readTextFile(nonExistentPath);
    
    EXPECT_TRUE(result.isEmpty()) << "Should return empty string for non-existent file";
}

/**
 * Test 4: Case-insensitive Extension Matching
 * Verifies that file extensions are matched case-insensitively
 */
TEST_F(DocumentLoaderTest, ScanDirectory_CaseInsensitiveExtensions) {
    // Create files with various case combinations
    ASSERT_TRUE(createFile("File1.CPP", "// uppercase"));
    ASSERT_TRUE(createFile("File2.Cpp", "// mixed case"));
    ASSERT_TRUE(createFile("File3.H", "// uppercase .H"));
    ASSERT_TRUE(createFile("File4.MD", "// uppercase .MD"));
    ASSERT_TRUE(createFile("File5.Json", "// mixed case .Json"));
    
    QStringList result = DocumentLoader::scanDirectory(tempDir.path());
    
    // Should find all 5 files regardless of extension case
    EXPECT_EQ(result.size(), 5) << "Should find all files with case-insensitive extension matching";
    
    // Verify each file is found
    QSet<QString> foundNames;
    for (const QString& path : result) {
        foundNames.insert(QFileInfo(path).fileName());
    }
    
    EXPECT_TRUE(foundNames.contains("File1.CPP"));
    EXPECT_TRUE(foundNames.contains("File2.Cpp"));
    EXPECT_TRUE(foundNames.contains("File3.H"));
    EXPECT_TRUE(foundNames.contains("File4.MD"));
    EXPECT_TRUE(foundNames.contains("File5.Json"));
}

/**
 * Test 5: Empty Directory
 * Verifies that scanDirectory handles empty directories gracefully
 */
TEST_F(DocumentLoaderTest, ScanDirectory_EmptyDirectory_ReturnsEmptyList) {
    QStringList result = DocumentLoader::scanDirectory(tempDir.path());
    
    EXPECT_TRUE(result.isEmpty()) << "Should return empty list for empty directory";
}
