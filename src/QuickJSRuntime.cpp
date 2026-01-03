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

#include "QuickJSRuntime.h"
#include <QDebug>

QuickJSRuntime::QuickJSRuntime() {
    m_rt = JS_NewRuntime();
    m_ctx = JS_NewContext(m_rt);
}

QuickJSRuntime::~QuickJSRuntime() {
    JS_FreeContext(m_ctx);
    JS_FreeRuntime(m_rt);
}

bool QuickJSRuntime::execute(const QString& script, IScriptHost* host) {
    if (!host) return false;

    // Set host as opaque data in the context
    JS_SetContextOpaque(m_ctx, host);

    // Setup global environment (console, pipeline)
    setupGlobalEnv(host);

    std::string scriptStd = script.toStdString();
    JSValue val = JS_Eval(m_ctx, scriptStd.c_str(), scriptStd.length(), "<input>", JS_EVAL_TYPE_GLOBAL);

    bool success = true;
    if (JS_IsException(val)) {
        JSValue exception = JS_GetException(m_ctx);
        const char* msg = JS_ToCString(m_ctx, exception);
        JSValue stack = JS_GetPropertyStr(m_ctx, exception, "stack");
        const char* stackTrace = JS_ToCString(m_ctx, stack);

        QString errorMsg = QString::fromUtf8(msg);
        if (stackTrace) {
            errorMsg += "\nStack trace:\n" + QString::fromUtf8(stackTrace);
        }

        host->setError(errorMsg);
        
        JS_FreeCString(m_ctx, msg);
        JS_FreeCString(m_ctx, stackTrace);
        JS_FreeValue(m_ctx, stack);
        JS_FreeValue(m_ctx, exception);
        success = false;
    }

    JS_FreeValue(m_ctx, val);
    return success;
}

void QuickJSRuntime::setupGlobalEnv(IScriptHost* host) {
    JSValue global_obj = JS_GetGlobalObject(m_ctx);

    // console object
    JSValue console = JS_NewObject(m_ctx);
    JS_SetPropertyStr(m_ctx, console, "log", JS_NewCFunction(m_ctx, js_console_log, "log", 1));
    JS_SetPropertyStr(m_ctx, global_obj, "console", console);

    // pipeline object
    JSValue pipeline = JS_NewObject(m_ctx);
    JS_SetPropertyStr(m_ctx, pipeline, "getInput", JS_NewCFunction(m_ctx, js_pipeline_get_input, "getInput", 1));
    JS_SetPropertyStr(m_ctx, pipeline, "setOutput", JS_NewCFunction(m_ctx, js_pipeline_set_output, "setOutput", 2));
    JS_SetPropertyStr(m_ctx, global_obj, "pipeline", pipeline);

    JS_FreeValue(m_ctx, global_obj);
}

JSValue QuickJSRuntime::js_console_log(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    IScriptHost* host = static_cast<IScriptHost*>(JS_GetContextOpaque(ctx));
    if (host && argc > 0) {
        QStringList parts;
        for (int i = 0; i < argc; ++i) {
            const char* str = JS_ToCString(ctx, argv[i]);
            if (str) {
                parts << QString::fromUtf8(str);
                JS_FreeCString(ctx, str);
            }
        }
        host->log(parts.join(QStringLiteral(" ")));
    }
    return JS_UNDEFINED;
}

JSValue QuickJSRuntime::js_pipeline_get_input(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    IScriptHost* host = static_cast<IScriptHost*>(JS_GetContextOpaque(ctx));
    if (host && argc > 0) {
        const char* key = JS_ToCString(ctx, argv[0]);
        if (key) {
            QVariant val = host->getInput(QString::fromUtf8(key));
            JS_FreeCString(ctx, key);
            return variantToJs(ctx, val);
        }
    }
    return JS_UNDEFINED;
}

JSValue QuickJSRuntime::js_pipeline_set_output(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    IScriptHost* host = static_cast<IScriptHost*>(JS_GetContextOpaque(ctx));
    if (host && argc > 1) {
        const char* key = JS_ToCString(ctx, argv[0]);
        if (key) {
            QVariant val = jsToVariant(ctx, argv[1]);
            host->setOutput(QString::fromUtf8(key), val);
            JS_FreeCString(ctx, key);
        }
    }
    return JS_UNDEFINED;
}

QVariant QuickJSRuntime::jsToVariant(JSContext* ctx, JSValueConst val) {
    if (JS_IsString(val)) {
        const char* str = JS_ToCString(ctx, val);
        QString qstr = QString::fromUtf8(str);
        JS_FreeCString(ctx, str);
        return qstr;
    } else if (JS_IsNumber(val)) {
        double d;
        JS_ToFloat64(ctx, &d, val);
        return d;
    } else if (JS_IsBool(val)) {
        return (bool)JS_ToBool(ctx, val);
    }
    return QVariant();
}

JSValue QuickJSRuntime::variantToJs(JSContext* ctx, const QVariant& var) {
    if (var.type() == QVariant::String) {
        return JS_NewString(ctx, var.toString().toUtf8().constData());
    } else if (var.type() == QVariant::Double || var.type() == QVariant::Int) {
        return JS_NewFloat64(ctx, var.toDouble());
    } else if (var.type() == QVariant::Bool) {
        return JS_NewBool(ctx, var.toBool());
    }
    return JS_UNDEFINED;
}
