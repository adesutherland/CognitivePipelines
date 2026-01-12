#include <gtest/gtest.h>

#include <QApplication>
#include <QtTest>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QString>
#include "Logger.h"

namespace {

void configureQtPlatform()
{
    // Respect any caller-provided platform selection.
    if (!qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        return;
    }

    const QString pluginDir = QLibraryInfo::path(QLibraryInfo::PluginsPath)
                              + QStringLiteral("/platforms");

#ifdef Q_OS_MAC
    // On macOS, QWebEngine (used by MermaidRenderService) requires the 'cocoa' platform 
    // to render correctly. The 'offscreen' platform results in blank images.
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("cocoa"));
#elif defined(Q_OS_WIN)
    const QString offscreenPlugin = QStringLiteral("qoffscreen.dll");
    const QString candidate = pluginDir + QLatin1Char('/') + offscreenPlugin;
    if (QFileInfo::exists(candidate)) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    }
#else
    const QString offscreenPlugin = QStringLiteral("libqoffscreen.so");
    const QString candidate = pluginDir + QLatin1Char('/') + offscreenPlugin;
    if (QFileInfo::exists(candidate)) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    }
#endif
}

} // namespace

int main(int argc, char** argv)
{
    configureQtPlatform();

    // Parse command line arguments for debug flag
    bool debugEnabled = false;
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("-d")) {
            debugEnabled = true;
            break;
        }
    }
    AppLogHelper::setGlobalDebugEnabled(debugEnabled);

    QApplication app(argc, argv);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
