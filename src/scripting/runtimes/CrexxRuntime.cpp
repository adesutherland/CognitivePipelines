//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//

#include "CrexxRuntime.h"

#include "CommonDataTypes.h"

extern "C" {
#include <crexxsaa.h>
}

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>

#include <memory>

namespace {

struct PipelineInvocation {
    IScriptHost* host = nullptr;
    QStringList inputItems;
    QStringList outputs;
    QStringList logs;
    QStringList errors;
};

struct CrexxThreadState {
    crexxsaa_context* context = nullptr;
    PipelineInvocation* invocation = nullptr;
    QString lastError;

    ~CrexxThreadState()
    {
        if (context) {
            crexxsaa_destroy(context);
            context = nullptr;
        }
    }
};

thread_local std::unique_ptr<CrexxThreadState> t_crexxState;

QString fromUtf8(const char* value)
{
    return value ? QString::fromUtf8(value) : QString();
}

QByteArray toUtf8Bytes(const QString& value)
{
    return value.toUtf8();
}

bool fileExists(const QString& path)
{
    return !path.trimmed().isEmpty() && QFileInfo::exists(path);
}

QString appDirPath()
{
    if (QCoreApplication::instance()) {
        return QCoreApplication::applicationDirPath();
    }
    return QDir::currentPath();
}

QStringList runtimeDirCandidates()
{
    QStringList dirs;

    const QByteArray envRuntimeDir = qgetenv("CP_CREXX_RUNTIME_DIR");
    if (!envRuntimeDir.isEmpty()) {
        dirs << QString::fromLocal8Bit(envRuntimeDir);
    }

    const QString appDir = appDirPath();
    dirs << appDir + QStringLiteral("/crexx");

#ifdef Q_OS_MAC
    dirs << QDir(appDir).absoluteFilePath(QStringLiteral("../Resources/crexx"));
#endif

    dirs << QStringLiteral(CP_CREXX_RUNTIME_DIR);

    QStringList cleaned;
    for (const QString& dir : dirs) {
        const QString clean = QDir::cleanPath(dir);
        if (!clean.isEmpty() && !cleaned.contains(clean)) {
            cleaned << clean;
        }
    }
    return cleaned;
}

QString findRuntimeFile(const QString& envName, const QString& configuredPath, const QString& fileName)
{
    const QByteArray envValue = qgetenv(envName.toLocal8Bit().constData());
    if (!envValue.isEmpty()) {
        const QString envPath = QString::fromLocal8Bit(envValue);
        if (fileExists(envPath)) {
            return QFileInfo(envPath).absoluteFilePath();
        }
    }

    for (const QString& dir : runtimeDirCandidates()) {
        const QString candidate = QDir(dir).absoluteFilePath(fileName);
        if (fileExists(candidate)) {
            return QFileInfo(candidate).absoluteFilePath();
        }
    }

    if (fileExists(configuredPath)) {
        return QFileInfo(configuredPath).absoluteFilePath();
    }

    return QString();
}

QString cacheDirPath()
{
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheDir.isEmpty()) {
        cacheDir = QDir::tempPath();
    }
    cacheDir = QDir(cacheDir).absoluteFilePath(QStringLiteral("crexx"));
    QDir().mkpath(cacheDir);
    return cacheDir;
}

QString scriptDirPath(IScriptHost* host)
{
    QString baseDir = host ? host->getTempDir() : QString();
    if (baseDir.trimmed().isEmpty()) {
        baseDir = QDir::tempPath();
    }

    const QString dir = QDir(baseDir).absoluteFilePath(QStringLiteral("crexx"));
    QDir().mkpath(dir);
    return dir;
}

QString variantToProtocolString(const QVariant& value)
{
    if (!value.isValid() || value.isNull()) {
        return QString();
    }

    const int type = value.typeId();
    if (type == QMetaType::QVariantMap || type == QMetaType::QVariantList || type == QMetaType::QStringList) {
        return QString::fromUtf8(QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact));
    }

    return value.toString();
}

