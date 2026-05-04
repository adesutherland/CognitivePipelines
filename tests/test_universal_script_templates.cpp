#include <gtest/gtest.h>

#include "ScriptHighlighterConfig.h"
#include "ScriptSyntaxHighlighter.h"
#include "UniversalScriptTemplates.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>

namespace {

template<typename Predicate>
bool waitUntil(Predicate predicate, int timeoutMs = 2000)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (predicate()) {
            return true;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(10);
    }
    return predicate();
}

} // namespace

TEST(UniversalScriptTemplatesTest, ProvidesQuickJsTemplate)
{
    const QString script = UniversalScriptTemplates::forEngine(QStringLiteral("quickjs"));
    EXPECT_TRUE(script.contains(QStringLiteral("QuickJS runtime, not Node.js")));
    EXPECT_TRUE(script.contains(QStringLiteral("pipeline.getInput")));
    EXPECT_TRUE(script.contains(QStringLiteral("pipeline.setOutput")));
}

TEST(UniversalScriptTemplatesTest, ProvidesCrexxTemplate)
{
    const QString script = UniversalScriptTemplates::forEngine(QStringLiteral("crexx"));
    EXPECT_TRUE(script.contains(QStringLiteral("options levelb")));
    EXPECT_TRUE(script.contains(QStringLiteral("produce: procedure = .int")));
    EXPECT_TRUE(script.contains(QStringLiteral("arg input = .string[]")));
    EXPECT_TRUE(script.contains(QStringLiteral("input[]")));
    EXPECT_TRUE(script.contains(QStringLiteral("output[i]")));
    EXPECT_TRUE(script.contains(QStringLiteral("errors[1]")));
}

TEST(UniversalScriptTemplatesTest, DetectsManagedTemplates)
{
    EXPECT_TRUE(UniversalScriptTemplates::isManagedTemplate(QString()));
    EXPECT_TRUE(UniversalScriptTemplates::isManagedTemplate(UniversalScriptTemplates::quickJs()));
    EXPECT_TRUE(UniversalScriptTemplates::isManagedTemplate(UniversalScriptTemplates::crexx()));
    EXPECT_FALSE(UniversalScriptTemplates::isManagedTemplate(QStringLiteral("pipeline.setOutput('output', 'custom');")));
}

TEST(UniversalScriptTemplatesTest, MapsEnginesToHighlighterFileTypes)
{
    EXPECT_EQ(ScriptHighlighterConfig::fileTypeForEngine(QStringLiteral("quickjs")), QStringLiteral(".js"));
    EXPECT_EQ(ScriptHighlighterConfig::fileTypeForEngine(QStringLiteral("crexx")), QStringLiteral(".rexx"));
}

TEST(UniversalScriptTemplatesTest, ProvidesHighlighterCommandRows)
{
    const QMap<QString, QString> commands = ScriptHighlighterConfig::defaultCommands();
    EXPECT_TRUE(commands.contains(QStringLiteral(".js")));
    EXPECT_TRUE(commands.contains(QStringLiteral(".rexx")));
}

#if defined(CP_HAS_DSLSH) && CP_HAS_DSLSH && defined(CP_HAS_CREXX) && CP_HAS_CREXX
TEST(UniversalScriptTemplatesTest, ConfiguredCrexxHighlighterSmoke)
{
    QString message;
    EXPECT_TRUE(ScriptSyntaxHighlighter::checkHighlighterCommand(
        QStringLiteral(".rexx"),
        QString::fromUtf8(CP_CREXX_RXC_PATH),
        &message)) << message.toStdString();
    EXPECT_TRUE(message.contains(QStringLiteral("diagnostic"), Qt::CaseInsensitive)) << message.toStdString();
    EXPECT_FALSE(message.contains(QStringLiteral("responsded"), Qt::CaseInsensitive)) << message.toStdString();
    EXPECT_FALSE(message.contains(QStringLiteral("dignostig"), Qt::CaseInsensitive)) << message.toStdString();
}

TEST(UniversalScriptTemplatesTest, CrexxHighlighterReturnsDiagnosticMessages)
{
    QString message;
    const QVector<ScriptHighlighterDiagnostic> diagnostics = ScriptSyntaxHighlighter::collectDiagnostics(
        QStringLiteral(".rexx"),
        QString::fromUtf8(CP_CREXX_RXC_PATH),
        QStringLiteral("options levelb\nsay 'hello'\nif then\nreturn 0\n"),
        &message);

    ASSERT_FALSE(diagnostics.isEmpty()) << message.toStdString();

    bool foundMessage = false;
    for (const ScriptHighlighterDiagnostic& diagnostic : diagnostics) {
        if (diagnostic.message.contains(QStringLiteral("SYNTAX_ERROR"))
            || diagnostic.message.contains(QStringLiteral("UNEXPECTED_THEN"))) {
            foundMessage = true;
            break;
        }
    }
    EXPECT_TRUE(foundMessage) << message.toStdString();
}

TEST(UniversalScriptTemplatesTest, CrexxHighlighterAppliesExternalFormatsToDocument)
{
    QTextDocument document;
    document.setPlainText(QStringLiteral("options levelb\nsay 'hello'\nif then\nreturn 0\n"));

    ScriptSyntaxHighlighter highlighter(&document);
    highlighter.setEngineId(QStringLiteral("crexx"));
    highlighter.setExternalCommandOverrideForTesting(QString::fromUtf8(CP_CREXX_RXC_PATH));
    highlighter.rehighlight();

    const bool sawPaintedSyntax = waitUntil([&]() {
        const QTextBlock sayBlock = document.findBlockByNumber(1);
        if (!sayBlock.isValid() || !sayBlock.layout()) {
            return false;
        }
        for (const QTextLayout::FormatRange& range : sayBlock.layout()->formats()) {
            if (range.start == 0 && range.length >= 3 && range.format.foreground().style() != Qt::NoBrush) {
                return true;
            }
        }
        return false;
    });
    EXPECT_TRUE(sawPaintedSyntax);
}

TEST(UniversalScriptTemplatesTest, CrexxHighlighterAppliesErrorTooltipsToDocument)
{
    QTextDocument document;
    document.setPlainText(QStringLiteral("options levelb\nsay 'hello'\nif then\nreturn 0\n"));

    ScriptSyntaxHighlighter highlighter(&document);
    highlighter.setEngineId(QStringLiteral("crexx"));
    highlighter.setExternalCommandOverrideForTesting(QString::fromUtf8(CP_CREXX_RXC_PATH));
    highlighter.rehighlight();

    EXPECT_TRUE(waitUntil([&]() {
        const QTextBlock errorBlock = document.findBlockByNumber(2);
        if (!errorBlock.isValid() || !errorBlock.layout()) {
            return false;
        }

        bool sawTooltip = false;
        bool sawUnderline = false;
        for (const QTextLayout::FormatRange& range : errorBlock.layout()->formats()) {
            sawTooltip = sawTooltip
                || range.format.toolTip().contains(QStringLiteral("SYNTAX_ERROR"))
                || range.format.toolTip().contains(QStringLiteral("UNEXPECTED_THEN"));
            sawUnderline = sawUnderline || range.format.underlineStyle() != QTextCharFormat::NoUnderline;
        }
        return sawTooltip && sawUnderline;
    }));
}
#endif
