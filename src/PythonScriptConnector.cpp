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
#include "Logger.h"

PythonScriptConnector::PythonScriptConnector(QObject* parent)
    : QObject(parent)
{
    // Default executable includes unbuffered flag for timely stdout flushing
    m_executable = QStringLiteral("python3 -u");
}

NodeDescriptor PythonScriptConnector::getDescriptor() const
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

TokenList PythonScriptConnector::execute(const TokenList& incomingTokens)
{
    // Merge incoming tokens into a single DataPacket
    DataPacket inputs;
    for (const auto& token : incomingTokens) {
        for (auto it = token.data.cbegin(); it != token.data.cend(); ++it) {
            inputs.insert(it.key(), it.value());
        }
    }

    // Gather stdin from inputs and capture current config
    const QString stdinText = inputs.value(QStringLiteral("stdin")).toString();
    const QString executable = m_executable.trimmed();
    const QString scriptContent = m_scriptContent; // may be empty; we'll still attempt to run

    DataPacket packet;
    const QString outKey = QStringLiteral("stdout");
    const QString errKey = QStringLiteral("stderr");

    if (executable.isEmpty()) {
        const QString msg = QStringLiteral("ERROR: Python executable/command is empty.");
        packet.insert(outKey, QString());
        packet.insert(errKey, msg);
        CP_WARN << "PythonScriptConnector:" << msg;
    } else {
        // Create a script file in the node-specific output directory
        QString outDir = inputs.value(QStringLiteral("_sys_node_output_dir")).toString();
        if (outDir.isEmpty()) {
            outDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
            if (outDir.isEmpty()) {
                outDir = QDir::tempPath();
            }
        }
        
        QString scriptPath = outDir + QDir::separator() + QStringLiteral("script.py");
        QFile scriptFile(scriptPath);

        if (!scriptFile.open(QIODevice::WriteOnly)) {
            const QString msg = QStringLiteral("Failed to create script file: ") + scriptFile.errorString();
            packet.insert(outKey, QString());
            packet.insert(errKey, msg);
            CP_WARN << "PythonScriptConnector:" << msg;
        } else {
            // Write script content to file
            const QByteArray scriptBytes = scriptContent.toUtf8();
            if (scriptFile.write(scriptBytes) != scriptBytes.size()) {
                const QString msg = QStringLiteral("Failed to write script to file: ") + scriptFile.errorString();
                packet.insert(outKey, QString());
                packet.insert(errKey, msg);
                CP_WARN << "PythonScriptConnector:" << msg;
            } else {
                scriptFile.flush();
                scriptFile.close();

                // Prepare process
                QProcess proc;
                proc.setProcessChannelMode(QProcess::SeparateChannels);

                // Build command by splitting the executable into program + args and appending the script path as its own arg.
                // Avoid shell wrappers (cmd/sh) to prevent quoting issues across platforms.
                QStringList tokens = QProcess::splitCommand(executable);
                if (tokens.isEmpty()) {
                    const QString msg = QStringLiteral("ERROR: Invalid Python executable/command: '") + executable + QStringLiteral("'");
                    packet.insert(outKey, QString());
                    packet.insert(errKey, msg);
                    CP_WARN << "PythonScriptConnector:" << msg;
                } else {
                    const QString program = tokens.takeFirst();
                    QStringList args = tokens;
                    args << scriptPath;
                    proc.setProgram(program);
                    proc.setArguments(args);

                    // Start the process
                    proc.start();
                    const bool started = proc.waitForStarted(10000); // 10s to start
                    if (!started) {
                        const QString err = QStringLiteral("Failed to start process: ") + proc.errorString();
                        CP_WARN << "PythonScriptConnector:" << err
                                   << ", qprocess error =" << static_cast<int>(proc.error())
                                   << ", state =" << static_cast<int>(proc.state());
                        packet.insert(outKey, QString());
                        packet.insert(errKey, err);
                    } else {
                        // Write stdin and close
                        if (!stdinText.isEmpty()) {
                            const QByteArray bytes = stdinText.toUtf8();
                            proc.write(bytes);
                        }
                        proc.closeWriteChannel();

                        // Wait for finish with timeout (60s)
                        if (!proc.waitForFinished(60000)) {
                            CP_WARN << "PythonScriptConnector: process timeout, killing...";
                            proc.kill();
                            proc.waitForFinished();
                            packet.insert(outKey, QString());
                            packet.insert(errKey, QStringLiteral("Process timed out"));
                        } else {
                            const QString stdoutStr = QString::fromUtf8(proc.readAllStandardOutput());
                            const QString stderrStr = QString::fromUtf8(proc.readAllStandardError());

                            packet.insert(outKey, stdoutStr);
                            packet.insert(errKey, stderrStr);
                        }
                    }
                }
            }
        }
    }

    ExecutionToken token;
    token.data = packet;

    TokenList result;
    result.push_back(std::move(token));
    return result;
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
