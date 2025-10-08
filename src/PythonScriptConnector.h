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

#include "IToolConnector.h"

#include <QString>
#include <QPointer>

/**
 * @brief A concrete IToolConnector that executes a Python script as a separate process.
 *
 * Input protocol (executeAsync):
 *  - Uses the configured script path (from the configuration widget) or, if provided,
 *    the input pin with id "script_path" (QString) to select the script to run.
 *  - All other input pins are serialized to JSON and written to the Python process stdin.
 *
 * Output protocol:
 *  - "output": QVariant — Parsed JSON value from the script's stdout (object/array/primitive),
 *                or the raw stdout text if not valid JSON.
 *  - "stderr": QString — Entire stderr captured from the Python process.
 *  - "exit_code": int — Process exit code (or -1 for timeout/failure to start).
 *  - "error": QString (optional) — Human-readable error on failure (missing script,
 *                timeout, non-zero exit, parse error, etc.).
 */
class PythonScriptConnector : public IToolConnector {
public:
    PythonScriptConnector() = default;
    ~PythonScriptConnector() override = default;

    // IToolConnector interface
    ToolMetadata metadata() const override;
    QWidget* createConfigurationWidget(QWidget* parent) override;
    QFuture<OutputMap> executeAsync(const InputMap& inputs) override;

    // Optional setters useful for programmatic configuration and tests
    void setPythonExecutable(const QString& exe) { pythonExecutable_ = exe; }
    void setScriptPath(const QString& path) { scriptPath_ = path; }
    void setTimeoutMs(int ms) { timeoutMs_ = ms; }

private:
    // Configuration (mutable by the UI)
    QString pythonExecutable_ = QStringLiteral("python3");
    QString scriptPath_;
    int timeoutMs_ = 30000; // 30 seconds default

    QPointer<QWidget> configWidget_;
};
