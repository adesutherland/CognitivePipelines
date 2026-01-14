//
// Shared QApplication helper for tests
// Ensures a single QApplication instance is reused across all unit tests to
// avoid multiple application creations (which can crash on Windows).
//

#pragma once

#include <QApplication>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QString>

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

/**
 * @brief Parses an HTTP status code from a string (e.g., "HTTP 503" or "HTTP/1.1 503").
 */
inline int parseHttpCodeFromText(const QString& text) {
    static const QRegularExpression re1(QStringLiteral("\\bHTTP\\s+(\\d{3})\\b"));
    static const QRegularExpression re2(QStringLiteral("\\bHTTP\\/\\d+(?:\\.\\d+)?\\s+(\\d{3})\\b"));
    
    QRegularExpressionMatch m = re1.match(text);
    if (m.hasMatch()) return m.captured(1).toInt();
    
    m = re2.match(text);
    if (m.hasMatch()) return m.captured(1).toInt();
    
    return 0;
}

/**
 * @brief Checks if an HTTP status code or error message represents a temporary condition.
 * Temporary conditions include: 429 (Too Many Requests), 502 (Bad Gateway), 503 (Service Unavailable), 504 (Gateway Timeout).
 */
inline bool isTemporaryError(int httpCode) {
    return (httpCode == 429 || httpCode == 502 || httpCode == 503 || httpCode == 504);
}

inline bool isTemporaryError(const QString& errorText) {
    int code = parseHttpCodeFromText(errorText);
    if (isTemporaryError(code)) return true;
    
    // Also check for common phrases in case the code is not explicitly "HTTP xxx"
    return errorText.contains(QStringLiteral("overloaded"), Qt::CaseInsensitive) ||
           errorText.contains(QStringLiteral("try again later"), Qt::CaseInsensitive) ||
           errorText.contains(QStringLiteral("rate limit"), Qt::CaseInsensitive);
}
