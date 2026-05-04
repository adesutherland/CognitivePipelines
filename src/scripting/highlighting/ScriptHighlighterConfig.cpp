//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//

#include "ScriptHighlighterConfig.h"

#include <QSettings>

namespace {

constexpr const char* kSettingsGroup = "SyntaxHighlighting";
constexpr const char* kCommandsGroup = "Commands";

} // namespace

namespace ScriptHighlighterConfig {

QMap<QString, QString> loadCommands()
{
    QMap<QString, QString> commands = defaultCommands();
    QSettings settings;
    settings.beginGroup(QString::fromLatin1(kSettingsGroup));
    settings.beginGroup(QString::fromLatin1(kCommandsGroup));
    for (const QString& fileType : supportedFileTypes()) {
        commands.insert(fileType, settings.value(fileType).toString());
    }
    settings.endGroup();
    settings.endGroup();
    return commands;
}

void saveCommands(const QMap<QString, QString>& commands)
{
    QSettings settings;
    settings.beginGroup(QString::fromLatin1(kSettingsGroup));
    settings.beginGroup(QString::fromLatin1(kCommandsGroup));
    const QMap<QString, QString> supported = withSupportedRows(commands);
    for (const QString& fileType : supportedFileTypes()) {
        settings.setValue(fileType, supported.value(fileType).trimmed());
    }
    settings.endGroup();
    settings.endGroup();
    settings.sync();
}

QString sampleSourceForFileType(const QString& fileType)
{
    if (fileType == crexxFileType()) {
        return QStringLiteral("options levelb\nsay 'hello'\nreturn 0\n");
    }

    return QStringLiteral("const value = 1;\nconsole.log(value);\n");
}

} // namespace ScriptHighlighterConfig
