//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//
#pragma once

#include <QString>
#include <QDebug>

class AppLogHelper {
public:
    AppLogHelper(bool isWarn);
    ~AppLogHelper();
    QDebug stream() { return QDebug(&m_buffer).nospace(); }

    static void setGlobalDebugEnabled(bool enabled);
    static bool isGlobalDebugEnabled();

private:
    QString m_buffer;
    bool m_isWarn;
    static bool s_globalDebugEnabled;
};

#define CP_LOG AppLogHelper(false).stream()
#define CP_WARN AppLogHelper(true).stream()
#define CP_CLOG(category) (AppLogHelper(false).stream() << "[" #category "] ")
