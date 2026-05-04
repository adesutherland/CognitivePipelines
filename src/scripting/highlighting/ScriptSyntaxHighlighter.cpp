//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//

#include "ScriptSyntaxHighlighter.h"
#include "ScriptHighlighterConfig.h"

#include <QBrush>
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QString>
#include <QTextDocument>

#include <algorithm>
#include <mutex>
#include <utility>

#if CP_HAS_DSLSH
#include <cstdlib>
#include <cstring>
#ifndef restrict
#define restrict
#define CP_SCRIPT_HIGHLIGHT_UNDEF_RESTRICT
#endif
extern "C" {
#include "dslsyntax_common.h"
#include "dslsyntax_editor.h"
}
#ifdef CP_SCRIPT_HIGHLIGHT_UNDEF_RESTRICT
#undef restrict
#undef CP_SCRIPT_HIGHLIGHT_UNDEF_RESTRICT
#endif
#endif

namespace {

#if CP_HAS_DSLSH
constexpr const char* kDslshConfig = R"(
[.rexx]
keywords=address,arg,by,call,do,else,end,error,exit,expose,for,forever,if,import,interpret,iterate,leave,loop,namespace,options,otherwise,parse,procedure,pull,return,say,select,signal,then,to,until,when,while
operators=||,\\=,<=,>=,<>,==,=,+,-,*,/,%,<,>,:,.,[,],(,)
line_comment=--
block_start=/*
block_end=*/
quotes="'"
ident_extra_chars=_

[.js]
keywords=async,await,break,case,catch,class,const,continue,debugger,default,delete,do,else,export,extends,false,finally,for,function,if,import,in,instanceof,let,new,null,return,static,super,switch,this,throw,true,try,typeof,undefined,var,void,while,yield
operators====,!==,===,!=,==,>=,<=,&&,||,=>,++,--,+,-,*,/,%,=,<,>,!,.,?,:
line_comment=//
block_start=/*
block_end=*/
quotes="'"`
ident_extra_chars=$_
)";

std::once_flag g_dslshInitOnce;
std::once_flag g_editorInitOnce;

void ensureDslshConfig()
{
    std::call_once(g_dslshInitOnce, []() {
        cb_load_ep_config_from_string(kDslshConfig);
    });
}

void ensureEditorInitialized()
{
    std::call_once(g_editorInitOnce, []() {
        editor_init();
    });
}

QString filenameForEngine(const QString& engineId)
{
    if (ScriptHighlighterConfig::fileTypeForEngine(engineId) == ScriptHighlighterConfig::crexxFileType()) {
        return QStringLiteral("script.rexx");
    }
    return QStringLiteral("script.js");
}

CodeBuffer* createSingleLineBuffer(const QString& engineId, const QString& text)
{
    CodeBuffer* cb = create_code_buffer(nullptr, nullptr);
    if (!cb) {
        return nullptr;
    }

    const QByteArray filename = filenameForEngine(engineId).toUtf8();
    cb->unique_document_id = ::strdup(filename.constData());
    cb->line_count = 1;
    cb->lines = static_cast<CodeBufferLine*>(std::calloc(1, sizeof(CodeBufferLine)));
    if (!cb->lines) {
        free_code_buffer(cb);
        return nullptr;
    }

    const QByteArray bytes = text.toUtf8();
    if (first_line_utf8_to_line(bytes.constData(), &cb->lines[0]) != 0) {
        free_code_buffer(cb);
        return nullptr;
    }

    cb_seed_ep_rules(cb, filename.constData());
    return cb;
}

QString expandLeadingHome(const QString& command)
{
    QString expanded = command.trimmed();
    if (expanded.startsWith(QStringLiteral("~/"))) {
        expanded.replace(0, 1, QDir::homePath());
    } else if (expanded.startsWith(QStringLiteral("\"~/"))) {
        expanded.replace(1, 1, QDir::homePath());
    } else if (expanded.startsWith(QStringLiteral("'~/"))) {
        expanded.replace(1, 1, QDir::homePath());
    }
    return expanded;
}

QString firstCommandPart(const QString& command)
{
    const QStringList parts = QProcess::splitCommand(command);
    return parts.isEmpty() ? QString() : parts.first();
}

bool commandExecutableExists(const QString& command)
{
    const QString executable = firstCommandPart(command);
    if (executable.isEmpty()) {
        return false;
    }

    if (executable.contains(QDir::separator())) {
        const QFileInfo info(executable);
        return info.exists() && info.isExecutable();
    }

    return !QStandardPaths::findExecutable(executable).isEmpty();
}

QString normalizeExternalCommand(const QString& engineId, const QString& command)
{
    QString normalized = expandLeadingHome(command);
    if (normalized.isEmpty()) {
        return normalized;
    }

    const QFileInfo executable(firstCommandPart(normalized));
    const bool isCrexx = ScriptHighlighterConfig::fileTypeForEngine(engineId) == ScriptHighlighterConfig::crexxFileType();
    const bool isRxc = executable.fileName().compare(QStringLiteral("rxc"), Qt::CaseInsensitive) == 0;
    if (isCrexx && isRxc && !normalized.contains(QStringLiteral("--port"))) {
        if (normalized.contains(QStringLiteral("--parser")) && !normalized.contains(QStringLiteral("--syntaxhighlight"))) {
            normalized.replace(QStringLiteral("--parser"), QStringLiteral("--syntaxhighlight"));
        } else if (!normalized.contains(QStringLiteral("--syntaxhighlight"))) {
            normalized += QStringLiteral(" --syntaxhighlight");
        }
    }

    return normalized;
}
#endif

QSet<ScriptSyntaxHighlighter*> g_openHighlighters;

} // namespace

ScriptSyntaxHighlighter::ScriptSyntaxHighlighter(QTextDocument* document)
    : QSyntaxHighlighter(document)
{
    initFormats();
    g_openHighlighters.insert(this);

    m_parsePollTimer.setInterval(30);
    connect(&m_parsePollTimer, &QTimer::timeout, this, [this]() {
        pollPendingExternalParse();
    });
}

ScriptSyntaxHighlighter::~ScriptSyntaxHighlighter()
{
    clearPendingExternalParse(true);
    g_openHighlighters.remove(this);
}

void ScriptSyntaxHighlighter::setEngineId(const QString& engineId)
{
    if (m_engineId == engineId) {
        return;
    }
    m_engineId = engineId;
    rehighlight();
}

void ScriptSyntaxHighlighter::setExternalCommandOverrideForTesting(const QString& command)
{
    m_hasCommandOverride = true;
    m_commandOverride = command;
    clearPendingExternalParse(true);
    m_cachedExternalValid = false;
    m_cachedExternalTokenTypes.clear();
    rehighlight();
}

bool ScriptSyntaxHighlighter::backendAvailable()
{
#if CP_HAS_DSLSH
    return true;
#else
    return false;
#endif
}

void ScriptSyntaxHighlighter::rehighlightOpenEditors()
{
    const QSet<ScriptSyntaxHighlighter*> highlighters = g_openHighlighters;
    for (ScriptSyntaxHighlighter* highlighter : highlighters) {
        if (highlighter) {
            highlighter->rehighlight();
        }
    }
}

bool ScriptSyntaxHighlighter::checkHighlighterCommand(const QString& fileType, const QString& command, QString* message)
{
#if CP_HAS_DSLSH
    if (command.trimmed().isEmpty()) {
        if (message) {
            *message = QStringLiteral("OK: using built-in DSLSH fallback rules.");
        }
        return true;
    }

    QTextDocument document;
    ScriptSyntaxHighlighter highlighter(&document);
    highlighter.m_engineId = (fileType == ScriptHighlighterConfig::crexxFileType())
                                 ? QStringLiteral("crexx")
                                 : QStringLiteral("quickjs");
    if (!highlighter.parseWithExternalCommand(
        ScriptHighlighterConfig::sampleSourceForFileType(fileType),
        command,
        message)) {
        return false;
    }

    if (fileType == ScriptHighlighterConfig::crexxFileType()) {
        QString diagnosticMessage;
        const QVector<ScriptHighlighterDiagnostic> diagnostics = collectDiagnostics(
            fileType,
            command,
            QStringLiteral("options levelb\nif then\nreturn 0\n"),
            &diagnosticMessage);

        if (diagnostics.isEmpty()) {
            if (message) {
                *message = QStringLiteral("Command starts, but no CREXX diagnostics came back for an invalid sample.");
            }
            return false;
        }

        auto diagnosticLabel = [&]() {
            for (const ScriptHighlighterDiagnostic& diagnostic : diagnostics) {
                if (!diagnostic.message.isEmpty()) {
                    return diagnostic.message;
                }
            }
            return diagnosticMessage.isEmpty()
                ? QStringLiteral("parser diagnostic")
                : diagnosticMessage;
        };

        if (message) {
            *message = QStringLiteral("OK: highlighter responded. Parser diagnostics are available (test diagnostic: %1).")
                           .arg(diagnosticLabel());
        }
    }

    return true;
#else
    if (message) {
        *message = QStringLiteral("DSLSH was not built into this application.");
    }
    Q_UNUSED(fileType);
    Q_UNUSED(command);
    return false;
#endif
}

QVector<ScriptHighlighterDiagnostic> ScriptSyntaxHighlighter::collectDiagnostics(
    const QString& fileType,
    const QString& command,
    const QString& source,
    QString* message)
{
    QVector<ScriptHighlighterDiagnostic> diagnostics;

#if CP_HAS_DSLSH
    if (command.trimmed().isEmpty()) {
        if (message) {
            *message = QStringLiteral("Built-in fallback rules do not provide parser diagnostics.");
        }
        return diagnostics;
    }

    QTextDocument document;
    ScriptSyntaxHighlighter highlighter(&document);
    highlighter.m_engineId = (fileType == ScriptHighlighterConfig::crexxFileType())
                                 ? QStringLiteral("crexx")
                                 : QStringLiteral("quickjs");
    if (!highlighter.parseWithExternalCommand(source, command, message)) {
        return diagnostics;
    }

    diagnostics = highlighter.diagnosticsFromCache();
    if (message && diagnostics.isEmpty()) {
        *message = QStringLiteral("Highlighter command responded but returned no diagnostics.");
    }
#else
    Q_UNUSED(fileType);
    Q_UNUSED(command);
    Q_UNUSED(source);
    if (message) {
        *message = QStringLiteral("DSLSH was not built into this application.");
    }
#endif

    return diagnostics;
}

void ScriptSyntaxHighlighter::highlightBlock(const QString& text)
{
#if CP_HAS_DSLSH
    if (text.isEmpty()) {
        return;
    }

    if (!configuredCommandForEngine().isEmpty() && ensureExternalParseCache()) {
        const int lineNumber = currentBlock().blockNumber();
        if (lineNumber >= 0 && lineNumber < m_cachedExternalTokenTypes.size()) {
            applyTokenRuns(m_cachedExternalTokenTypes.at(lineNumber), text.size());
            return;
        }
    }

    highlightWithEmergencyRules(text);
#else
    Q_UNUSED(text);
#endif
}

void ScriptSyntaxHighlighter::highlightWithEmergencyRules(const QString& text)
{
#if CP_HAS_DSLSH
    ensureDslshConfig();
    CodeBuffer* cb = createSingleLineBuffer(m_engineId, text);
    if (!cb || cb->line_count == 0 || !cb->lines || !cb->lines[0].characters) {
        if (cb) {
            free_code_buffer(cb);
        }
        return;
    }

    const CodeBufferLine& line = cb->lines[0];
    const int limit = std::min<int>(static_cast<int>(line.length), text.size());
    QVector<ScriptHighlightCell> tokenTypes;
    tokenTypes.reserve(limit);
    for (int i = 0; i < limit; ++i) {
        tokenTypes.append({static_cast<unsigned char>(line.characters[i].token_type), CB_NONE, QString()});
    }
    applyTokenRuns(tokenTypes, text.size());

    free_code_buffer(cb);
#else
    Q_UNUSED(text);
#endif
}

bool ScriptSyntaxHighlighter::ensureExternalParseCache()
{
#if CP_HAS_DSLSH
    const QString command = configuredCommandForEngine();
    if (command.isEmpty()) {
        clearPendingExternalParse(false);
        return false;
    }

    const QString content = document() ? document()->toPlainText() : QString();
    if (m_cachedExternalEngineId == m_engineId
        && m_cachedExternalCommand == command
        && m_cachedExternalContent == content) {
        return m_cachedExternalValid;
    }

    if (m_pendingCodeBuffer) {
        const bool pendingMatches = m_pendingExternalEngineId == m_engineId
            && m_pendingExternalCommand == command
            && m_pendingExternalContent == content;

        if (isPendingExternalParseComplete()) {
            if (pendingMatches) {
                copyTokenTypesFromCodeBuffer(m_pendingCodeBuffer);
                m_cachedExternalEngineId = m_pendingExternalEngineId;
                m_cachedExternalCommand = m_pendingExternalCommand;
                m_cachedExternalContent = m_pendingExternalContent;
                m_cachedExternalValid = !m_cachedExternalTokenTypes.isEmpty();
                clearPendingExternalParse(false);
                return m_cachedExternalValid;
            }

            clearPendingExternalParse(false);
        } else {
            if (!m_parsePollTimer.isActive()) {
                m_parsePollTimer.start();
            }
            return false;
        }
    }

    m_cachedExternalEngineId = m_engineId;
    m_cachedExternalCommand = command;
    m_cachedExternalContent = content;
    m_cachedExternalTokenTypes.clear();
    m_cachedExternalValid = false;
    startExternalParse(content, command);
    return false;
#else
    return false;
#endif
}

bool ScriptSyntaxHighlighter::startExternalParse(const QString& content, const QString& command)
{
#if CP_HAS_DSLSH
    clearPendingExternalParse(false);
    if (m_pendingCodeBuffer) {
        return false;
    }

    const QString normalizedCommand = normalizeExternalCommand(m_engineId, command);
    if (!commandExecutableExists(normalizedCommand)) {
        return false;
    }

    ensureEditorInitialized();

    const QByteArray commandBytes = normalizedCommand.toUtf8();
    CommunicationFunctions* comm = create_stdio_communication_functions(commandBytes.constData());
    if (!comm) {
        return false;
    }

    CodeBuffer* cb = create_code_buffer(comm, nullptr);
    if (!cb) {
        free_stdio_communication_functions(comm);
        return false;
    }

    const QByteArray filename = filenameForEngine(m_engineId).toUtf8();
    const QByteArray contentBytes = content.toUtf8();
    InitialLoad* initialLoad = create_initial_load(filename.constData(), contentBytes.constData());
    if (!initialLoad) {
        free_code_buffer(cb);
        free_stdio_communication_functions(comm);
        return false;
    }

    load_initial_content(cb, initialLoad);

    m_pendingExternalEngineId = m_engineId;
    m_pendingExternalCommand = command;
    m_pendingExternalContent = content;
    m_pendingCodeBuffer = cb;
    m_pendingCommunication = comm;
    m_parsePollTimer.start();
    return true;
#else
    Q_UNUSED(content);
    Q_UNUSED(command);
    return false;
#endif
}

bool ScriptSyntaxHighlighter::isPendingExternalParseComplete() const
{
#if CP_HAS_DSLSH
    auto* cb = static_cast<CodeBuffer*>(m_pendingCodeBuffer);
    return cb && (cb_check_parse_complete_event(cb) || !cb->async_parse_active);
#else
    return false;
#endif
}

void ScriptSyntaxHighlighter::pollPendingExternalParse()
{
#if CP_HAS_DSLSH
    if (!m_pendingCodeBuffer) {
        m_parsePollTimer.stop();
        return;
    }

    if (!isPendingExternalParseComplete()) {
        return;
    }

    const QString currentCommand = configuredCommandForEngine();
    const QString currentContent = document() ? document()->toPlainText() : QString();
    const bool pendingStillMatches = m_pendingExternalEngineId == m_engineId
        && m_pendingExternalCommand == currentCommand
        && m_pendingExternalContent == currentContent;

    if (pendingStillMatches) {
        copyTokenTypesFromCodeBuffer(m_pendingCodeBuffer);
        m_cachedExternalEngineId = m_pendingExternalEngineId;
        m_cachedExternalCommand = m_pendingExternalCommand;
        m_cachedExternalContent = m_pendingExternalContent;
        m_cachedExternalValid = !m_cachedExternalTokenTypes.isEmpty();
        clearPendingExternalParse(false);
        rehighlight();
        return;
    }

    clearPendingExternalParse(false);
    if (!currentCommand.isEmpty()) {
        startExternalParse(currentContent, currentCommand);
    }
#endif
}

void ScriptSyntaxHighlighter::clearPendingExternalParse(bool waitForCompletion)
{
#if CP_HAS_DSLSH
    if (waitForCompletion && m_pendingCodeBuffer && !isPendingExternalParseComplete()) {
        editor_wait_for_parser_threads();
    }

    if (m_pendingCodeBuffer && !isPendingExternalParseComplete()) {
        return;
    }

    auto* cb = static_cast<CodeBuffer*>(m_pendingCodeBuffer);
    auto* comm = static_cast<CommunicationFunctions*>(m_pendingCommunication);
    if (cb) {
        free_code_buffer(cb);
    }
    if (comm) {
        free_stdio_communication_functions(comm);
    }
#endif

    m_pendingCodeBuffer = nullptr;
    m_pendingCommunication = nullptr;
    m_pendingExternalEngineId.clear();
    m_pendingExternalCommand.clear();
    m_pendingExternalContent.clear();
    if (!m_pendingCodeBuffer) {
        m_parsePollTimer.stop();
    }
}

void ScriptSyntaxHighlighter::copyTokenTypesFromCodeBuffer(const void* codeBuffer)
{
#if CP_HAS_DSLSH
    const auto* cb = static_cast<const CodeBuffer*>(codeBuffer);
    if (!cb || !cb->lines || cb->line_count == 0) {
        m_cachedExternalTokenTypes.clear();
        return;
    }

    QVector<QVector<ScriptHighlightCell>> tokenTypes;
    tokenTypes.reserve(static_cast<int>(cb->line_count));
    for (size_t lineIndex = 0; lineIndex < cb->line_count; ++lineIndex) {
        const CodeBufferLine& line = cb->lines[lineIndex];
        QVector<ScriptHighlightCell> lineTokens;
        lineTokens.reserve(static_cast<int>(line.length));
        for (size_t column = 0; column < line.length; ++column) {
            const CodeBufferCharacter& character = line.characters[column];
            QString diagnostic;
            if (character.node && character.node->message) {
                diagnostic = QString::fromUtf8(character.node->message);
            } else if (character.node && character.node->message_code) {
                diagnostic = QString::fromUtf8(character.node->message_code);
            }
            lineTokens.append({
                static_cast<unsigned char>(character.token_type),
                static_cast<unsigned char>(character.severity),
                diagnostic
            });
        }
        tokenTypes.append(std::move(lineTokens));
    }

    m_cachedExternalTokenTypes = std::move(tokenTypes);
#else
    Q_UNUSED(codeBuffer);
#endif
}

bool ScriptSyntaxHighlighter::parseWithExternalCommand(const QString& content, const QString& command, QString* message)
{
#if CP_HAS_DSLSH
    const QString normalizedCommand = normalizeExternalCommand(m_engineId, command);
    if (!commandExecutableExists(normalizedCommand)) {
        if (message) {
            *message = QStringLiteral("Not found or not executable: %1").arg(firstCommandPart(normalizedCommand));
        }
        return false;
    }

    ensureEditorInitialized();

    const QByteArray commandBytes = normalizedCommand.toUtf8();
    CommunicationFunctions* comm = create_stdio_communication_functions(commandBytes.constData());
    if (!comm) {
        if (message) {
            *message = QStringLiteral("Could not start highlighter command.");
        }
        return false;
    }

    CodeBuffer* cb = create_code_buffer(comm, nullptr);
    if (!cb) {
        free_stdio_communication_functions(comm);
        if (message) {
            *message = QStringLiteral("Could not create DSLSH editor buffer.");
        }
        return false;
    }

    const QByteArray filename = filenameForEngine(m_engineId).toUtf8();
    const QByteArray contentBytes = content.toUtf8();
    InitialLoad* initialLoad = create_initial_load(filename.constData(), contentBytes.constData());
    if (!initialLoad) {
        free_code_buffer(cb);
        free_stdio_communication_functions(comm);
        if (message) {
            *message = QStringLiteral("Could not create DSLSH initial load.");
        }
        return false;
    }

    load_initial_content(cb, initialLoad);
    editor_wait_for_parser_threads();

    if (!cb->lines || cb->line_count == 0) {
        free_code_buffer(cb);
        free_stdio_communication_functions(comm);
        if (message) {
            *message = QStringLiteral("Highlighter returned no editor lines.");
        }
        return false;
    }

    QVector<QVector<ScriptHighlightCell>> tokenTypes;
    tokenTypes.reserve(static_cast<int>(cb->line_count));
    for (size_t lineIndex = 0; lineIndex < cb->line_count; ++lineIndex) {
        const CodeBufferLine& line = cb->lines[lineIndex];
        QVector<ScriptHighlightCell> lineTokens;
        lineTokens.reserve(static_cast<int>(line.length));
        for (size_t column = 0; column < line.length; ++column) {
            const CodeBufferCharacter& character = line.characters[column];
            QString diagnostic;
            if (character.node && character.node->message) {
                diagnostic = QString::fromUtf8(character.node->message);
            } else if (character.node && character.node->message_code) {
                diagnostic = QString::fromUtf8(character.node->message_code);
            }
            lineTokens.append({
                static_cast<unsigned char>(character.token_type),
                static_cast<unsigned char>(character.severity),
                diagnostic
            });
        }
        tokenTypes.append(std::move(lineTokens));
    }

    m_cachedExternalTokenTypes = std::move(tokenTypes);

    free_code_buffer(cb);
    free_stdio_communication_functions(comm);
    const bool ok = !m_cachedExternalTokenTypes.isEmpty();
    if (message) {
        *message = ok
            ? QStringLiteral("OK: highlighter command responded.")
            : QStringLiteral("Highlighter command returned no token data.");
    }
    return ok;
#else
    Q_UNUSED(content);
    Q_UNUSED(command);
    Q_UNUSED(message);
    return false;
#endif
}

void ScriptSyntaxHighlighter::initFormats()
{
#if CP_HAS_DSLSH
    QTextCharFormat keyword;
    keyword.setForeground(QColor(QStringLiteral("#0057a8")));
    keyword.setFontWeight(QFont::DemiBold);
    m_formats.insert(LEXER_KEYWORD, keyword);

    QTextCharFormat preprocessor;
    preprocessor.setForeground(QColor(QStringLiteral("#6d28d9")));
    preprocessor.setFontWeight(QFont::DemiBold);
    m_formats.insert(LEXER_PREPROCESSOR, preprocessor);

    QTextCharFormat stringLiteral;
    stringLiteral.setForeground(QColor(QStringLiteral("#177245")));
    m_formats.insert(LEXER_STRING_LITERAL, stringLiteral);

    QTextCharFormat numberLiteral;
    numberLiteral.setForeground(QColor(QStringLiteral("#8a3ffc")));
    m_formats.insert(LEXER_NUMBER_LITERAL, numberLiteral);

    QTextCharFormat comment;
    comment.setForeground(QColor(QStringLiteral("#667085")));
    comment.setFontItalic(true);
    m_formats.insert(LEXER_COMMENT, comment);

    QTextCharFormat op;
    op.setForeground(QColor(QStringLiteral("#9a5a00")));
    m_formats.insert(LEXER_OPERATOR, op);
    m_formats.insert(LEXER_OPERATOR_ASSIGN, op);
    m_formats.insert(LEXER_OPERATOR_ARITHMETIC, op);
    m_formats.insert(LEXER_OPERATOR_LOGICAL, op);
    m_formats.insert(LEXER_SEPARATOR, op);
    m_formats.insert(LEXER_STATEMENT_SEPARATOR, op);
    m_formats.insert(LEXER_LH_EXPR, op);
    m_formats.insert(LEXER_RH_EXPR, op);
    m_formats.insert(LEXER_LH_BLOCK, op);
    m_formats.insert(LEXER_RH_BLOCK, op);
    m_formats.insert(LEXER_LH_CODEBLOCK, op);
    m_formats.insert(LEXER_RH_CODEBLOCK, op);

    QTextCharFormat identifier;
    identifier.setForeground(QColor(QStringLiteral("#374151")));
    m_formats.insert(LEXER_IDENTIFIER, identifier);
    m_formats.insert(LEXER_FUNCTION_IDENTIFIER, identifier);
    m_formats.insert(LEXER_TYPE_IDENTIFIER, identifier);
    m_formats.insert(LEXER_CONSTANT_IDENTIFIER, identifier);

    QTextCharFormat error;
    error.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
    error.setUnderlineColor(QColor(QStringLiteral("#c62828")));
    error.setBackground(QColor(QStringLiteral("#fff1f2")));
    m_formats.insert(LEXER_UNKNOWN, error);
    m_formats.insert(SYNTAX_ERROR, error);
#endif
}

QString ScriptSyntaxHighlighter::configuredCommandForEngine() const
{
#if CP_HAS_DSLSH
    if (m_hasCommandOverride) {
        return m_commandOverride.trimmed();
    }

    const QString fileType = ScriptHighlighterConfig::fileTypeForEngine(m_engineId);
    return ScriptHighlighterConfig::loadCommands().value(fileType).trimmed();
#else
    return QString();
#endif
}

QVector<ScriptHighlighterDiagnostic> ScriptSyntaxHighlighter::diagnosticsFromCache() const
{
    QVector<ScriptHighlighterDiagnostic> diagnostics;

#if CP_HAS_DSLSH
    QSet<QString> seen;
    for (int line = 0; line < m_cachedExternalTokenTypes.size(); ++line) {
        const QVector<ScriptHighlightCell>& cells = m_cachedExternalTokenTypes.at(line);
        for (int column = 0; column < cells.size(); ++column) {
            const ScriptHighlightCell& cell = cells.at(column);
            if (cell.severity != CB_WARNING && cell.severity != CB_ERROR && cell.message.isEmpty()) {
                continue;
            }

            const QString key = QStringLiteral("%1:%2:%3:%4")
                                    .arg(line)
                                    .arg(column)
                                    .arg(cell.severity)
                                    .arg(cell.message);
            if (seen.contains(key)) {
                continue;
            }
            seen.insert(key);
            diagnostics.append({line, column, cell.severity, cell.message});
        }
    }
#endif

    return diagnostics;
}

QTextCharFormat ScriptSyntaxHighlighter::formatForToken(int tokenType) const
{
    return m_formats.value(tokenType);
}

QTextCharFormat ScriptSyntaxHighlighter::formatForCell(const ScriptHighlightCell& cell) const
{
    QTextCharFormat format = formatForToken(cell.tokenType);

#if CP_HAS_DSLSH
    if (cell.severity == CB_WARNING || cell.severity == CB_ERROR || !cell.message.isEmpty()) {
        format.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
        format.setUnderlineColor(cell.severity == CB_WARNING
                                     ? QColor(QStringLiteral("#b7791f"))
                                     : QColor(QStringLiteral("#c62828")));
        format.setBackground(cell.severity == CB_WARNING
                                 ? QColor(QStringLiteral("#fffbeb"))
                                 : QColor(QStringLiteral("#fff1f2")));
    }
#endif

    if (!cell.message.isEmpty()) {
        format.setToolTip(cell.message);
    }

    return format;
}

void ScriptSyntaxHighlighter::applyTokenRuns(const QVector<ScriptHighlightCell>& tokenTypes, int textLength)
{
    const int limit = std::min(static_cast<int>(tokenTypes.size()), textLength);
    if (limit <= 0) {
        return;
    }

    int runStart = 0;
    ScriptHighlightCell runCell = tokenTypes.at(0);

    auto sameRun = [](const ScriptHighlightCell& left, const ScriptHighlightCell& right) {
        return left.tokenType == right.tokenType
            && left.severity == right.severity
            && left.message == right.message;
    };

    auto flushRun = [&](int end) {
        if (end <= runStart) {
            return;
        }
        const QTextCharFormat format = formatForCell(runCell);
        if (format.isValid()) {
            setFormat(runStart, end - runStart, format);
        }
    };

    for (int i = 1; i < limit; ++i) {
        const ScriptHighlightCell cell = tokenTypes.at(i);
        if (!sameRun(cell, runCell)) {
            flushRun(i);
            runStart = i;
            runCell = cell;
        }
    }
    flushRun(limit);
}
