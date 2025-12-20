#include <gtest/gtest.h>

#include <QApplication>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include <QDebug>
#include <QtGlobal>
#include <cstdio>
#include "test_app.h"

#include "TextInputNode.h"
#include "TextInputPropertiesWidget.h"
#include "PromptBuilderNode.h"
#include "PromptBuilderPropertiesWidget.h"
#include "PythonScriptConnector.h"
#include "PythonScriptConnectorPropertiesWidget.h"
#include <QLineEdit>
#include <QTextEdit>
#include <QTemporaryFile>
#include <QTest>

#include "TextOutputNode.h"
#include "TextOutputPropertiesWidget.h"

#include "DatabaseConnector.h"
#include "DatabaseConnectorPropertiesWidget.h"

// Install a Qt message handler to force all Qt logs to stderr (helps Windows CI capture qInfo/qWarning output)
static void qtTestMessageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    const char* level = "INFO";
    switch (type) {
        case QtDebugMsg: level = "DEBUG"; break;
        case QtInfoMsg: level = "INFO"; break;
        case QtWarningMsg: level = "WARN"; break;
        case QtCriticalMsg: level = "CRIT"; break;
        case QtFatalMsg: level = "FATAL"; break;
    }
    QByteArray loc = msg.toLocal8Bit();
    const char* file = ctx.file ? ctx.file : "?";
    const char* func = ctx.function ? ctx.function : "?";
    fprintf(stderr, "[QT][%s] %s (%s:%d, %s)\n", level, loc.constData(), file, ctx.line, func);
    fflush(stderr);
    if (type == QtFatalMsg) {
        abort();
    }
}

// Ensure a QApplication exists for widget-based property editors used by nodes.
static QApplication* ensureApp()
{
    static bool handlerInstalled = false;
    if (!handlerInstalled) {
        // Direct all Qt logs to stderr and request console logging explicitly
        qInstallMessageHandler(qtTestMessageHandler);
        qputenv("QT_LOGGING_TO_CONSOLE", QByteArray("1"));
        handlerInstalled = true;
    }
    return sharedTestApp();
}

TEST(TextInputNodeTest, EmitsConfiguredTextViaExecute)
{
    ensureApp();

    TextInputNode node;

    // Simulate user setting text through the properties widget (as done in the UI)
    QWidget* w = node.createConfigurationWidget(nullptr);
    ASSERT_NE(w, nullptr);
    auto* props = dynamic_cast<TextInputPropertiesWidget*>(w);
    ASSERT_NE(props, nullptr);

    const QString kText = QStringLiteral("Hello unit tests");
    props->setText(kText);

    // Process queued signals to propagate widget->node updates
    QApplication::processEvents();

    // Execute and verify output packet using V3 token API
    DataPacket in;
    ExecutionToken token;
    token.data = in;
    TokenList tokens;
    tokens.push_back(std::move(token));

    const TokenList outTokens = node.execute(tokens);
    ASSERT_FALSE(outTokens.empty());
    const DataPacket& out = outTokens.front().data;

    ASSERT_TRUE(out.contains(QString::fromLatin1(TextInputNode::kOutputId)));
    EXPECT_EQ(out.value(QString::fromLatin1(TextInputNode::kOutputId)).toString(), kText);

    delete w; // cleanup widget
}

TEST(PromptBuilderNodeTest, FormatsTemplateWithInput)
{
    ensureApp();

    PromptBuilderNode node;

    // Configure template via the properties widget
    QWidget* w = node.createConfigurationWidget(nullptr);
    ASSERT_NE(w, nullptr);
    auto* props = dynamic_cast<PromptBuilderPropertiesWidget*>(w);
    ASSERT_NE(props, nullptr);

    const QString kTpl = QStringLiteral("Hi {input}! This is {input}.");
    props->setTemplateText(kTpl);
    QApplication::processEvents();

    // Build input packet and execute via V3 token API
    DataPacket in;
    in.insert(QString::fromLatin1(PromptBuilderNode::kInputId), QStringLiteral("Alice"));

    ExecutionToken token;
    token.data = in;
    TokenList tokens;
    tokens.push_back(std::move(token));

    const TokenList outTokens = node.execute(tokens);
    ASSERT_FALSE(outTokens.empty());
    const DataPacket& out = outTokens.front().data;
    ASSERT_TRUE(out.contains(QString::fromLatin1(PromptBuilderNode::kOutputId)));
    EXPECT_EQ(out.value(QString::fromLatin1(PromptBuilderNode::kOutputId)).toString(),
              QStringLiteral("Hi Alice! This is Alice."));

    delete w;
}

