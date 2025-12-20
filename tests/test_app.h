//
// Shared QApplication helper for tests
// Ensures a single QApplication instance is reused across all unit tests to
// avoid multiple application creations (which can crash on Windows).
//

#pragma once

#include <QApplication>
#include <QCoreApplication>

inline QApplication* sharedTestApp(int argc = 0, char** argv = nullptr)
{
    if (auto* existing = qobject_cast<QApplication*>(QCoreApplication::instance())) {
        return existing;
    }

    static int qtArgc = (argc > 0) ? argc : 1;
    static char appName[] = "unit_tests";
    static char* defaultArgv[] = { appName, nullptr };
    static char** qtArgv = (argv && argc > 0) ? argv : defaultArgv;
    static QApplication* app = new QApplication(qtArgc, qtArgv);
    return app;
}
