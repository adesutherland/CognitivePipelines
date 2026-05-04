//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//

#pragma once

#include "IScriptHost.h"

/**
 * @brief CREXX implementation of the Universal Script runtime.
 *
 * Scripts can be authored as a produce() body:
 *
 *   output[1] = input[1]
 *   log[1] = "done"
 *
 * The runtime wraps that body in a small CREXX module, fills input[] through the
 * PIPELINE ADDRESS environment, and reads output[]/log[]/errors[] when the
 * script returns.
 */
class CrexxRuntime : public IScriptEngine {
public:
    bool execute(const QString& script, IScriptHost* host) override;
    QString getEngineId() const override;
};

