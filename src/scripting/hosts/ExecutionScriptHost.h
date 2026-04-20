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

#pragma once

#include "IScriptHost.h"
#include "CommonDataTypes.h"
#include <QList>
#include <QString>

/**
 * @brief Concrete implementation of IScriptHost that bridges a script engine to the pipeline's data.
 */
class ExecutionScriptHost : public IScriptHost {
public:
    /**
     * @brief Constructs an ExecutionScriptHost.
     * @param inputPacket Const reference to the input data.
     * @param outputPacket Reference to the output data to be populated by the script.
     * @param logs Reference to a list to collect logs and error messages.
     */
    ExecutionScriptHost(const DataPacket& inputPacket, DataPacket& outputPacket, QList<QString>& logs);

    ~ExecutionScriptHost() override = default;

    // IScriptHost interface implementation
    void log(const QString& message) override;
    QVariant getInput(const QString& key) override;
    void setOutput(const QString& key, const QVariant& value) override;
    void setError(const QString& message) override;
    QString getTempDir() const override;

private:
    const DataPacket& m_inputPacket;
    DataPacket& m_outputPacket;
    QList<QString>& m_logs;
};
