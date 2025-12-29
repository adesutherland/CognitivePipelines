//
// Unit tests for string utilities
//

#include <gtest/gtest.h>
#include <QString>
#include "string_utils.h"

using cp::strings::canonicalize_model_id;

TEST(StringUtils, Canonicalize_NoChangeForPlainId)
{
    const QString in = QStringLiteral("gpt-5.1");
    EXPECT_EQ(canonicalize_model_id(in), QStringLiteral("gpt-5.1"));
}

TEST(StringUtils, Canonicalize_TrimsWhitespace)
{
    const QString in = QStringLiteral("  gpt-5.1   ");
    EXPECT_EQ(canonicalize_model_id(in), QStringLiteral("gpt-5.1"));
}

TEST(StringUtils, Canonicalize_StripsAsciiDoubleQuotes)
{
    const QString in = QStringLiteral("\"gpt-5.1\"");
    EXPECT_EQ(canonicalize_model_id(in), QStringLiteral("gpt-5.1"));
}

TEST(StringUtils, Canonicalize_StripsSmartDoubleQuotes)
{
    const QString in = QString::fromUtf8("\xE2\x80\x9Cgpt-5-mini\xE2\x80\x9D"); // “gpt-5-mini”
    EXPECT_EQ(canonicalize_model_id(in), QStringLiteral("gpt-5-mini"));
}

TEST(StringUtils, Canonicalize_StripsSmartSingleQuotes)
{
    const QString in = QString::fromUtf8("\xE2\x80\x98gpt-5-pro\xE2\x80\x99"); // ‘gpt-5-pro’
    EXPECT_EQ(canonicalize_model_id(in), QStringLiteral("gpt-5-pro"));
}

TEST(StringUtils, Canonicalize_EmptyAfterQuotes)
{
    const QString in = QStringLiteral("\"\"");
    EXPECT_EQ(canonicalize_model_id(in), QString());
}