TEST(TextOutputNodeTest, UpdatesWidgetOnExecute)
{
    ensureApp();

    TextOutputNode node;

    QWidget* w = node.createConfigurationWidget(nullptr);
    ASSERT_NE(w, nullptr);
    auto* props = dynamic_cast<TextOutputPropertiesWidget*>(w);
    ASSERT_NE(props, nullptr);

    // Prepare input packet with text
    const QString kText = QStringLiteral("Hello, TextOutput!");
    DataPacket in;
    in.insert(QString::fromLatin1(TextOutputNode::kInputId), kText);

    // Execute via V3 token API: this posts a queued call to the widget's onSetText
    ExecutionToken token;
    token.data = in;
    TokenList tokens;
    tokens.push_back(std::move(token));

    (void)node.execute(tokens);

    // Allow the event loop to process the queued UI update
    QTest::qWait(100);

    // Find the QTextEdit inside the properties widget and verify contents
    auto* edit = w->findChild<QTextEdit*>();
    ASSERT_NE(edit, nullptr);
    EXPECT_EQ(edit->toPlainText(), kText);

    delete w;
}


TEST(PythonScriptConnectorTest, ExecutesScriptAndHandlesIO)
{
    ensureApp();

    PythonScriptConnector node;

    // Create and configure properties widget (simulate user interaction)
    QWidget* w = node.createConfigurationWidget(nullptr);
    ASSERT_NE(w, nullptr);
    auto* props = dynamic_cast<PythonScriptConnectorPropertiesWidget*>(w);
    ASSERT_NE(props, nullptr);

    // Find underlying editor widgets
    auto* exeEdit = w->findChild<QLineEdit*>();
    auto* scriptEdit = w->findChild<QTextEdit*>();
    ASSERT_NE(exeEdit, nullptr);
    ASSERT_NE(scriptEdit, nullptr);

    // Prefer python3; tests may fall back to python on some systems
    exeEdit->setText(QStringLiteral("python3 -u"));

    const QString kScript = QString::fromLatin1(
        "import sys\n"
        "data = sys.stdin.read()\n"
        "print(data)\n"
        "print(\"PythonScriptConnector: Test message to stderr.\", file=sys.stderr)\n"
    );
    scriptEdit->setPlainText(kScript);

    // Ensure signals propagate to connector state
    QApplication::processEvents();

    // Build input packet for stdin
    const QString kStdin = QStringLiteral("Hello from stdin");
    DataPacket in;
    in.insert(QStringLiteral("stdin"), kStdin);

    // Execute the script via V3 token API
    ExecutionToken token;
    token.data = in;
    TokenList tokens;
    tokens.push_back(std::move(token));

    TokenList outTokens = node.execute(tokens);
    ASSERT_FALSE(outTokens.empty());
    DataPacket out = outTokens.front().data;

    // If python3 isn't available on the system, try python as a fallback
    QString stdoutStr = out.value(QStringLiteral("stdout")).toString();
    QString stderrStr = out.value(QStringLiteral("stderr")).toString();

    const bool maybeNoPython3 = stderrStr.contains(QStringLiteral("command not found"), Qt::CaseInsensitive)
                             || stderrStr.contains(QStringLiteral("is not recognized"), Qt::CaseInsensitive)
                             || stderrStr.contains(QStringLiteral("No such file or directory"), Qt::CaseInsensitive);

    if (maybeNoPython3 || (stdoutStr.isEmpty() && stderrStr.contains(QStringLiteral("python3"), Qt::CaseInsensitive))) {
        exeEdit->setText(QStringLiteral("python -u"));
        QApplication::processEvents();

        ExecutionToken token2;
        token2.data = in;
        TokenList tokens2;
        tokens2.push_back(std::move(token2));

        TokenList outTokens2 = node.execute(tokens2);
        ASSERT_FALSE(outTokens2.empty());
        out = outTokens2.front().data;
        stdoutStr = out.value(QStringLiteral("stdout")).toString();
        stderrStr = out.value(QStringLiteral("stderr")).toString();
    }

    // Basic shape of outputs
    ASSERT_TRUE(out.contains(QStringLiteral("stdout")));
    ASSERT_TRUE(out.contains(QStringLiteral("stderr")));

    // Validate that stdout echoed stdin and stderr contains the test marker
    EXPECT_TRUE(stdoutStr.contains(kStdin));
    EXPECT_TRUE(stderrStr.contains(QStringLiteral("PythonScriptConnector: Test message to stderr.")));

    delete w;
}


