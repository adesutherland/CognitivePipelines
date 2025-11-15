//
// Cognitive Pipeline Application
//
// ExecutionStateModel: stores execution states for nodes and connections by QUuid
//
// Copyright (c) 2025 Adrian Sutherland
//
// MIT License
//
#pragma once

#include <QObject>
#include <QMap>
#include <QSet>
#include <QUuid>
#include <QMutex>
#include <QMutexLocker>

#include "ExecutionState.h"

class ExecutionStateModel : public QObject
{
    Q_OBJECT
public:
    explicit ExecutionStateModel(QObject* parent = nullptr)
        : QObject(parent) {}

    ExecutionState stateFor(const QUuid& id) const {
        QMutexLocker lock(&mutex_);
        return states_.value(id, ExecutionState::Idle);
    }

signals:
    void stateChanged();

public slots:
    void onNodeStatusChanged(const QUuid& nodeId, int state) {
        setState_(nodeId, static_cast<ExecutionState>(state));
    }
    void onConnectionStatusChanged(const QUuid& connId, int state) {
        setState_(connId, static_cast<ExecutionState>(state));
    }

private:
    void setState_(const QUuid& id, ExecutionState s) {
        bool changed = false;
        {
            QMutexLocker lock(&mutex_);
            auto it = states_.find(id);
            if (it == states_.end() || it.value() != s) {
                states_[id] = s;
                changed = true;
            }
        }
        if (changed) emit stateChanged();
    }

private:
    mutable QMutex mutex_;
    QMap<QUuid, ExecutionState> states_;
};
