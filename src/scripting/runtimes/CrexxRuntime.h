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
 * Scripts can read and write named pins through the PIPELINE ADDRESS
 * environment:
 *
 *   value = ""
 *   address pipeline "GET input INTO :value"
 *   result = upper(value)
 *   address pipeline "SET output :result"
 *
 * The runtime wraps the script body in a small CREXX module. Scripts may also
 * provide their own produce: procedure, but the pin contract remains the
 * PIPELINE ADDRESS environment.
 */
class CrexxRuntime : public IScriptEngine {
public:
    bool execute(const QString& script, IScriptHost* host) override;
    QString getEngineId() const override;
};
