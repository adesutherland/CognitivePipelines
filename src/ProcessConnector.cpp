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
#include "ProcessConnector.h"
#include "ProcessConnectorPropertiesWidget.h"

#include <QJsonObject>
#include <QtConcurrent/QtConcurrent>
#include <QDebug>

#include <QProcess>

NodeDescriptor ProcessConnector::getDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("process-connector");
    desc.name = QStringLiteral("Process Connector");
    desc.category = QStringLiteral("Processes");

    // Input pin: stdin (text)
    PinDefinition in;
    in.direction = PinDirection::Input;
    in.id = QString::fromLatin1(kInStdin);
    in.name = QStringLiteral("stdin");
    in.type = QStringLiteral("text");
    desc.inputPins.insert(in.id, in);

    // Output pin: stdout (text)
    PinDefinition out1;
    out1.direction = PinDirection::Output;
    out1.id = QString::fromLatin1(kOutStdout);
    out1.name = QStringLiteral("stdout");
    out1.type = QStringLiteral("text");
    desc.outputPins.insert(out1.id, out1);

    // Output pin: stderr (text)
    PinDefinition out2;
    out2.direction = PinDirection::Output;
    out2.id = QString::fromLatin1(kOutStderr);
    out2.name = QStringLiteral("stderr");
    out2.type = QStringLiteral("text");
    desc.outputPins.insert(out2.id, out2);

    return desc;
}

QWidget* ProcessConnector::createConfigurationWidget(QWidget* parent)
{
    if (!propertiesWidget) {
        propertiesWidget = new ProcessConnectorPropertiesWidget(parent);
        // initialize UI from current state
        propertiesWidget->setCommand(m_command);
        // connect UI -> node
        QObject::connect(propertiesWidget, &ProcessConnectorPropertiesWidget::commandChanged,
                         this, &ProcessConnector::onCommandChanged);
    }
    return propertiesWidget;
}

TokenList ProcessConnector::execute(const TokenList& incomingTokens)
{
    // Merge incoming tokens into a single DataPacket
    DataPacket inputs;
    for (const auto& token : incomingTokens) {
        for (auto it = token.data.cbegin(); it != token.data.cend(); ++it) {
            inputs.insert(it.key(), it.value());
        }
    }

    // Gather stdin from inputs (optional)
    const QString stdinText = inputs.value(QString::fromLatin1(kInStdin)).toString();
    const QString command = m_command.trimmed();

    QFuture<DataPacket> fut = QtConcurrent::run([stdinText, command]() -> DataPacket {
        DataPacket packet;
        const QString outKey = QString::fromLatin1(kOutStdout);
        const QString errKey = QString::fromLatin1(kOutStderr);

        if (command.isEmpty()) {
            const QString msg = QStringLiteral("ERROR: Command is empty.");
            packet.insert(outKey, QString());
            packet.insert(errKey, msg);
            qWarning() << "ProcessConnector:" << msg;
            return packet;
        }

        QProcess proc;
        proc.setProcessChannelMode(QProcess::SeparateChannels);

        // Build command by splitting the user-provided command string into program + args
        // Avoid shell wrappers (cmd/sh) to prevent quoting issues across platforms.
        QStringList tokens = QProcess::splitCommand(command);
        if (tokens.isEmpty()) {
            const QString msg = QStringLiteral("ERROR: Invalid command: '") + command + QStringLiteral("'");
            packet.insert(outKey, QString());
            packet.insert(errKey, msg);
            qWarning() << "ProcessConnector:" << msg;
            return packet;
        }
        const QString program = tokens.takeFirst();
        const QStringList args = tokens;
        proc.setProgram(program);
        proc.setArguments(args);

        // Start the process
        proc.start();
        if (!proc.waitForStarted(10000)) { // 10s to start
            const QString err = QStringLiteral("Failed to start process: ") + proc.errorString();
            qWarning() << "ProcessConnector:" << err
                       << ", qprocess error =" << static_cast<int>(proc.error())
                       << ", state =" << static_cast<int>(proc.state());
            packet.insert(outKey, QString());
            packet.insert(errKey, err);
            return packet;
        }

        // Feed stdin (if any) and close write channel to signal EOF
        if (!stdinText.isEmpty()) {
            const QByteArray bytes = stdinText.toUtf8();
            proc.write(bytes);
        }
        proc.closeWriteChannel();

        // Wait for process to finish (60s default to mirror other connectors)
        if (!proc.waitForFinished(60000)) {
            qWarning() << "ProcessConnector: process timeout, killing...";
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

    const DataPacket out = fut.result();

    ExecutionToken token;
    token.data = out;

    TokenList result;
    result.push_back(std::move(token));
    return result;
}

QJsonObject ProcessConnector::saveState() const
{
    QJsonObject state;
    state.insert(QStringLiteral("command"), m_command);
    return state;
}

void ProcessConnector::loadState(const QJsonObject& state)
{
    if (state.contains(QStringLiteral("command"))) {
        m_command = state.value(QStringLiteral("command")).toString();
        if (propertiesWidget) {
            propertiesWidget->setCommand(m_command);
        }
    }
}

void ProcessConnector::onCommandChanged(const QString &newCommand)
{
    if (m_command == newCommand) return;
    m_command = newCommand;
}
