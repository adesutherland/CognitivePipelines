#include "DocumentLoader.h"
#include "TextChunker.h"
#include <QDirIterator>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDebug>

QStringList DocumentLoader::scanDirectory(const QString& rootPath, const QStringList& nameFilters)
{
    QStringList result;
    
    // Use QDirIterator with Subdirectories flag to traverse recursively
    // If nameFilters is provided, pass it to QDirIterator; otherwise scan all files
    QDirIterator it(nameFilters.isEmpty() ? rootPath : rootPath,
                    nameFilters.isEmpty() ? QStringList() : nameFilters,
                    QDir::Files,
                    QDirIterator::Subdirectories);
    
    while (it.hasNext()) {
        QString filePath = it.next();
        QFileInfo fileInfo(filePath);
        
        // Skip directories (only process files)
        if (!fileInfo.isFile()) {
            continue;
        }
        
        // If nameFilters was provided, files are already filtered by QDirIterator
        // Otherwise, check if the file has a supported extension
        if (nameFilters.isEmpty()) {
            if (hasSupportedExtension(fileInfo.fileName())) {
                result.append(fileInfo.absoluteFilePath());
            }
        } else {
            // nameFilters already applied, just add the file
            result.append(fileInfo.absoluteFilePath());
        }
    }
    
    return result;
}

QString DocumentLoader::readTextFile(const QString& filePath)
{
    QFile file(filePath);
    
    // Try to open the file for reading
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "DocumentLoader: Failed to open file:" << filePath 
                   << "Error:" << file.errorString();
        return QString();
    }
    
    // Read the content as UTF-8 text
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    QString content = stream.readAll();
    
    file.close();
    return content;
}

bool DocumentLoader::hasSupportedExtension(const QString& fileName)
{
    // List of supported extensions (case-insensitive)
    static const QStringList supportedExtensions = {
        // C-family (brace-based)
        QStringLiteral(".cpp"),
        QStringLiteral(".h"),
        QStringLiteral(".hpp"),
        QStringLiteral(".c"),
        QStringLiteral(".cs"),
        QStringLiteral(".java"),
        QStringLiteral(".js"),
        QStringLiteral(".ts"),
        QStringLiteral(".tsx"),
        QStringLiteral(".go"),
        QStringLiteral(".rs"),
        QStringLiteral(".swift"),
        QStringLiteral(".kt"),
        // Python
        QStringLiteral(".py"),
        // Rexx
        QStringLiteral(".rexx"),
        QStringLiteral(".rex"),
        QStringLiteral(".cmd"),
        // SQL
        QStringLiteral(".sql"),
        QStringLiteral(".plsql"),
        QStringLiteral(".tsql"),
        // Shell
        QStringLiteral(".sh"),
        QStringLiteral(".bash"),
        QStringLiteral(".ps1"),
        QStringLiteral(".zsh"),
        // Cobol
        QStringLiteral(".cbl"),
        QStringLiteral(".cob"),
        QStringLiteral(".copy"),
        // YAML/Terraform
        QStringLiteral(".yaml"),
        QStringLiteral(".yml"),
        QStringLiteral(".tf"),
        QStringLiteral(".hcl"),
        // Assembly
        QStringLiteral(".asm"),
        QStringLiteral(".s"),
        // Generic text / Markdown
        QStringLiteral(".md"),
        QStringLiteral(".markdown"),
        QStringLiteral(".txt"),
        QStringLiteral(".json"),
        QStringLiteral(".xml"),
        QStringLiteral(".cmake")
    };
    
    // Convert filename to lower case for case-insensitive comparison
    QString lowerFileName = fileName.toLower();
    
    // Check if any supported extension matches
    for (const QString& ext : supportedExtensions) {
        if (lowerFileName.endsWith(ext)) {
            return true;
        }
    }
    
    return false;
}

FileType DocumentLoader::getFileTypeFromExtension(const QString& filePath)
{
    // Convert to lowercase for case-insensitive comparison
    QString lowerPath = filePath.toLower();
    
    // Rexx family
    if (lowerPath.endsWith(".rexx") || lowerPath.endsWith(".rex") || lowerPath.endsWith(".cmd")) {
        return FileType::CodeRexx;
    }
    
    // Python family
    if (lowerPath.endsWith(".py")) {
        return FileType::CodePython;
    }
    
    // SQL family
    if (lowerPath.endsWith(".sql") || lowerPath.endsWith(".plsql") || lowerPath.endsWith(".tsql")) {
        return FileType::CodeSql;
    }
    
    // Shell family (Bash, PowerShell, Zsh) and Assembly (line-based fallback)
    if (lowerPath.endsWith(".sh") || lowerPath.endsWith(".bash") || lowerPath.endsWith(".ps1") ||
        lowerPath.endsWith(".zsh") || lowerPath.endsWith(".asm") || lowerPath.endsWith(".s")) {
        return FileType::CodeShell;
    }
    
    // Cobol family
    if (lowerPath.endsWith(".cbl") || lowerPath.endsWith(".cob") || lowerPath.endsWith(".copy")) {
        return FileType::CodeCobol;
    }
    
    // YAML/Terraform family
    if (lowerPath.endsWith(".yaml") || lowerPath.endsWith(".yml") || 
        lowerPath.endsWith(".tf") || lowerPath.endsWith(".hcl")) {
        return FileType::CodeYaml;
    }
    
    // Markdown family
    if (lowerPath.endsWith(".md") || lowerPath.endsWith(".markdown")) {
        return FileType::CodeMarkdown;
    }
    
    // C-family (C, C++, C#, Java, JavaScript, TypeScript, Go, Rust, Swift, Kotlin)
    if (lowerPath.endsWith(".cpp") || lowerPath.endsWith(".h") || lowerPath.endsWith(".hpp") ||
        lowerPath.endsWith(".c") || lowerPath.endsWith(".cs") || lowerPath.endsWith(".java") ||
        lowerPath.endsWith(".js") || lowerPath.endsWith(".ts") || lowerPath.endsWith(".tsx") ||
        lowerPath.endsWith(".go") || lowerPath.endsWith(".rs") || lowerPath.endsWith(".swift") ||
        lowerPath.endsWith(".kt")) {
        return FileType::CodeCpp;
    }
    
    // Default to plain text
    return FileType::PlainText;
}
