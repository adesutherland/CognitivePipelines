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

#include "ExecutionScriptHost.h"
#include <QStandardPaths>
#include <QDir>

ExecutionScriptHost::ExecutionScriptHost(const DataPacket& inputPacket, DataPacket& outputPacket, QList<QString>& logs)
    : m_inputPacket(inputPacket), m_outputPacket(outputPacket), m_logs(logs)
{
}

void ExecutionScriptHost::log(const QString& message)
{
    m_logs.append(message);

    QString currentLogs = m_outputPacket.value(QStringLiteral("logs")).toString();
    if (!currentLogs.isEmpty()) {
        currentLogs += QStringLiteral("  \n");
    }
    
    // Ensure internal newlines in the message are also treated as Markdown line breaks
    QString formattedMessage = message;
    formattedMessage.replace(QStringLiteral("\n"), QStringLiteral("  \n"));
    
    currentLogs += formattedMessage;
    m_outputPacket.insert(QStringLiteral("logs"), currentLogs);
}

QVariant ExecutionScriptHost::getInput(const QString& key)
{
    return m_inputPacket.value(key);
}

void ExecutionScriptHost::setOutput(const QString& key, const QVariant& value)
{
    m_outputPacket.insert(key, value);
}

void ExecutionScriptHost::setError(const QString& message)
{
    m_logs.append(QString("Error: %1").arg(message));
}

QString ExecutionScriptHost::getTempDir() const
{
    const QString sysKey = QStringLiteral("_sys_run_temp_dir");
    if (m_inputPacket.contains(sysKey)) {
        return m_inputPacket.value(sysKey).toString();
    }

    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempDir.isEmpty()) {
        tempDir = QDir::tempPath();
    }
    return tempDir;
}
