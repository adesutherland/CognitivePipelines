//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#pragma once

#include "IScriptHost.h"
#include "quickjs.h"
#include <QString>
#include <QJsonValue>
#include <memory>

class ScriptDatabaseBridge;

/**
 * @brief Implementation of IScriptEngine using the QuickJS engine.
 */
class QuickJSRuntime : public IScriptEngine {
public:
    QuickJSRuntime();
    ~QuickJSRuntime() override;

    bool execute(const QString& script, IScriptHost* host) override;
    QString getEngineId() const override { return QStringLiteral("quickjs"); }

private:
    void setupGlobalEnv(IScriptHost* host);

    // Static C callbacks for QuickJS
    static JSValue js_console_log(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_pipeline_get_input(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_pipeline_set_output(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_sqlite_exec(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

    // Conversion helpers
    static QVariant jsToVariant(JSContext* ctx, JSValueConst val);
    static JSValue variantToJs(JSContext* ctx, const QVariant& var);
    static JSValue qJsonToJSValue(JSContext* ctx, const QJsonValue& value);

    JSRuntime* m_rt;
    JSContext* m_ctx;
    IScriptHost* m_currentHost = nullptr;
    std::unique_ptr<ScriptDatabaseBridge> m_dbBridge;
};
