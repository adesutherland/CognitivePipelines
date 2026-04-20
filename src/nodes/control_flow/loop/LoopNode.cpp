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

#include "LoopNode.h"
#include "LoopPropertiesWidget.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <algorithm>
#include <QRegularExpression>

LoopNode::LoopNode(QObject* parent)
    : QObject(parent)
{
}

NodeDescriptor LoopNode::getDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("loop-foreach");
    desc.name = QStringLiteral("Loop (For Each)");
    desc.category = QStringLiteral("Control Flow");

    // Input: list_in (Text)
    PinDefinition in;
    in.direction = PinDirection::Input;
    in.id = QString::fromLatin1(kInputListId);
    in.name = QStringLiteral("List (Text)");
    in.type = QStringLiteral("text");
    desc.inputPins.insert(in.id, in);

    // Output: body (Text)
    PinDefinition outBody;
    outBody.direction = PinDirection::Output;
    outBody.id = QString::fromLatin1(kOutputBodyId);
    outBody.name = QStringLiteral("Body");
    outBody.type = QStringLiteral("text");
    desc.outputPins.insert(outBody.id, outBody);

    // Output: passthrough (Text) — carries original, full input
    PinDefinition outPassthrough;
    outPassthrough.direction = PinDirection::Output;
    outPassthrough.id = QString::fromLatin1(kOutputPassthroughId);
    outPassthrough.name = QStringLiteral("Original List");
    outPassthrough.type = QStringLiteral("text");
    desc.outputPins.insert(outPassthrough.id, outPassthrough);

    return desc;
}

QWidget* LoopNode::createConfigurationWidget(QWidget* parent)
{
    auto* w = new LoopPropertiesWidget(parent);
    // Reflect last count updates into the widget (read-only informational)
    QObject::connect(this, &LoopNode::lastItemCountChanged, w, &LoopPropertiesWidget::setLastItemCount);
    return w;
}

TokenList LoopNode::execute(const TokenList& incomingTokens)
{
    TokenList outputs;
    const QString listKey = QString::fromLatin1(kInputListId);
    int totalItems = 0;

    for (const auto& token : incomingTokens) {
        if (!token.data.contains(listKey)) continue;

        const QString raw = token.data.value(listKey).toString();
        const QStringList items = parseItems(raw);
        totalItems += items.size();

        // Body tokens for this input token
        for (const QString& item : items) {
            DataPacket out;
            out.insert(QStringLiteral("text"), item);
            out.insert(QString::fromLatin1(kOutputBodyId), item);

            ExecutionToken tok;
            tok.data = out;
            outputs.push_back(std::move(tok));
        }

        // Passthrough token for this input token
        {
            DataPacket out;
            out.insert(QStringLiteral("text"), raw);
            out.insert(QString::fromLatin1(kOutputPassthroughId), raw);
            ExecutionToken tok;
            tok.data = out;
            outputs.push_back(std::move(tok));
        }
    }

    if (m_lastItemCount != totalItems) {
        m_lastItemCount = totalItems;
        emit lastItemCountChanged(m_lastItemCount);
    }

    return outputs;
}

QJsonObject LoopNode::saveState() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("lastItemCount"), m_lastItemCount);
    return obj;
}

void LoopNode::loadState(const QJsonObject& data)
{
    if (data.contains(QStringLiteral("lastItemCount"))) {
        m_lastItemCount = data.value(QStringLiteral("lastItemCount")).toInt();
        emit lastItemCountChanged(m_lastItemCount);
    }
}

