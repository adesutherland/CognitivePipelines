#include <QtGlobal>
#include <QDebug>
#include <cstdio>

// Global Qt message handler to ensure all Qt logs are written to stderr in unit tests
static void unitQtMessageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    const char* level = "INFO";
    switch (type) {
        case QtDebugMsg: level = "DEBUG"; break;
        case QtInfoMsg: level = "INFO"; break;
        case QtWarningMsg: level = "WARN"; break;
        case QtCriticalMsg: level = "CRIT"; break;
        case QtFatalMsg: level = "FATAL"; break;
    }
    QByteArray loc = msg.toLocal8Bit();
    const char* file = ctx.file ? ctx.file : "?";
    const char* func = ctx.function ? ctx.function : "?";
    fprintf(stderr, "[QT][%s] %s (%s:%d, %s)\n", level, loc.constData(), file, ctx.line, func);
    fflush(stderr);
    if (type == QtFatalMsg) {
        abort();
    }
}

struct UnitTestLogInitializer {
    UnitTestLogInitializer() {
        qInstallMessageHandler(unitQtMessageHandler);
        qputenv("QT_LOGGING_TO_CONSOLE", QByteArray("1"));
    }
};

// Instantiate a single global initializer so the handler is installed before tests run
static UnitTestLogInitializer s_unitTestLogInitializer;
