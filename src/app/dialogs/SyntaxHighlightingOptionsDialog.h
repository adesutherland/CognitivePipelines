//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//

#pragma once

#include <QDialog>
#include <QMap>
#include <QString>

class QDialogButtonBox;
class QTableWidget;

/**
 * @brief Application-level syntax highlighting settings dialog.
 */
class SyntaxHighlightingOptionsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SyntaxHighlightingOptionsDialog(QWidget* parent = nullptr);

private slots:
    void accept() override;
    void onCheckHighlighter();

private:
    void populateTable();
    QMap<QString, QString> commandsFromTable() const;
    int rowForButton(QObject* sender) const;
    void setRowStatus(int row, bool ok, const QString& message);

    QTableWidget* m_table {nullptr};
    QDialogButtonBox* m_buttons {nullptr};
};