QStringList LoopNode::parseItems(const QString& raw)
{
    const QString input = raw;
    const QString trimmedAll = input.trimmed();
    if (trimmedAll.isEmpty()) {
        return {};
    }

    auto filterNonEmpty = [](QStringList list){
        for (QString& s : list) s = s.trimmed();
        list.erase(std::remove_if(list.begin(), list.end(), [](const QString& s){ return s.trimmed().isEmpty(); }), list.end());
        return list;
    };

    // Priority 1: Whole payload as JSON array
    {
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(trimmedAll.toUtf8(), &err);
        if (err.error == QJsonParseError::NoError && doc.isArray()) {
            QStringList list;
            const QJsonArray arr = doc.array();
            for (const auto& v : arr) {
                if (v.isString()) list.push_back(v.toString());
                else if (v.isDouble()) list.push_back(QString::number(v.toDouble()));
                else if (v.isBool()) list.push_back(v.toBool() ? QStringLiteral("true") : QStringLiteral("false"));
                else if (v.isObject()) list.push_back(QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact)));
                else if (v.isArray()) list.push_back(QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact)));
                // nulls ignored
            }
            return filterNonEmpty(list);
        }
    }

    // Priority 2: Markdown fenced code block ```json ... ```
    {
        QRegularExpression re(R"(```\s*json\s*\n([\s\S]*?)\n```)", QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatch m = re.match(input);
        if (m.hasMatch()) {
            const QString code = m.captured(1).trimmed();
            QJsonParseError err;
            const QJsonDocument doc = QJsonDocument::fromJson(code.toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && doc.isArray()) {
                QStringList list;
                const QJsonArray arr = doc.array();
                for (const auto& v : arr) {
                    if (v.isString()) list.push_back(v.toString());
                    else if (v.isDouble()) list.push_back(QString::number(v.toDouble()));
                    else if (v.isBool()) list.push_back(v.toBool() ? QStringLiteral("true") : QStringLiteral("false"));
                    else if (v.isObject()) list.push_back(QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact)));
                    else if (v.isArray()) list.push_back(QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact)));
                }
                return filterNonEmpty(list);
            }
        }
    }

    // Priority 3: Markdown Lists (bulleted or numbered)
    {
        QRegularExpression re(R"(^\n?|(^|\n)\s*(?:[-*+]|\d+[\.\)])\s+(.+)$)",
                              QRegularExpression::MultilineOption);
        // We'll scan line-by-line manually to better control captures
        QStringList lines = input.split('\n');
        QStringList items;
        QRegularExpression bulletLine(R"(^\s*(?:[-*+]|\d+[\.\)])\s+(.*)\s*$)");
        for (const QString& line : lines) {
            auto mm = bulletLine.match(line);
            if (mm.hasMatch()) {
                const QString item = mm.captured(1).trimmed();
                if (!item.isEmpty()) items.push_back(item);
            }
        }
        if (!items.isEmpty()) {
            return filterNonEmpty(items);
        }
    }

    // Priority 4: Markdown Tables
    {
        // Heuristic: presence of at least two '|' in any non-empty line
        const QStringList lines = input.split('\n');
        bool hasPipe = false;
        for (const QString& l : lines) {
            if (l.count('|') >= 2) { hasPipe = true; break; }
        }
        if (hasPipe) {
            QStringList items;
            auto isSeparator = [](const QString& l){
                QString s = l.trimmed();
                // lines like |---|---| or ---|:---:|
                if (!s.contains('|')) return false;
                s.remove('|'); s.remove(':'); s = s.trimmed();
                for (const QChar& c : s) { if (c != '-' && !c.isSpace()) return false; }
                return true;
            };
            bool headerSkipped = false;
            for (const QString& l : lines) {
                if (l.trimmed().isEmpty()) continue;
                if (!headerSkipped) {
                    // Skip header and separator if present
                    if (l.contains('|')) { headerSkipped = true; continue; }
                }
                if (isSeparator(l)) continue;
                if (!l.contains('|')) continue;
                QString row = l.trimmed();
                // Strip leading/trailing pipes
                if (row.startsWith('|')) row.remove(0, 1);
                if (row.endsWith('|')) row.chop(1);
                row = row.trimmed();
                if (!row.isEmpty()) items.push_back(row);
            }
            if (!items.isEmpty()) {
                return filterNonEmpty(items);
            }
        }
    }

    // Priority 5: Fallback newline split
    {
        QStringList list = input.split('\n', Qt::KeepEmptyParts);
        return filterNonEmpty(list);
    }
}