QStringList appendVariantItems(const QVariant& value, QStringList items)
{
    const int type = value.typeId();
    if (type == QMetaType::QStringList) {
        items.append(value.toStringList());
        return items;
    }

    if (type == QMetaType::QVariantList) {
        const QVariantList list = value.toList();
        for (const QVariant& item : list) {
            items << variantToProtocolString(item);
        }
        return items;
    }

    if (value.isValid() && !value.isNull()) {
        items << variantToProtocolString(value);
    }
    return items;
}

bool isSystemKey(const QString& key)
{
    return key.startsWith(QLatin1Char('_'));
}

QVariantMap userFieldsOnly(const QVariantMap& token)
{
    QVariantMap visible;
    for (auto it = token.cbegin(); it != token.cend(); ++it) {
        if (!isSystemKey(it.key())) {
            visible.insert(it.key(), it.value());
        }
    }
    return visible;
}

QStringList inputItemsFromHost(IScriptHost* host)
{
    QStringList items;
    if (!host) {
        return items;
    }

    const QVariant tokens = host->getInput(QStringLiteral("_tokens"));
    if (tokens.typeId() == QMetaType::QVariantList) {
        const QVariantList tokenList = tokens.toList();
        for (const QVariant& tokenValue : tokenList) {
            const QVariantMap token = tokenValue.toMap();
            if (token.contains(QStringLiteral("input"))) {
                items = appendVariantItems(token.value(QStringLiteral("input")), items);
            } else {
                const QVariantMap visible = userFieldsOnly(token);
                if (!visible.isEmpty()) {
                    items << variantToProtocolString(visible);
                }
            }
        }
    }

    if (!items.isEmpty()) {
        return items;
    }

    return appendVariantItems(host->getInput(QStringLiteral("input")), items);
}

bool setAddressVariable(crexxsaa_context* context, const QString& name, const QString& value, QString* error)
{
    const QByteArray nameBytes = toUtf8Bytes(name);
    const QByteArray valueBytes = toUtf8Bytes(value);
    const int rc = crexxsaa_address_variable_set(
        context,
        nameBytes.constData(),
        valueBytes.constData(),
        static_cast<size_t>(valueBytes.size()));
    if (rc == CREXXSAA_VARIABLE_OK) {
        return true;
    }

    if (error) {
        *error = QStringLiteral("CREXX variable set failed for %1: %2")
                     .arg(name, fromUtf8(crexxsaa_last_error(context)));
    }
    return false;
}

bool setStem(crexxsaa_context* context, const QString& stemName, const QStringList& values, QString* error)
{
    if (!setAddressVariable(context, stemName + QStringLiteral(".0"), QString::number(values.size()), error)) {
        return false;
    }

    for (int i = 0; i < values.size(); ++i) {
        if (!setAddressVariable(context,
                                QStringLiteral("%1.%2").arg(stemName).arg(i + 1),
                                values.at(i),
                                error)) {
            return false;
        }
    }
    return true;
}

bool getAddressVariable(crexxsaa_context* context, const QString& name, QString* value, QString* error)
{
    char* raw = nullptr;
    size_t len = 0;
    const QByteArray nameBytes = toUtf8Bytes(name);
    const int rc = crexxsaa_address_variable_get_alloc(context, nameBytes.constData(), &raw, &len);

    if (rc == CREXXSAA_VARIABLE_NOT_FOUND) {
        return false;
    }

    if (rc != CREXXSAA_VARIABLE_OK) {
        if (error) {
            *error = QStringLiteral("CREXX variable get failed for %1: %2")
                         .arg(name, fromUtf8(crexxsaa_last_error(context)));
        }
        return false;
    }

    if (value) {
        *value = QString::fromUtf8(raw, static_cast<qsizetype>(len));
    }
    crexxsaa_free(raw);
    return true;
}

QStringList getStem(crexxsaa_context* context, const QString& stemName, QString* error)
{
    QString countText;
    if (!getAddressVariable(context, stemName + QStringLiteral(".0"), &countText, error)) {
        return {};
    }

    bool ok = false;
    const int count = countText.toInt(&ok);
    if (!ok || count < 1) {
        return {};
    }

    QStringList values;
    for (int i = 1; i <= count; ++i) {
        QString item;
        if (getAddressVariable(context, QStringLiteral("%1.%2").arg(stemName).arg(i), &item, error)) {
            values << item;
        } else if (error && !error->isEmpty()) {
            return {};
        } else {
            values << QString();
        }
    }
    return values;
}

