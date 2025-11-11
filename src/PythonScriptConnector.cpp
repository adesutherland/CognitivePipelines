//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "PythonScriptConnector.h"
#include "PythonScriptConnectorPropertiesWidget.h"

#include <QLineEdit>
#include <QTextEdit>
#include <QtConcurrent/QtConcurrent>
#include <QProcess>
#include <QTemporaryFile>
#include <QDebug>

PythonScriptConnector::PythonScriptConnector(QObject* parent)
    : QObject(parent)
{
    // Default executable includes unbuffered flag for timely stdout flushing
    m_executable = QStringLiteral("python3 -u");
}

NodeDescriptor PythonScriptConnector::GetDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("python-script");
    desc.name = QStringLiteral("Python Script");
    desc.category = QStringLiteral("Scripting");

    // Input pin: stdin (text)
    PinDefinition in;
    in.direction = PinDirection::Input;
    in.id = QStringLiteral("stdin");
    in.name = QStringLiteral("stdin");
    in.type = QStringLiteral("text");
    desc.inputPins.insert(in.id, in);

    // Output pin: stdout (text)
    PinDefinition outStdout;
    outStdout.direction = PinDirection::Output;
    outStdout.id = QStringLiteral("stdout");
    outStdout.name = QStringLiteral("stdout");
    outStdout.type = QStringLiteral("text");
    desc.outputPins.insert(outStdout.id, outStdout);

    // Output pin: stderr (text)
    PinDefinition outStderr;
    outStderr.direction = PinDirection::Output;
    outStderr.id = QStringLiteral("stderr");
    outStderr.name = QStringLiteral("stderr");
    outStderr.type = QStringLiteral("text");
    desc.outputPins.insert(outStderr.id, outStderr);

    return desc;
}

QWidget* PythonScriptConnector::createConfigurationWidget(QWidget* parent)
{
    auto* widget = new PythonScriptConnectorPropertiesWidget(parent);

    // Initialize the UI from our current state
    if (!m_executable.isEmpty()) {
        if (auto* exeEdit = widget->findChild<QLineEdit*>(QString(), Qt::FindDirectChildrenOnly)) {
            // Not using objectName; this finds first QLineEdit direct child
            exeEdit->setText(m_executable);
        } else {
            // Fallback: try any QLineEdit in the subtree
            if (auto* exeAny = widget->findChild<QLineEdit*>()) {
                exeAny->setText(m_executable);
            }
        }
    }
    if (!m_scriptContent.isEmpty()) {
        if (auto* scriptEdit = widget->findChild<QTextEdit*>(QString(), Qt::FindDirectChildrenOnly)) {
            scriptEdit->setPlainText(m_scriptContent);
        } else {
            if (auto* scriptAny = widget->findChild<QTextEdit*>()) {
                scriptAny->setPlainText(m_scriptContent);
            }
        }
    }

    // Wire signals to keep the state in sync
    connect(widget, &PythonScriptConnectorPropertiesWidget::executableChanged,
            this, &PythonScriptConnector::onExecutableChanged);
    connect(widget, &PythonScriptConnectorPropertiesWidget::scriptContentChanged,
            this, &PythonScriptConnector::onScriptContentChanged);

    return widget;
}

QFuture<DataPacket> PythonScriptConnector::Execute(const DataPacket& inputs)
{
    // Gather stdin from inputs and capture current config
    const QString stdinText = inputs.value(QStringLiteral("stdin")).toString();
    const QString executable = m_executable.trimmed();
    const QString scriptContent = m_scriptContent; // may be empty; we'll still attempt to run

    return QtConcurrent::run([stdinText, executable, scriptContent]() -> DataPacket {
        DataPacket packet;
        const QString outKey = QStringLiteral("stdout");
        const QString errKey = QStringLiteral("stderr");

        if (executable.isEmpty()) {
            const QString msg = QStringLiteral("ERROR: Python executable/command is empty.");
            packet.insert(outKey, QString());
            packet.insert(errKey, msg);
            qWarning() << "PythonScriptConnector:" << msg;
            return packet;
        }

        // Create a temporary file for the script content
        QTemporaryFile tempFile;
        if (!tempFile.open()) {
            const QString msg = QStringLiteral("Failed to create temporary file for script: ") + tempFile.errorString();
            packet.insert(outKey, QString());
            packet.insert(errKey, msg);
            qWarning() << "PythonScriptConnector:" << msg;
            return packet;
        }

        // Write script content to temp file
        {
            const QByteArray scriptBytes = scriptContent.toUtf8();
            if (tempFile.write(scriptBytes) != scriptBytes.size()) {
                const QString msg = QStringLiteral("Failed to write script to temporary file: ") + tempFile.errorString();
                packet.insert(outKey, QString());
                packet.insert(errKey, msg);
                qWarning() << "PythonScriptConnector:" << msg;
                return packet;
            }
            tempFile.flush();
            tempFile.close();
        }

        const QString scriptPath = tempFile.fileName();

        // Prepare process
        QProcess proc;
        proc.setProcessChannelMode(QProcess::SeparateChannels);

        // Build command: <executable> <scriptPath>
#if defined(Q_OS_WIN)
        // Quote the script path for cmd
        const QString cmd = executable + QStringLiteral(" ") + QStringLiteral("\"") + scriptPath + QStringLiteral("\"");
        proc.setProgram(QStringLiteral("cmd"));
        proc.setArguments({QStringLiteral("/C"), cmd});
#else
        // Single-quote the path for POSIX shell
        const QString cmd = executable + QStringLiteral(" '") + scriptPath + QStringLiteral("'");
        proc.setProgram(QStringLiteral("/bin/sh"));
        proc.setArguments({QStringLiteral("-lc"), cmd});
#endif

        // Start the process
        proc.start();
        if (!proc.waitForStarted(10000)) { // 10s to start
            packet.insert(outKey, QString());
            packet.insert(errKey, QStringLiteral("Failed to start process: ") + proc.errorString());
            return packet;
        }

        // Write stdin and close
        if (!stdinText.isEmpty()) {
            const QByteArray bytes = stdinText.toUtf8();
            Q_UNUSED(proc.write(bytes));
        }
        proc.closeWriteChannel();

        // Wait for finish with timeout (60s)
        if (!proc.waitForFinished(60000)) {
            proc.kill();
            proc.waitForFinished();
            packet.insert(outKey, QString());
            packet.insert(errKey, QStringLiteral("Process timed out"));
            return packet;
        }

        const QString stdoutStr = QString::fromUtf8(proc.readAllStandardOutput());
        const QString stderrStr = QString::fromUtf8(proc.readAllStandardError());

        packet.insert(outKey, stdoutStr);
        packet.insert(errKey, stderrStr);
        return packet;
    });
}

QJsonObject PythonScriptConnector::saveState() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("executable"), m_executable);
    obj.insert(QStringLiteral("script"), m_scriptContent);
    return obj;
}

void PythonScriptConnector::loadState(const QJsonObject& data)
{
    if (data.contains(QStringLiteral("executable")) && data.value(QStringLiteral("executable")).isString()) {
        m_executable = data.value(QStringLiteral("executable")).toString();
    }
    if (data.contains(QStringLiteral("script")) && data.value(QStringLiteral("script")).isString()) {
        m_scriptContent = data.value(QStringLiteral("script")).toString();
    }
}

void PythonScriptConnector::onExecutableChanged(const QString& executable)
{
    m_executable = executable;
}

void PythonScriptConnector::onScriptContentChanged(const QString& scriptContent)
{
    m_scriptContent = scriptContent;
}
