#include <gtest/gtest.h>

#include <QApplication>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>

#include "TextInputNode.h"
#include "TextInputPropertiesWidget.h"
#include "PromptBuilderNode.h"
#include "PromptBuilderPropertiesWidget.h"
#include "PythonScriptConnector.h"
#include "PythonScriptConnectorPropertiesWidget.h"
#include <QLineEdit>
#include <QTextEdit>

// Ensure a QApplication exists for widget-based property editors used by nodes.
static QApplication* ensureApp()
{
    static QApplication* app = nullptr;
    if (!app) {
        static int argc = 1;
        static char appName[] = "unit_tests";
        static char* argv[] = { appName, nullptr };
        app = new QApplication(argc, argv);
    }
    return app;
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

    // Execute and verify output packet
    QFuture<DataPacket> fut = node.Execute({});
    fut.waitForFinished();
    const DataPacket out = fut.result();

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

    // Build input packet and execute
    DataPacket in;
    in.insert(QString::fromLatin1(PromptBuilderNode::kInputId), QStringLiteral("Alice"));

    QFuture<DataPacket> fut = node.Execute(in);
    fut.waitForFinished();

    const DataPacket out = fut.result();
    ASSERT_TRUE(out.contains(QString::fromLatin1(PromptBuilderNode::kOutputId)));
    EXPECT_EQ(out.value(QString::fromLatin1(PromptBuilderNode::kOutputId)).toString(),
              QStringLiteral("Hi Alice! This is Alice."));

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

    // Execute the script
    QFuture<DataPacket> fut = node.Execute(in);
    fut.waitForFinished();
    DataPacket out = fut.result();

    // If python3 isn't available on the system, try python as a fallback
    QString stdoutStr = out.value(QStringLiteral("stdout")).toString();
    QString stderrStr = out.value(QStringLiteral("stderr")).toString();

    const bool maybeNoPython3 = stderrStr.contains(QStringLiteral("command not found"), Qt::CaseInsensitive)
                             || stderrStr.contains(QStringLiteral("is not recognized"), Qt::CaseInsensitive)
                             || stderrStr.contains(QStringLiteral("No such file or directory"), Qt::CaseInsensitive);

    if (maybeNoPython3 || (stdoutStr.isEmpty() && stderrStr.contains(QStringLiteral("python3"), Qt::CaseInsensitive))) {
        exeEdit->setText(QStringLiteral("python -u"));
        QApplication::processEvents();
        QFuture<DataPacket> fut2 = node.Execute(in);
        fut2.waitForFinished();
        out = fut2.result();
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
