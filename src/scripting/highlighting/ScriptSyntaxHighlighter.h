//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//

#pragma once

#include <QHash>
#include <QMap>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTimer>
#include <QVector>

class QTextDocument;

struct ScriptHighlightCell {
    int tokenType = 0;
    int severity = 0;
    QString message;
};

struct ScriptHighlighterDiagnostic {
    int line = 0;
    int column = 0;
    int severity = 0;
    QString message;
};

/**
 * @brief Optional DSLSH-backed syntax highlighter for Universal Script editors.
 *
 * The class is intentionally safe to instantiate without DSLSH. In that case
 * highlightBlock() is a no-op and the editor remains plain text.
 */
class ScriptSyntaxHighlighter : public QSyntaxHighlighter {
public:
    explicit ScriptSyntaxHighlighter(QTextDocument* document);
    ~ScriptSyntaxHighlighter() override;

    void setEngineId(const QString& engineId);
    void setExternalCommandOverrideForTesting(const QString& command);
    static bool backendAvailable();
    static void rehighlightOpenEditors();
    static bool checkHighlighterCommand(const QString& fileType, const QString& command, QString* message = nullptr);
    static QVector<ScriptHighlighterDiagnostic> collectDiagnostics(
        const QString& fileType,
        const QString& command,
        const QString& source,
        QString* message = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    void initFormats();
    void highlightWithEmergencyRules(const QString& text);
    bool ensureExternalParseCache();
    bool startExternalParse(const QString& content, const QString& command);
    bool isPendingExternalParseComplete() const;
    void pollPendingExternalParse();
    void clearPendingExternalParse(bool waitForCompletion);
    void copyTokenTypesFromCodeBuffer(const void* codeBuffer);
    bool parseWithExternalCommand(const QString& content, const QString& command, QString* message = nullptr);
    QString configuredCommandForEngine() const;
    QVector<ScriptHighlighterDiagnostic> diagnosticsFromCache() const;
    QTextCharFormat formatForToken(int tokenType) const;
    QTextCharFormat formatForCell(const ScriptHighlightCell& cell) const;
    void applyTokenRuns(const QVector<ScriptHighlightCell>& tokenTypes, int textLength);

    QString m_engineId;
    bool m_hasCommandOverride = false;
    QString m_commandOverride;
    QHash<int, QTextCharFormat> m_formats;
    QString m_cachedExternalEngineId;
    QString m_cachedExternalCommand;
    QString m_cachedExternalContent;
    QVector<QVector<ScriptHighlightCell>> m_cachedExternalTokenTypes;
    bool m_cachedExternalValid = false;

    QString m_pendingExternalEngineId;
    QString m_pendingExternalCommand;
    QString m_pendingExternalContent;
    void* m_pendingCodeBuffer = nullptr;
    void* m_pendingCommunication = nullptr;
    QTimer m_parsePollTimer;
};
