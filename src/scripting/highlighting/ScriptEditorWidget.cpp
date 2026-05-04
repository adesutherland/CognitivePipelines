//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//

#include "ScriptEditorWidget.h"

#include <QColor>
#include <QEvent>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextFormat>
#include <QTextLayout>
#include <QToolTip>

namespace {

QString tooltipForColumn(const QTextBlock& block, int column)
{
    if (!block.isValid() || column < 0 || !block.layout()) {
        return QString();
    }

    const QList<QTextLayout::FormatRange> formats = block.layout()->formats();
    for (const QTextLayout::FormatRange& range : formats) {
        if (column >= range.start && column < range.start + range.length) {
            const QString tooltip = range.format.toolTip();
            if (!tooltip.isEmpty()) {
                return tooltip;
            }
        }
    }

    return QString();
}

QString tooltipForCursor(const QTextCursor& cursor)
{
    const QTextBlock block = cursor.block();
    const int column = cursor.position() - block.position();

    QString tooltip = tooltipForColumn(block, column);
    if (tooltip.isEmpty() && column > 0) {
        tooltip = tooltipForColumn(block, column - 1);
    }

    return tooltip;
}

} // namespace

class ScriptLineNumberArea : public QWidget {
public:
    explicit ScriptLineNumberArea(ScriptEditorWidget* editor)
        : QWidget(editor)
        , m_editor(editor)
    {
    }

    QSize sizeHint() const override
    {
        return QSize(m_editor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        m_editor->lineNumberAreaPaintEvent(event);
    }

private:
    ScriptEditorWidget* m_editor;
};

ScriptEditorWidget::ScriptEditorWidget(QWidget* parent)
    : QPlainTextEdit(parent)
{
    m_lineNumberArea = new ScriptLineNumberArea(this);

    setLineWrapMode(QPlainTextEdit::NoWrap);
    setTabStopDistance(QFontMetricsF(font()).horizontalAdvance(QLatin1Char(' ')) * 4);
    setStyleSheet(QStringLiteral(
        "QPlainTextEdit {"
        "  background: #fbfcfe;"
        "  color: #1f2937;"
        "  border: 1px solid #cbd5e1;"
        "  selection-background-color: #bfdbfe;"
        "}"));

    connect(this, &QPlainTextEdit::blockCountChanged, this, &ScriptEditorWidget::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &ScriptEditorWidget::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &ScriptEditorWidget::highlightCurrentLine);

    updateLineNumberAreaWidth();
    highlightCurrentLine();
}

void ScriptEditorWidget::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);

    const QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void ScriptEditorWidget::changeEvent(QEvent* event)
{
    QPlainTextEdit::changeEvent(event);
    if (event->type() == QEvent::FontChange) {
        setTabStopDistance(QFontMetricsF(font()).horizontalAdvance(QLatin1Char(' ')) * 4);
        updateLineNumberAreaWidth();
        m_lineNumberArea->update();
    }
}

bool ScriptEditorWidget::viewportEvent(QEvent* event)
{
    if (event->type() == QEvent::ToolTip) {
        const auto* helpEvent = static_cast<QHelpEvent*>(event);
        const QTextCursor cursor = cursorForPosition(helpEvent->pos());
        const QString tooltip = tooltipForCursor(cursor);

        if (!tooltip.isEmpty()) {
            QToolTip::showText(helpEvent->globalPos(), tooltip, viewport());
            return true;
        }

        QToolTip::hideText();
        event->ignore();
        return true;
    }

    return QPlainTextEdit::viewportEvent(event);
}

void ScriptEditorWidget::updateLineNumberAreaWidth()
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void ScriptEditorWidget::updateLineNumberArea(const QRect& rect, int dy)
{
    if (dy) {
        m_lineNumberArea->scroll(0, dy);
    } else {
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth();
    }
}

void ScriptEditorWidget::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> selections;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        QColor lineColor(QStringLiteral("#eef6ff"));
        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        selections.append(selection);
    }

    setExtraSelections(selections);
}

int ScriptEditorWidget::lineNumberAreaWidth() const
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

    return 10 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

void ScriptEditorWidget::lineNumberAreaPaintEvent(QPaintEvent* event)
{
    QPainter painter(m_lineNumberArea);
    painter.setFont(font());
    painter.fillRect(event->rect(), QColor(QStringLiteral("#f1f5f9")));
    painter.setPen(QColor(QStringLiteral("#94a3b8")));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            const QString number = QString::number(blockNumber + 1);
            painter.drawText(0, top, m_lineNumberArea->width() - 4, fontMetrics().height(),
                             Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}
