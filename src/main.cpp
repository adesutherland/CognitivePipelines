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
#include <QIcon>
#include <QDir>
#include <QDebug>
#include <QFile>
#include "mainwindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

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

    // ========================================================================
    // RESOURCE DIAGNOSTICS: Inspect Qt Resource System at startup
    // ========================================================================
    qCritical() << "=== RESOURCE DIAGNOSTICS START ===";
    
    // 1. List Root Resources
    qCritical() << "Listing root resource directory (':/')...";
    QDir rootRes(":/");
    if (rootRes.exists()) {
        QStringList entries = rootRes.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
        if (entries.isEmpty()) {
            qCritical() << "  WARNING: Root resource directory exists but is EMPTY!";
        } else {
            qCritical() << "  Root resource entries found:" << entries.size();
            for (const QString& entry : entries) {
                qCritical() << "    -" << entry;
            }
        }
    } else {
        qCritical() << "  ERROR: Root resource directory ':/' does NOT exist!";
    }
    
    // 2. Explicit Check for DefaultStyle.json
    qCritical() << "Checking for ':/DefaultStyle.json'...";
    if (QFile::exists(":/DefaultStyle.json")) {
        qCritical() << "  STATUS: FOUND - ':/DefaultStyle.json' exists";
    } else {
        qCritical() << "  STATUS: MISSING - ':/DefaultStyle.json' does NOT exist";
    }
    
    // 3. Deep Scan: Check for QtNodes folder and list its contents
    qCritical() << "Checking for QtNodes resource folder...";
    QDir qtNodesDir(":/QtNodes");
    if (qtNodesDir.exists()) {
        qCritical() << "  QtNodes folder FOUND at ':/QtNodes'";
        QStringList qtNodesEntries = qtNodesDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
        if (qtNodesEntries.isEmpty()) {
            qCritical() << "    WARNING: QtNodes folder exists but is EMPTY!";
        } else {
            qCritical() << "    QtNodes entries found:" << qtNodesEntries.size();
            for (const QString& entry : qtNodesEntries) {
                qCritical() << "      -" << entry;
            }
        }
    } else {
        qCritical() << "  QtNodes folder NOT FOUND at ':/QtNodes'";
        // Try alternative paths
        QStringList alternativePaths = {":/qtnodes", ":/QtNodes/", ":/resources/QtNodes"};
        for (const QString& altPath : alternativePaths) {
            if (QDir(altPath).exists()) {
                qCritical() << "  Alternative path FOUND:" << altPath;
                QStringList altEntries = QDir(altPath).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
                for (const QString& entry : altEntries) {
                    qCritical() << "    -" << entry;
                }
            }
        }
    }
    
    qCritical() << "=== RESOURCE DIAGNOSTICS END ===";
    // ========================================================================

    MainWindow w;
    w.show();

    return app.exec();
}
