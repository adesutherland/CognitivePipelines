//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//

#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

namespace ScriptHighlighterConfig {

inline QString jsFileType()
{
    return QStringLiteral(".js");
}

inline QString crexxFileType()
{
    return QStringLiteral(".rexx");
}

inline QStringList supportedFileTypes()
{
    return {jsFileType(), crexxFileType()};
}

inline QString fileTypeForEngine(const QString& engineId)
{
    if (engineId.compare(QStringLiteral("crexx"), Qt::CaseInsensitive) == 0) {
        return crexxFileType();
    }
    return jsFileType();
}

inline QString displayNameForFileType(const QString& fileType)
{
    if (fileType == crexxFileType()) {
        return QStringLiteral("CREXX (.rexx)");
    }
    if (fileType == jsFileType()) {
        return QStringLiteral("JavaScript (.js)");
    }
    return fileType;
}

inline QMap<QString, QString> defaultCommands()
{
    QMap<QString, QString> commands;
    for (const QString& fileType : supportedFileTypes()) {
        commands.insert(fileType, QString());
    }
    return commands;
}

inline QMap<QString, QString> withSupportedRows(QMap<QString, QString> commands)
{
    for (const QString& fileType : supportedFileTypes()) {
        if (!commands.contains(fileType)) {
            commands.insert(fileType, QString());
        }
    }
    return commands;
}

QMap<QString, QString> loadCommands();
void saveCommands(const QMap<QString, QString>& commands);
QString sampleSourceForFileType(const QString& fileType);

} // namespace ScriptHighlighterConfig