QString resolveCommandPayload(const crexxsaa_address_request* request, const QString& command)
{
    const int firstSpace = command.indexOf(QLatin1Char(' '));
    QString payload = firstSpace < 0 ? QString() : command.mid(firstSpace + 1).trimmed();

    if (payload.startsWith(QStringLiteral("${")) && payload.endsWith(QLatin1Char('}'))) {
        payload = payload.mid(2, payload.size() - 3).trimmed();
    } else if (payload.startsWith(QLatin1Char(':'))) {
        payload = payload.mid(1).trimmed();
    } else {
        return payload;
    }

    QString value;
    QString ignoredError;
    if (getAddressVariable(request->context, payload, &value, &ignoredError)) {
        return value;
    }
    return QString();
}

int pipelineAddressCallback(const crexxsaa_address_request* request,
                            crexxsaa_address_response* response,
                            void* userdata)
{
    auto* state = static_cast<CrexxThreadState*>(userdata);
    if (!request || !request->context || !response || !state || !state->invocation) {
        return -1;
    }

    response->rc = 0;
    response->condition_name = nullptr;
    response->diagnostic = nullptr;

    const QString command = fromUtf8(request->command).trimmed();
    const QString op = command.section(QLatin1Char(' '), 0, 0).toUpper();

    QString error;
    if (op == QStringLiteral("INPUT")) {
        if (!setStem(request->context, QStringLiteral("cp_input"), state->invocation->inputItems, &error)) {
            state->lastError = error;
            response->rc = 20;
        }
        return 0;
    }

    if (op == QStringLiteral("RETURN")) {
        state->invocation->outputs = getStem(request->context, QStringLiteral("cp_output"), &error);
        if (!error.isEmpty()) {
            state->lastError = error;
            response->rc = 30;
            return 0;
        }

        state->invocation->logs = getStem(request->context, QStringLiteral("cp_log"), &error);
        if (!error.isEmpty()) {
            state->lastError = error;
            response->rc = 31;
            return 0;
        }

        state->invocation->errors = getStem(request->context, QStringLiteral("cp_errors"), &error);
        if (!error.isEmpty()) {
            state->lastError = error;
            response->rc = 32;
        }
        return 0;
    }

    if (op == QStringLiteral("LOG")) {
        state->invocation->logs << resolveCommandPayload(request, command);
        return 0;
    }

    if (op == QStringLiteral("ERROR")) {
        state->invocation->errors << resolveCommandPayload(request, command);
        return 0;
    }

    state->lastError = QStringLiteral("Unknown PIPELINE ADDRESS command: %1").arg(command);
    response->rc = 99;
    return 0;
}