TEST(DatabaseConnectorTest, ExecutesQueries)
{
    ensureApp();

    // Use a temporary file to get a unique database path
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    const QString dbPath = tempFile.fileName();
    tempFile.close(); // allow SQLite to open it exclusively if needed

    DatabaseConnector node;

    // Configure via properties widget (simulate user interaction)
    QWidget* w = node.createConfigurationWidget(nullptr);
    ASSERT_NE(w, nullptr);
    auto* props = dynamic_cast<DatabaseConnectorPropertiesWidget*>(w);
    ASSERT_NE(props, nullptr);

    props->setDatabasePath(dbPath);
    QApplication::processEvents();

    // 1) CREATE TABLE
    {
        DataPacket in;
        in.insert(QStringLiteral("sql"), QStringLiteral("CREATE TABLE test (id INT, name TEXT);"));

        ExecutionToken token;
        token.data = in;
        TokenList tokens;
        tokens.push_back(std::move(token));

        const TokenList outTokens = node.execute(tokens);
        ASSERT_FALSE(outTokens.empty());
        const DataPacket& out = outTokens.front().data;
        const QString stderrStr = out.value(QStringLiteral("stderr")).toString();
        EXPECT_TRUE(stderrStr.isEmpty()) << stderrStr.toStdString();
    }

    // 2) INSERT row
    {
        DataPacket in;
        in.insert(QStringLiteral("sql"), QStringLiteral("INSERT INTO test VALUES (1, 'Hello');"));

        ExecutionToken token;
        token.data = in;
        TokenList tokens;
        tokens.push_back(std::move(token));

        const TokenList outTokens = node.execute(tokens);
        ASSERT_FALSE(outTokens.empty());
        const DataPacket& out = outTokens.front().data;
        const QString stderrStr = out.value(QStringLiteral("stderr")).toString();
        EXPECT_TRUE(stderrStr.isEmpty()) << stderrStr.toStdString();
    }

    // 3) SELECT and verify contents
    {
        DataPacket in;
        in.insert(QStringLiteral("sql"), QStringLiteral("SELECT * FROM test;"));

        ExecutionToken token;
        token.data = in;
        TokenList tokens;
        tokens.push_back(std::move(token));

        const TokenList outTokens = node.execute(tokens);
        ASSERT_FALSE(outTokens.empty());
        const DataPacket& out = outTokens.front().data;
        const QString stderrStr = out.value(QStringLiteral("stderr")).toString();
        const QString stdoutStr = out.value(QStringLiteral("stdout")).toString();
        EXPECT_TRUE(stderrStr.isEmpty()) << stderrStr.toStdString();
        EXPECT_TRUE(stdoutStr.contains(QStringLiteral("Hello"))) << stdoutStr.toStdString();
    }

    delete w;
}
