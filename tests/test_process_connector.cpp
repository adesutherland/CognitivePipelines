#include <gtest/gtest.h>

#include <QApplication>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include <QLineEdit>

#include "ProcessConnector.h"
#include "ProcessConnectorPropertiesWidget.h"

// Local ensureApp for this test file to create a minimal QApplication when needed
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

TEST(ProcessConnectorTest, ExecutesCommandAndHandlesIO)
{
    ensureApp();

    ProcessConnector node;

    QWidget* w = node.createConfigurationWidget(nullptr);
    ASSERT_NE(w, nullptr);
    auto* props = dynamic_cast<ProcessConnectorPropertiesWidget*>(w);
    ASSERT_NE(props, nullptr);

    // We'll use Python to ensure cross-platform behavior similar to PythonScriptConnector test.
    // One-liner reads stdin and writes it to stdout, plus a message to stderr.
    const QString pyCmd3 = QStringLiteral("python3 -u -c \"import sys; d=sys.stdin.read(); print(d); print('ProcessConnector: Test stderr', file=sys.stderr)\"");
    const QString pyCmd2 = QStringLiteral("python -u -c \"import sys; d=sys.stdin.read(); print(d); print('ProcessConnector: Test stderr', file=sys.stderr)\"");

    props->setCommand(pyCmd3);

    QApplication::processEvents();

    const QString kStdin = QStringLiteral("Hello PC stdin");
    DataPacket in;
    in.insert(QString::fromLatin1(ProcessConnector::kInStdin), kStdin);

    QFuture<DataPacket> fut = node.Execute(in);
    fut.waitForFinished();
    DataPacket out = fut.result();

    QString stdoutStr = out.value(QString::fromLatin1(ProcessConnector::kOutStdout)).toString();
    QString stderrStr = out.value(QString::fromLatin1(ProcessConnector::kOutStderr)).toString();

    const bool maybeNoPython3 = stderrStr.contains(QStringLiteral("command not found"), Qt::CaseInsensitive)
                             || stderrStr.contains(QStringLiteral("is not recognized"), Qt::CaseInsensitive)
                             || stderrStr.contains(QStringLiteral("No such file or directory"), Qt::CaseInsensitive)
                             || stderrStr.contains(QStringLiteral("python3"), Qt::CaseInsensitive);

    if (maybeNoPython3 || stdoutStr.isEmpty()) {
        props->setCommand(pyCmd2);
        QApplication::processEvents();
        QFuture<DataPacket> fut2 = node.Execute(in);
        fut2.waitForFinished();
        out = fut2.result();
        stdoutStr = out.value(QString::fromLatin1(ProcessConnector::kOutStdout)).toString();
        stderrStr = out.value(QString::fromLatin1(ProcessConnector::kOutStderr)).toString();
    }

    ASSERT_TRUE(out.contains(QString::fromLatin1(ProcessConnector::kOutStdout)));
    ASSERT_TRUE(out.contains(QString::fromLatin1(ProcessConnector::kOutStderr)));

    EXPECT_TRUE(stdoutStr.contains(kStdin));
    EXPECT_TRUE(stderrStr.contains(QStringLiteral("ProcessConnector: Test stderr")));

    delete w;
}
