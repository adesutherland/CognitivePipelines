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
#include "Logger.h"
#include "quickjs-libc.h"
#include "ScriptDatabaseBridge.h"
#include <QJsonObject>
#include <QJsonArray>

QuickJSRuntime::QuickJSRuntime() {
    m_rt = JS_NewRuntime();
    js_std_init_handlers(m_rt);
    m_ctx = JS_NewContext(m_rt);

    // Loader for ES6 modules
    JS_SetModuleLoaderFunc(m_rt, NULL, js_module_loader, NULL);

    // Standard modules
    js_init_module_std(m_ctx, "std");
    js_init_module_os(m_ctx, "os");

    // Standard helpers (console, print, etc.)
    js_std_add_helpers(m_ctx, 0, nullptr);

    // Initialize database bridge
    // Use environment variable CP_QUICKJS_DB_PATH to override default "scripts.db"
    // This is useful for testing (e.g. setting it to ":memory:" or a temp file)
    QString dbPath = QString::fromUtf8(qgetenv("CP_QUICKJS_DB_PATH"));
    if (dbPath.isEmpty()) {
        dbPath = QStringLiteral("scripts.db");
    }
    m_dbBridge = std::make_unique<ScriptDatabaseBridge>(dbPath);
}

QuickJSRuntime::~QuickJSRuntime() {
    js_std_free_handlers(m_rt);
    JS_FreeContext(m_ctx);
    JS_FreeRuntime(m_rt);
}

bool QuickJSRuntime::execute(const QString& script, IScriptHost* host) {
    if (!host) return false;

    // Set host as current and this as opaque data in the context
    m_currentHost = host;
    JS_SetContextOpaque(m_ctx, this);

    // Setup global environment (console, pipeline, sqlite)
    setupGlobalEnv(host);

    std::string scriptStd = script.toStdString();
    
    // Heuristic: If the script contains "import" or "export", treat it as a module.
    // Otherwise, wrap it in an IIFE (Immediately Invoked Function Expression) to support top-level "return".
    bool isModule = script.contains(QStringLiteral("import ")) || script.contains(QStringLiteral("export "));
    
    JSValue val;
    if (isModule) {
        val = JS_Eval(m_ctx, scriptStd.c_str(), scriptStd.length(), "<input>", JS_EVAL_TYPE_MODULE);
    } else {
        std::string wrapped = "(function(){\n" + scriptStd + "\n})()";
        val = JS_Eval(m_ctx, wrapped.c_str(), wrapped.length(), "<input>", JS_EVAL_TYPE_GLOBAL);
    }

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
    } else {
        // If evaluation returned a value, set it as "output" in the host
        if (!JS_IsUndefined(val) && !JS_IsNull(val)) {
            QVariant result = jsToVariant(m_ctx, val);
            if (result.isValid()) {
                host->setOutput(QStringLiteral("output"), result);
            }
        }
    }

    JS_FreeValue(m_ctx, val);
    m_currentHost = nullptr;
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
    JS_SetPropertyStr(m_ctx, pipeline, "getTempDir", JS_NewCFunction(m_ctx, js_pipeline_get_temp_dir, "getTempDir", 0));
    JS_SetPropertyStr(m_ctx, global_obj, "pipeline", pipeline);

    // sqlite object
    JSValue sqlite = JS_NewObject(m_ctx);
    JS_SetPropertyStr(m_ctx, sqlite, "exec", JS_NewCFunction(m_ctx, js_sqlite_exec, "exec", 1));
    JS_SetPropertyStr(m_ctx, global_obj, "sqlite", sqlite);

    JS_FreeValue(m_ctx, global_obj);
}

JSValue QuickJSRuntime::js_console_log(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    QuickJSRuntime* self = static_cast<QuickJSRuntime*>(JS_GetContextOpaque(ctx));
    IScriptHost* host = self ? self->m_currentHost : nullptr;
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
    QuickJSRuntime* self = static_cast<QuickJSRuntime*>(JS_GetContextOpaque(ctx));
    IScriptHost* host = self ? self->m_currentHost : nullptr;
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
    QuickJSRuntime* self = static_cast<QuickJSRuntime*>(JS_GetContextOpaque(ctx));
    IScriptHost* host = self ? self->m_currentHost : nullptr;
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

JSValue QuickJSRuntime::js_pipeline_get_temp_dir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    Q_UNUSED(this_val);
    Q_UNUSED(argc);
    Q_UNUSED(argv);
    QuickJSRuntime* self = static_cast<QuickJSRuntime*>(JS_GetContextOpaque(ctx));
    IScriptHost* host = self ? self->m_currentHost : nullptr;
    if (host) {
        QString tempDir = host->getTempDir();
        return JS_NewString(ctx, tempDir.toUtf8().constData());
    }
    return JS_UNDEFINED;
}

