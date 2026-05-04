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
        "// Available host API: pipeline.getInput, pipeline.setOutput,\n"
        "// pipeline.setError, pipeline.getTempDir, console.log.\n"
        "const value = pipeline.getInput(\"input\");\n"
        "\n"
        "if (value === undefined || value === null || value === \"\") {\n"
        "  pipeline.setError(\"No input received\");\n"
        "} else if (Array.isArray(value)) {\n"
        "  pipeline.setOutput(\"output\", value.map(item => String(item).toUpperCase()));\n"
        "} else {\n"
        "  pipeline.setOutput(\"output\", String(value).toUpperCase());\n"
        "}\n"
        "\n"
        "console.log(\"Script complete\");\n");
}

inline QString crexx()
{
    return QStringLiteral(
        "options levelb\n"
        "import rxfnsb\n"
        "\n"
        "/* CREXX Universal Script handler.\n"
        "   Available arrays: input[], output[], log[], errors[].\n"
        "   input.0 is the number of input items.\n"
        "*/\n"
        "\n"
        "produce: procedure = .int\n"
        "  arg input = .string[], expose output = .string[], expose log = .string[], expose errors = .string[]\n"
        "\n"
        "if input.0 = 0 then do\n"
        "    errors[1] = \"No input received\"\n"
        "    return 1\n"
        "end\n"
        "\n"
        "do i = 1 to input.0\n"
        "    output[i] = upper(input[i])\n"
        "end\n"
        "\n"
        "log[1] = \"Processed \" || input.0 || \" item(s)\"\n"
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
