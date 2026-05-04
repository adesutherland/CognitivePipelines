//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//

#pragma once

#include <QPlainTextEdit>

class QWidget;

/**
 * @brief Plain-text script editor with tooltip support for highlighter diagnostics.
 */
class ScriptEditorWidget : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit ScriptEditorWidget(QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool viewportEvent(QEvent* event) override;

private slots:
    void updateLineNumberAreaWidth();
    void updateLineNumberArea(const QRect& rect, int dy);
    void highlightCurrentLine();

private:
    int lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent* event);

    QWidget* m_lineNumberArea {nullptr};

    friend class ScriptLineNumberArea;
};
