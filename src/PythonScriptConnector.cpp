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

#include <QWidget>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QFileDialog>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QVariant>
#include <QPointer>
#include <QPromise>

#include <QProcess>
#include <thread>
#include <mutex>
#include <sstream>

// ---------------- IToolConnector implementation ----------------

ToolMetadata PythonScriptConnector::metadata() const {
    ToolMetadata md;
    md.name = QStringLiteral("Python Script Connector");
    md.description = QStringLiteral("Executes a Python script using boost::process. Inputs are serialized to JSON and provided on stdin. Stdout JSON is returned as output.");

    // Input pins
    {
        PinDefinition p;
        p.id = QStringLiteral("script_path");
        p.name = QStringLiteral("Script Path");
        p.description = QStringLiteral("Path to the Python script. Overrides configured path if provided.");
        p.type = QMetaType::QString;
        p.optional = true;
        md.inputs.push_back(p);
    }
    {
        PinDefinition p;
        p.id = QStringLiteral("data");
        p.name = QStringLiteral("Data");
        p.description = QStringLiteral("Optional QVariantMap of inputs. If not provided, all connected pins (except script_path) are forwarded as JSON.");
        p.type = QMetaType::QVariantMap;
        p.optional = true;
        md.inputs.push_back(p);
    }

    // Output pins
    {
        PinDefinition p;
        p.id = QStringLiteral("output");
        p.name = QStringLiteral("Output");
        p.description = QStringLiteral("Parsed JSON from stdout (or raw text if parsing fails).");
        p.type = QMetaType::QVariant;
        p.optional = false;
        md.outputs.push_back(p);
    }
    {
        PinDefinition p;
        p.id = QStringLiteral("stderr");
        p.name = QStringLiteral("Stderr");
        p.description = QStringLiteral("Captured stderr output from the Python process.");
        p.type = QMetaType::QString;
        p.optional = true;
        md.outputs.push_back(p);
    }
    {
        PinDefinition p;
        p.id = QStringLiteral("exit_code");
        p.name = QStringLiteral("Exit Code");
        p.description = QStringLiteral("Process exit code (-1 for timeout or failure to start).");
        p.type = QMetaType::Int;
        p.optional = true;
        md.outputs.push_back(p);
    }
    {
        PinDefinition p;
        p.id = QStringLiteral("error");
        p.name = QStringLiteral("Error");
        p.description = QStringLiteral("Error message if the process failed, timed out, or returned non-zero exit code.");
        p.type = QMetaType::QString;
        p.optional = true;
        md.outputs.push_back(p);
    }

    return md;
}

QWidget* PythonScriptConnector::createConfigurationWidget(QWidget* parent) {
    if (configWidget_) {
        // If an existing widget exists with a different parent, recreate to ensure proper ownership
        if (configWidget_->parentWidget() != parent) {
            configWidget_->deleteLater();
            configWidget_.clear();
        }
    }

    if (!configWidget_) {
        QWidget* w = new QWidget(parent);
        auto* form = new QFormLayout(w);

        // Python executable
        auto* pyRow = new QWidget(w);
        auto* pyLayout = new QHBoxLayout(pyRow);
        pyLayout->setContentsMargins(0,0,0,0);
        auto* pyEdit = new QLineEdit(pyRow);
        pyEdit->setPlaceholderText(QStringLiteral("python3"));
        pyEdit->setText(pythonExecutable_);
        pyLayout->addWidget(pyEdit);
        form->addRow(QStringLiteral("Python Executable:"), pyRow);

        // Script path with a browse button
        auto* scriptRow = new QWidget(w);
        auto* scriptLayout = new QHBoxLayout(scriptRow);
        scriptLayout->setContentsMargins(0,0,0,0);
        auto* scriptEdit = new QLineEdit(scriptRow);
        scriptEdit->setPlaceholderText(QStringLiteral("/path/to/script.py"));
        scriptEdit->setText(scriptPath_);
        auto* browseBtn = new QPushButton(QStringLiteral("Browse..."), scriptRow);
        scriptLayout->addWidget(scriptEdit);
        scriptLayout->addWidget(browseBtn);
        form->addRow(QStringLiteral("Script Path:"), scriptRow);

        // Timeout (seconds)
        auto* timeoutSpin = new QSpinBox(w);
        timeoutSpin->setRange(0, 24 * 60 * 60); // 0 = no timeout
        timeoutSpin->setSuffix(QStringLiteral(" s"));
        timeoutSpin->setValue(timeoutMs_ / 1000);
        form->addRow(QStringLiteral("Timeout:"), timeoutSpin);

        // Wire up signals
        QObject::connect(pyEdit, &QLineEdit::textChanged, w, [this](const QString& t){
            pythonExecutable_ = t.trimmed().isEmpty() ? QStringLiteral("python3") : t.trimmed();
        });
        QObject::connect(scriptEdit, &QLineEdit::textChanged, w, [this](const QString& t){
            scriptPath_ = t.trimmed();
        });
        QObject::connect(browseBtn, &QPushButton::clicked, w, [this, scriptEdit, w]() {
            const QString f = QFileDialog::getOpenFileName(w, QStringLiteral("Select Python Script"), QString(), QStringLiteral("Python Scripts (*.py);;All Files (*)"));
            if (!f.isEmpty()) scriptEdit->setText(f);
        });
        QObject::connect(timeoutSpin, qOverload<int>(&QSpinBox::valueChanged), w, [this](int secs){
            timeoutMs_ = secs * 1000;
        });

        configWidget_ = w;
    }

    return configWidget_;
}