JSValue QuickJSRuntime::js_sqlite_exec(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    QuickJSRuntime* self = static_cast<QuickJSRuntime*>(JS_GetContextOpaque(ctx));
    if (self && self->m_dbBridge && argc > 0) {
        const char* sql = JS_ToCString(ctx, argv[0]);
        if (sql) {
            QJsonValue res = self->m_dbBridge->exec(QString::fromUtf8(sql));
            JS_FreeCString(ctx, sql);
            return qJsonToJSValue(ctx, res);
        }
    }
    return JS_UNDEFINED;
}

QVariant QuickJSRuntime::jsToVariant(JSContext* ctx, JSValueConst val) {
    if (JS_IsNull(val) || JS_IsUndefined(val)) {
        return QVariant();
    } else if (JS_IsBool(val)) {
        return (bool)JS_ToBool(ctx, val);
    } else if (JS_IsNumber(val)) {
        double d;
        JS_ToFloat64(ctx, &d, val);
        return d;
    } else if (JS_IsString(val)) {
        const char* str = JS_ToCString(ctx, val);
        QString qstr = QString::fromUtf8(str);
        JS_FreeCString(ctx, str);
        return qstr;
    } else if (JS_IsArray(val)) {
        QVariantList list;
        JSValue lenVal = JS_GetPropertyStr(ctx, val, "length");
        uint32_t len = 0;
        JS_ToUint32(ctx, &len, lenVal);
        JS_FreeValue(ctx, lenVal);
        for (uint32_t i = 0; i < len; i++) {
            JSValue element = JS_GetPropertyUint32(ctx, val, i);
            list.append(jsToVariant(ctx, element));
            JS_FreeValue(ctx, element);
        }
        return list;
    } else if (JS_IsObject(val)) {
        QVariantMap map;
        JSPropertyEnum *ptab = nullptr;
        uint32_t plen = 0;
        if (JS_GetOwnPropertyNames(ctx, &ptab, &plen, val, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) >= 0) {
            for (uint32_t i = 0; i < plen; i++) {
                JSValue propVal = JS_GetProperty(ctx, val, ptab[i].atom);
                const char *key = JS_AtomToCString(ctx, ptab[i].atom);
                if (key) {
                    map.insert(QString::fromUtf8(key), jsToVariant(ctx, propVal));
                    JS_FreeCString(ctx, key);
                }
                JS_FreeValue(ctx, propVal);
            }
            JS_FreePropertyEnum(ctx, ptab, plen);
        }
        return map;
    }
    return QVariant();
}

JSValue QuickJSRuntime::variantToJs(JSContext* ctx, const QVariant& var) {
    if (var.typeId() == QMetaType::QString) {
        return JS_NewString(ctx, var.toString().toUtf8().constData());
    } else if (var.typeId() == QMetaType::Double || var.typeId() == QMetaType::Int) {
        return JS_NewFloat64(ctx, var.toDouble());
    } else if (var.typeId() == QMetaType::Bool) {
        return JS_NewBool(ctx, var.toBool());
    }
    return JS_UNDEFINED;
}

JSValue QuickJSRuntime::qJsonToJSValue(JSContext* ctx, const QJsonValue& value) {
    if (value.isObject()) {
        QJsonObject obj = value.toObject();
        JSValue jsObj = JS_NewObject(ctx);
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            JS_SetPropertyStr(ctx, jsObj, it.key().toUtf8().constData(), qJsonToJSValue(ctx, it.value()));
        }
        return jsObj;
    } else if (value.isArray()) {
        QJsonArray arr = value.toArray();
        JSValue jsArr = JS_NewArray(ctx);
        for (int i = 0; i < arr.size(); ++i) {
            JS_SetPropertyUint32(ctx, jsArr, i, qJsonToJSValue(ctx, arr[i]));
        }
        return jsArr;
    } else if (value.isString()) {
        return JS_NewString(ctx, value.toString().toUtf8().constData());
    } else if (value.isDouble()) {
        return JS_NewFloat64(ctx, value.toDouble());
    } else if (value.isBool()) {
        return JS_NewBool(ctx, value.toBool());
    } else if (value.isNull()) {
        return JS_NULL;
    }
    return JS_UNDEFINED;
}