bool initializeThreadState(CrexxThreadState* state)
{
    if (!state) {
        return false;
    }
    if (state->context) {
        return true;
    }

    const QString rxcPath = findRuntimeFile(QStringLiteral("CP_CREXX_RXC"), QStringLiteral(CP_CREXX_RXC_PATH), QStringLiteral("rxc"));
    const QString rxasPath = findRuntimeFile(QStringLiteral("CP_CREXX_RXAS"), QStringLiteral(CP_CREXX_RXAS_PATH), QStringLiteral("rxas"));
    const QString libraryPath = findRuntimeFile(QStringLiteral("CP_CREXX_LIBRARY_RXBIN"),
                                                QStringLiteral(CP_CREXX_LIBRARY_RXBIN),
                                                QStringLiteral("library.rxbin"));

    if (!fileExists(rxcPath) || !fileExists(rxasPath) || !fileExists(libraryPath)) {
        state->lastError = QStringLiteral("CREXX runtime is incomplete. rxc=%1, rxas=%2, library.rxbin=%3")
                               .arg(rxcPath, rxasPath, libraryPath);
        return false;
    }

    const QString runtimeDir = QFileInfo(libraryPath).absolutePath();
    crexxsaa_context* context = nullptr;
    const QByteArray runtimeDirBytes = toUtf8Bytes(runtimeDir);
    const QByteArray libraryBytes = toUtf8Bytes(libraryPath);
    if (crexxsaa_create(runtimeDirBytes.constData(), libraryBytes.constData(), &context) != 0 || !context) {
        state->lastError = QStringLiteral("Failed to create CREXX context");
        return false;
    }

    const QByteArray rxcBytes = toUtf8Bytes(rxcPath);
    const QByteArray rxasBytes = toUtf8Bytes(rxasPath);
    if (crexxsaa_set_compiler(context, rxcBytes.constData(), rxasBytes.constData(), runtimeDirBytes.constData()) != 0) {
        state->lastError = QStringLiteral("Failed to configure CREXX compiler: %1").arg(fromUtf8(crexxsaa_last_error(context)));
        crexxsaa_destroy(context);
        return false;
    }

    const QByteArray cacheBytes = toUtf8Bytes(cacheDirPath());
    if (crexxsaa_set_cache_dir(context, cacheBytes.constData()) != 0) {
        state->lastError = QStringLiteral("Failed to configure CREXX cache: %1").arg(fromUtf8(crexxsaa_last_error(context)));
        crexxsaa_destroy(context);
        return false;
    }

    if (crexxsaa_register_address_environment(context, "PIPELINE", pipelineAddressCallback, state) != 0) {
        state->lastError = QStringLiteral("Failed to register PIPELINE ADDRESS environment: %1")
                               .arg(fromUtf8(crexxsaa_last_error(context)));
        crexxsaa_destroy(context);
        return false;
    }

    if (crexxsaa_set_address_environment(context, "PIPELINE") != 0) {
        state->lastError = QStringLiteral("Failed to set PIPELINE ADDRESS environment: %1")
                               .arg(fromUtf8(crexxsaa_last_error(context)));
        crexxsaa_destroy(context);
        return false;
    }

    state->context = context;
    return true;
}

bool hasProcedure(const QString& script, const QString& procedureName)
{
    const QRegularExpression pattern(QStringLiteral("(?im)^\\s*%1\\s*:").arg(QRegularExpression::escape(procedureName)));
    return pattern.match(script).hasMatch();
}

QString indentedBody(const QString& script)
{
    QStringList lines = script.split(QLatin1Char('\n'));
    for (QString& line : lines) {
        if (!line.trimmed().isEmpty()) {
            line.prepend(QStringLiteral("  "));
        }
    }
    return lines.join(QLatin1Char('\n'));
}

QString standardPreamble()
{
    return QStringLiteral(
        "options levelb\n"
        "import rxfnsb\n");
}

QString generatedMain()
{
    return QStringLiteral(
        "main: procedure = .int\n"
        "  cp_input = .string[]\n"
        "  cp_output = .string[]\n"
        "  cp_log = .string[]\n"
        "  cp_errors = .string[]\n"
        "  rc = 0\n"
        "  produce_rc = 0\n"
        "  address pipeline \"INPUT\" expose cp_input[]\n"
        "  if rc <> 0 then return rc\n"
        "  produce_rc = produce(cp_input, cp_output, cp_log, cp_errors)\n"
        "  address pipeline \"RETURN\" expose cp_output[] cp_log[] cp_errors[]\n"
        "  if rc <> 0 then return rc\n"
        "  if produce_rc <> 0 then return produce_rc\n"
        "  if cp_errors.0 > 0 then return 1\n"
        "  return 0\n");
}

struct CrexxScriptParts {
    QString preamble;
    QString body;
};

CrexxScriptParts splitPreamble(const QString& script)
{
    const QStringList lines = script.split(QLatin1Char('\n'));
    QStringList preambleLines;
    QStringList bodyLines;
    bool inPreamble = true;

    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        const bool isPreambleLine = trimmed.isEmpty()
            || trimmed.startsWith(QStringLiteral("options "), Qt::CaseInsensitive)
            || trimmed.startsWith(QStringLiteral("import "), Qt::CaseInsensitive);

        if (inPreamble && isPreambleLine) {
            preambleLines << line;
            continue;
        }

        inPreamble = false;
        bodyLines << line;
    }

    return {preambleLines.join(QLatin1Char('\n')).trimmed(), bodyLines.join(QLatin1Char('\n')).trimmed()};
}

