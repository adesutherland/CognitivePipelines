//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//

#pragma once

#include <QString>

namespace UniversalScriptTemplates {

inline QString quickJs()
{
    return QStringLiteral(
        "// QuickJS runtime, not Node.js.\n"
        "// Named pin API: pipeline.input(name) and pipeline.output(name, value).\n"
        "// pipeline.error, pipeline.tempDir, console.log.\n"
        "const value = pipeline.input(\"input\");\n"
        "\n"
        "if (value === undefined || value === null || value === \"\") {\n"
        "  pipeline.error(\"No input received\");\n"
        "} else if (Array.isArray(value)) {\n"
        "  pipeline.output(\"output\", value.map(item => String(item).toUpperCase()));\n"
        "} else {\n"
        "  pipeline.output(\"output\", String(value).toUpperCase());\n"
        "}\n"
        "\n"
        "console.log(\"Script complete\");\n");
}

inline QString crexx()
{
    return QStringLiteral(
        "/* CREXX Universal Script handler.\n"
        "   Named pin API:\n"
        "     address pipeline \"GET input INTO :value\"\n"
        "     address pipeline \"SET output :result\"\n"
        "*/\n"
        "\n"
        "value = \"\"\n"
        "address pipeline \"GET input INTO :value\"\n"
        "\n"
        "if value = \"\" then do\n"
        "    address pipeline \"ERROR No input received\"\n"
        "    return 1\n"
        "end\n"
        "\n"
        "result = upper(value)\n"
        "address pipeline \"SET output :result\"\n"
        "address pipeline \"LOG Processed input pin\"\n"
        "return 0\n");
}

inline QString forEngine(const QString& engineId)
{
    if (engineId.compare(QStringLiteral("crexx"), Qt::CaseInsensitive) == 0) {
        return crexx();
    }
    return quickJs();
}

inline QString normalized(QString value)
{
    value.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    value.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return value.trimmed();
}

inline bool isManagedTemplate(const QString& script)
{
    const QString text = normalized(script);
    return text.isEmpty()
        || text == normalized(quickJs())
        || text == normalized(crexx());
}

} // namespace UniversalScriptTemplates
