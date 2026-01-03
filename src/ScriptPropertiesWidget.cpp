//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//

#include "ScriptPropertiesWidget.h"
#include "IScriptHost.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QFontDatabase>
#include <QLabel>

ScriptPropertiesWidget::ScriptPropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* formLayout = new QFormLayout();
    m_engineCombo = new QComboBox();
    formLayout->addRow(tr("Engine"), m_engineCombo);
    mainLayout->addLayout(formLayout);

    m_scriptEditor = new QPlainTextEdit();
    // Set a monospaced font
    const QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_scriptEditor->setFont(monoFont);
    
    mainLayout->addWidget(new QLabel(tr("Script")));
    mainLayout->addWidget(m_scriptEditor);

    // Populate engine combo
    const auto engines = ScriptEngineRegistry::instance().registeredEngineIds();
    for (const auto& id : engines) {
        m_engineCombo->addItem(id);
    }

    // Connections
    connect(m_scriptEditor, &QPlainTextEdit::textChanged, this, &ScriptPropertiesWidget::onScriptTextChanged);
    connect(m_engineCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ScriptPropertiesWidget::onEngineIndexChanged);
}

void ScriptPropertiesWidget::setScript(const QString& script)
{
    if (m_scriptEditor->toPlainText() != script) {
        QSignalBlocker blocker(m_scriptEditor);
        m_scriptEditor->setPlainText(script);
    }
}

void ScriptPropertiesWidget::setEngineId(const QString& engineId)
{
    int index = m_engineCombo->findText(engineId);
    if (index != -1 && m_engineCombo->currentIndex() != index) {
        QSignalBlocker blocker(m_engineCombo);
        m_engineCombo->setCurrentIndex(index);
    }
}

QString ScriptPropertiesWidget::script() const
{
    return m_scriptEditor->toPlainText();
}

QString ScriptPropertiesWidget::engineId() const
{
    return m_engineCombo->currentText();
}

void ScriptPropertiesWidget::onScriptTextChanged()
{
    emit scriptChanged(m_scriptEditor->toPlainText());
}

void ScriptPropertiesWidget::onEngineIndexChanged(int index)
{
    Q_UNUSED(index);
    emit engineChanged(m_engineCombo->currentText());
}