QString wrapScript(const QString& script)
{
    if (hasProcedure(script, QStringLiteral("main"))) {
        return script;
    }

    if (hasProcedure(script, QStringLiteral("produce"))) {
        const CrexxScriptParts parts = splitPreamble(script);
        return (parts.preamble.isEmpty() ? standardPreamble() : parts.preamble)
            + QStringLiteral("\n\n")
            + generatedMain()
            + QStringLiteral("\n")
            + parts.body
            + QStringLiteral("\n");
    }

    return standardPreamble()
        + QStringLiteral("\n")
        + generatedMain()
        + QStringLiteral("\n")
        + QStringLiteral("produce: procedure = .int\n")
        + QStringLiteral("  arg input = .string[], expose output = .string[], expose log = .string[], expose errors = .string[]\n")
        + indentedBody(script)
        + QStringLiteral("\n")
        + QStringLiteral("  return 0\n");
}

bool writeScriptSource(const QString& wrappedScript, IScriptHost* host, QString* sourcePath, QString* error)
{
    const QByteArray scriptBytes = wrappedScript.toUtf8();
    const QByteArray hash = QCryptographicHash::hash(scriptBytes, QCryptographicHash::Sha256).toHex();
    const QString path = QDir(scriptDirPath(host)).absoluteFilePath(
        QStringLiteral("universal-script-%1.rexx").arg(QString::fromLatin1(hash.left(16))));

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("Unable to write CREXX source %1: %2").arg(path, file.errorString());
        }
        return false;
    }

    if (file.write(scriptBytes) != scriptBytes.size()) {
        if (error) {
            *error = QStringLiteral("Unable to write full CREXX source %1").arg(path);
        }
        return false;
    }

    if (sourcePath) {
        *sourcePath = path;
    }
    return true;
}

QVariant outputValueFromList(const QStringList& outputs)
{
    if (outputs.size() == 1) {
        return outputs.first();
    }

    QVariantList list;
    for (const QString& output : outputs) {
        list << output;
    }
    return list;
}

} // namespace

QString CrexxRuntime::getEngineId() const
{
    return QStringLiteral("crexx");
}

bool CrexxRuntime::execute(const QString& script, IScriptHost* host)
{
    if (!host) {
        return false;
    }

    if (!t_crexxState) {
        t_crexxState = std::make_unique<CrexxThreadState>();
    }

    CrexxThreadState* state = t_crexxState.get();
    state->lastError.clear();
    if (!initializeThreadState(state)) {
        host->setError(state->lastError);
        return false;
    }

    const QString wrappedScript = wrapScript(script);
    QString sourcePath;
    QString error;
    if (!writeScriptSource(wrappedScript, host, &sourcePath, &error)) {
        host->setError(error);
        return false;
    }

    PipelineInvocation invocation;
    invocation.host = host;
    invocation.inputItems = inputItemsFromHost(host);

    state->invocation = &invocation;

    int programRc = -1;
    const QByteArray sourceBytes = toUtf8Bytes(sourcePath);
    const int runRc = crexxsaa_run_source(
        state->context,
        sourceBytes.constData(),
        "cognitive-pipelines-universal-script",
        0,
        0,
        nullptr,
        &programRc);

    state->invocation = nullptr;

    for (const QString& log : invocation.logs) {
        if (!log.isEmpty()) {
            host->log(log);
        }
    }

    if (!invocation.outputs.isEmpty()) {
        host->setOutput(QStringLiteral("output"), outputValueFromList(invocation.outputs));
    }

    if (!invocation.errors.isEmpty()) {
        host->setError(invocation.errors.join(QStringLiteral("\n")));
        return false;
    }

    if (runRc != 0) {
        const QString message = !state->lastError.isEmpty()
            ? state->lastError
            : QStringLiteral("CREXX execution failed: %1").arg(fromUtf8(crexxsaa_last_error(state->context)));
        host->setError(message);
        return false;
    }

    if (programRc != 0) {
        host->setError(QStringLiteral("CREXX script returned %1").arg(programRc));
        return false;
    }

    return true;
}
