//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//
#include "Logger.h"
#include "mainwindow.h"

bool AppLogHelper::s_globalDebugEnabled = false;

AppLogHelper::AppLogHelper(bool isWarn)
    : m_isWarn(isWarn)
{
}

void AppLogHelper::setGlobalDebugEnabled(bool enabled)
{
    s_globalDebugEnabled = enabled;
}

bool AppLogHelper::isGlobalDebugEnabled()
{
    return s_globalDebugEnabled;
}

AppLogHelper::~AppLogHelper()
{
    if (MainWindow::instanceExists()) {
        if (m_isWarn) {
            MainWindow::logMessage(QStringLiteral("Warning: ") + m_buffer);
        } else {
            MainWindow::logMessage(m_buffer);
        }
    } else {
        // Fallback for tests or headless mode
        // Only print debug logs if global debug is enabled.
        // Warnings always go to console when headless.
        if (m_isWarn) {
            qWarning().noquote() << m_buffer;
        } else if (s_globalDebugEnabled) {
            qDebug().noquote() << m_buffer;
        }
    }
}
