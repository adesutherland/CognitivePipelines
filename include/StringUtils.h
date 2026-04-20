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

#include <QString>

namespace cp::strings {

// Canonicalize a model id by trimming and stripping exactly one matching pair of outer quotes.
// Handles ASCII double quotes (") and common smart quotes (U+201C/U+201D and U+2018/U+2019).
inline QString canonicalize_model_id(QString s)
{
    if (s.isEmpty()) return s;
    QString t = s.trimmed();
    if (t.size() < 2) return t;

    auto strip_pair = [&t](QChar l, QChar r) -> bool {
        if (t.size() >= 2 && t.front() == l && t.back() == r) {
            t = t.mid(1, t.size() - 2);
            t = t.trimmed();
            return true;
        }
        return false;
    };

    // Try known quote pairs; strip at most one layer
    if (strip_pair(QChar('"'), QChar('"'))) return t;
    if (strip_pair(QChar(0x201C), QChar(0x201D))) return t; // “ ”
    if (strip_pair(QChar(0x2018), QChar(0x2019))) return t; // ‘ ’

    return t;
}

} // namespace cp::strings
