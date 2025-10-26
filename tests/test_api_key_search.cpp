//
// Cognitive Pipeline Application
//
// Focused unit test for LLMConnector::getApiKey upward accounts.json search.
//
#include <gtest/gtest.h>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QtGlobal>

#include "LLMConnector.h"

static bool writeTextFile(const QString& path, const QByteArray& data) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    const auto w = f.write(data);
    f.close();
    return w == data.size();
}

TEST(ApiKeySearchTest, FindsAccountsJsonUpToRootFromCwd) {
    // Ensure env var does not interfere
#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    qunsetenv("OPENAI_API_KEY");
#else
    qputenv("OPENAI_API_KEY", QByteArray());
#endif

    // Create a temporary root directory with nested subdirs: top/a/b/c
    QTemporaryDir tmpRoot;
    ASSERT_TRUE(tmpRoot.isValid());
    const QString top = QDir(tmpRoot.path()).absolutePath();

    QDir d(top);
    ASSERT_TRUE(d.mkpath("a/b/c"));

    const QString deepest = QDir(top + "/a/b/c").absolutePath();

    // Place accounts.json at the top directory
    const QString accountsPath = top + "/accounts.json";
    const QByteArray json = R"JSON({
        "accounts": [ { "name": "default_openai", "api_key": "TEST_KEY_123" } ]
    })JSON";
    ASSERT_TRUE(writeTextFile(accountsPath, json));

    // Change current working directory to the deepest nested path
    const QString oldCwd = QDir::currentPath();
    ASSERT_TRUE(QDir::setCurrent(deepest));

    // Now LLMConnector::getApiKey should walk up and find the top accounts.json
    const QString key = LLMConnector::getApiKey();

    // Restore CWD before assertions finalize
    QDir::setCurrent(oldCwd);

    ASSERT_EQ(key, QStringLiteral("TEST_KEY_123"));
}