static IToolConnector::OutputMap makeErrorResult(const QString& message,
                                                 int exitCode,
                                                 const QString& stdoutTxt,
                                                 const QString& stderrTxt) {
    IToolConnector::OutputMap out;
    out.insert(QStringLiteral("error"), message);
    out.insert(QStringLiteral("exit_code"), exitCode);
    if (!stdoutTxt.isEmpty()) out.insert(QStringLiteral("output"), stdoutTxt);
    if (!stderrTxt.isEmpty()) out.insert(QStringLiteral("stderr"), stderrTxt);
    return out;
}

QFuture<IToolConnector::OutputMap> PythonScriptConnector::executeAsync(const InputMap& inputs) {
    QPromise<OutputMap> promise;
    auto future = promise.future();

    // Snapshot configuration to avoid data races with UI updates
    const QString pyExe = pythonExecutable_;
    const QString configuredScript = scriptPath_;
    const int timeoutMs = timeoutMs_;

    std::thread([promise = std::move(promise), inputs, pyExe, configuredScript, timeoutMs]() mutable {
        try {
            // Resolve script path (input overrides config)
            QString scriptPath = inputs.value(QStringLiteral("script_path"), configuredScript).toString();
            if (scriptPath.trimmed().isEmpty()) {
                promise.addResult(makeErrorResult(QStringLiteral("No script path provided."), -1, {}, {}));
                promise.finish();
                return;
            }

            // Prepare JSON payload from inputs (excluding script_path); if a dedicated 'data' pin is provided, prefer it.
            QVariantMap payloadMap;
            if (inputs.contains(QStringLiteral("data")) && inputs.value(QStringLiteral("data")).typeId() == QMetaType::QVariantMap) {
                payloadMap = inputs.value(QStringLiteral("data")).toMap();
            } else {
                for (auto it = inputs.constBegin(); it != inputs.constEnd(); ++it) {
                    if (it.key() == QStringLiteral("script_path")) continue;
                    payloadMap.insert(it.key(), it.value());
                }
            }
            QJsonObject jsonObj;
            for (auto it = payloadMap.constBegin(); it != payloadMap.constEnd(); ++it) {
                jsonObj.insert(it.key(), QJsonValue::fromVariant(it.value()));
            }
            const QByteArray jsonBytes = QJsonDocument(jsonObj).toJson(QJsonDocument::Compact);

            // Run the Python process via QProcess
            QProcess proc;
            proc.setProgram(pyExe);
            proc.setArguments({ scriptPath });
            // Use separate channels to capture both
            proc.setProcessChannelMode(QProcess::SeparateChannels);
            proc.start();

            if (!proc.waitForStarted()) {
                promise.addResult(makeErrorResult(QStringLiteral("Failed to start process: ") + proc.errorString(), -1, {}, {}));
                promise.finish();
                return;
            }

            // Write stdin payload
            proc.write(jsonBytes);
            proc.closeWriteChannel();

            bool finished = true;
            if (timeoutMs > 0) {
                finished = proc.waitForFinished(timeoutMs);
            } else {
                finished = proc.waitForFinished(-1);
            }

            if (!finished) {
                proc.kill();
                proc.waitForFinished();
                promise.addResult(makeErrorResult(QStringLiteral("Execution timed out."), -1,
                                                  QString::fromUtf8(proc.readAllStandardOutput()),
                                                  QString::fromUtf8(proc.readAllStandardError())));
                promise.finish();
                return;
            }

            const int code = proc.exitCode();
            const QString stdoutQt = QString::fromUtf8(proc.readAllStandardOutput());
            const QString stderrQt = QString::fromUtf8(proc.readAllStandardError());

            OutputMap out;
            out.insert(QStringLiteral("exit_code"), code);
            out.insert(QStringLiteral("stderr"), stderrQt);

            // Try to parse stdout as JSON
            QJsonParseError parseErr{};
            const QJsonDocument doc = QJsonDocument::fromJson(stdoutQt.toUtf8(), &parseErr);
            if (parseErr.error == QJsonParseError::NoError) {
                out.insert(QStringLiteral("output"), doc.toVariant());
            } else {
                // Not JSON — return raw text
                out.insert(QStringLiteral("output"), stdoutQt);
            }

            if (code != 0) {
                out.insert(QStringLiteral("error"), QStringLiteral("Script exited with non-zero code: ") + QString::number(code));
            }

            promise.addResult(out);
            promise.finish();
        } catch (const std::exception& ex) {
            promise.addResult(makeErrorResult(QStringLiteral("Exception: ") + QString::fromUtf8(ex.what()), -1, {}, {}));
            promise.finish();
        } catch (...) {
            promise.addResult(makeErrorResult(QStringLiteral("Unknown error during execution."), -1, {}, {}));
            promise.finish();
        }
    }).detach();

    return future;
}
