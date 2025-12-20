#include <gtest/gtest.h>

#include <QApplication>
#include <QtTest>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QString>

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
    const QString offscreenPlugin = QStringLiteral("libqoffscreen.dylib");
    const QString candidate = pluginDir + QLatin1Char('/') + offscreenPlugin;
    if (QFileInfo::exists(candidate)) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    } else {
        // Homebrew Qt on macOS may ship only the cocoa plugin; fall back to it
        // instead of crashing when offscreen is unavailable.
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("cocoa"));
    }
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

    QApplication app(argc, argv);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
