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

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QDebug>
#include <QLoggingCategory>
#include "logging_categories.h"
#include "mainwindow.h"
#include "ModelCapsRegistry.h"

int main(int argc, char* argv[]) {
    QCoreApplication::setOrganizationName(QStringLiteral("CognitivePipelines"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("cognitivepipelines.com"));
    QCoreApplication::setApplicationName(QStringLiteral("CognitivePipelines"));

    // By default, silence debug-level logs for our app categories unless the user
    // explicitly opts in via QT_LOGGING_RULES. This keeps the main app console clean
    // while preserving warnings/criticals.
    if (qEnvironmentVariableIsEmpty("QT_LOGGING_RULES")) {
        QLoggingCategory::setFilterRules(QStringLiteral(
            "cp.*.debug=false" // hide qCDebug from all cp.* categories
        ));
    }

    // Default: silence our application logging categories unless the user overrides
    // via QT_LOGGING_RULES. This keeps the main app console clean by default.
    if (qEnvironmentVariableIsEmpty("QT_LOGGING_RULES")) {
        QLoggingCategory::setFilterRules(QStringLiteral(
            "cp.*.debug=false\n"
            "cp.*.info=false"));
    }

    QApplication app(argc, argv);

    // Silence our categorized logs by default in the main app unless explicitly enabled
    // via QT_LOGGING_RULES. This keeps normal runs clean while preserving opt-in verbosity.
    if (qEnvironmentVariableIsEmpty("QT_LOGGING_RULES")) {
        QLoggingCategory::setFilterRules(QStringLiteral(
            "cp.*.debug=false\n"
            "cp.*.info=false\n"
        ));
    }

    qCDebug(cp_registry) << "Initializing Model Capabilities Registry...";
    ModelCapsRegistry::instance().loadFromFile(":/resources/model_caps.json");

    // Set application icon (cross-platform)
    // Note: Using PNG for macOS to avoid "skipping unknown tag type" warnings
    // from Qt's ICNS plugin when parsing complex .icns files with JPEG2000 compression.
    // The .icns file is still used by macOS for the app bundle icon via Info.plist.
#ifdef Q_OS_WIN
    app.setWindowIcon(QIcon(":/packaging/windows/CognitivePipelines.ico"));
#else
    // macOS, Linux, and other platforms - use PNG
    app.setWindowIcon(QIcon(":/packaging/linux/CognitivePipelines.png"));
#endif

    MainWindow w;
    w.show();

    return app.exec();
}
