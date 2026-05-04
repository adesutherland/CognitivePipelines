//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//

#include "SyntaxHighlightingOptionsDialog.h"
#include "ScriptHighlighterConfig.h"
#include "ScriptSyntaxHighlighter.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {

constexpr int kFileTypeColumn = 0;
constexpr int kCommandColumn = 1;
constexpr int kCheckColumn = 2;
constexpr int kStatusColumn = 3;

} // namespace

SyntaxHighlightingOptionsDialog::SyntaxHighlightingOptionsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Syntax Highlighting Options"));
    resize(760, 280);

    auto* layout = new QVBoxLayout(this);
    auto* label = new QLabel(tr("Configure external DSLSH parser commands. Leave a command blank to use the built-in fallback rules."), this);
    label->setWordWrap(true);
    layout->addWidget(label);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({tr("File Type"), tr("Highlighter Command"), tr("Check"), tr("Status")});
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(kFileTypeColumn, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(kCommandColumn, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(kCheckColumn, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(kStatusColumn, QHeaderView::ResizeToContents);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked
                             | QAbstractItemView::EditKeyPressed
                             | QAbstractItemView::AnyKeyPressed);
    layout->addWidget(m_table);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &SyntaxHighlightingOptionsDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &SyntaxHighlightingOptionsDialog::reject);
    layout->addWidget(m_buttons);

    populateTable();
}

void SyntaxHighlightingOptionsDialog::accept()
{
    ScriptHighlighterConfig::saveCommands(commandsFromTable());
    ScriptSyntaxHighlighter::rehighlightOpenEditors();
    QDialog::accept();
}

void SyntaxHighlightingOptionsDialog::onCheckHighlighter()
{
    const int row = rowForButton(sender());
    if (row < 0) {
        return;
    }

    const auto* fileTypeItem = m_table->item(row, kFileTypeColumn);
    const auto* commandItem = m_table->item(row, kCommandColumn);
    if (!fileTypeItem || !commandItem) {
        return;
    }

    QString message;
    const QString fileType = fileTypeItem->data(Qt::UserRole).toString();
    const bool ok = ScriptSyntaxHighlighter::checkHighlighterCommand(fileType, commandItem->text(), &message);
    setRowStatus(row, ok, message);
}

void SyntaxHighlightingOptionsDialog::populateTable()
{
    const QStringList fileTypes = ScriptHighlighterConfig::supportedFileTypes();
    const QMap<QString, QString> commands = ScriptHighlighterConfig::loadCommands();

    m_table->setRowCount(fileTypes.size());
    for (int row = 0; row < fileTypes.size(); ++row) {
        const QString& fileType = fileTypes.at(row);

        auto* fileTypeItem = new QTableWidgetItem(ScriptHighlighterConfig::displayNameForFileType(fileType));
        fileTypeItem->setData(Qt::UserRole, fileType);
        fileTypeItem->setFlags(fileTypeItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, kFileTypeColumn, fileTypeItem);

        auto* commandItem = new QTableWidgetItem(commands.value(fileType));
        commandItem->setToolTip(tr("Blank uses the built-in DSLSH fallback rules."));
        m_table->setItem(row, kCommandColumn, commandItem);

        auto* checkButton = new QPushButton(tr("Check"), m_table);
        connect(checkButton, &QPushButton::clicked, this, &SyntaxHighlightingOptionsDialog::onCheckHighlighter);
        m_table->setCellWidget(row, kCheckColumn, checkButton);

        auto* statusItem = new QTableWidgetItem(tr("Not checked"));
        statusItem->setFlags(statusItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, kStatusColumn, statusItem);
    }
}

QMap<QString, QString> SyntaxHighlightingOptionsDialog::commandsFromTable() const
{
    QMap<QString, QString> commands = ScriptHighlighterConfig::defaultCommands();
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const auto* fileTypeItem = m_table->item(row, kFileTypeColumn);
        const auto* commandItem = m_table->item(row, kCommandColumn);
        if (!fileTypeItem || !commandItem) {
            continue;
        }
        commands.insert(fileTypeItem->data(Qt::UserRole).toString(), commandItem->text().trimmed());
    }
    return commands;
}

int SyntaxHighlightingOptionsDialog::rowForButton(QObject* senderObject) const
{
    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (m_table->cellWidget(row, kCheckColumn) == senderObject) {
            return row;
        }
    }
    return -1;
}

void SyntaxHighlightingOptionsDialog::setRowStatus(int row, bool ok, const QString& message)
{
    auto* statusItem = m_table->item(row, kStatusColumn);
    if (!statusItem) {
        statusItem = new QTableWidgetItem();
        statusItem->setFlags(statusItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, kStatusColumn, statusItem);
    }

    statusItem->setText(message);
    statusItem->setToolTip(message);
    statusItem->setForeground(ok ? Qt::darkGreen : Qt::red);
}
