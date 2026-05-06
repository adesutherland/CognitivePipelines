//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//

#include "UniversalScriptPropertiesWidget.h"
#include "IScriptHost.h"
#include "ScriptEditorWidget.h"
#include "ScriptSyntaxHighlighter.h"
#include "UniversalScriptTemplates.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace {
QStringList pinsFromText(const QString& text)
{
    QString normalized = text;
    normalized.replace(QLatin1Char('\n'), QLatin1Char(','));
    normalized.replace(QLatin1Char(';'), QLatin1Char(','));
    QStringList pins = normalized.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (QString& pin : pins) {
        pin = pin.trimmed();
    }
    pins.removeAll(QString());
    pins.removeDuplicates();
    return pins;
}

QString textFromPins(const QStringList& pins)
{
    return pins.join(QStringLiteral(", "));
}
} // namespace

UniversalScriptPropertiesWidget::UniversalScriptPropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* formLayout = new QFormLayout();
    m_engineCombo = new QComboBox();
    formLayout->addRow(tr("Engine"), m_engineCombo);

    m_fanOutLabel = new QLabel(tr("Enable Fan-Out"));
    m_fanOutCheck = new QCheckBox();
    formLayout->addRow(m_fanOutLabel, m_fanOutCheck);

    m_inputPinsLabel = new QLabel(tr("Input Pins"));
    m_inputPinsEdit = new QLineEdit();
    m_inputPinsEdit->setPlaceholderText(tr("input"));
    formLayout->addRow(m_inputPinsLabel, m_inputPinsEdit);

    m_outputPinsLabel = new QLabel(tr("Output Pins"));
    m_outputPinsEdit = new QLineEdit();
    m_outputPinsEdit->setPlaceholderText(tr("output, status"));
    formLayout->addRow(m_outputPinsLabel, m_outputPinsEdit);

    m_syntaxHighlightCheck = new QCheckBox();
    m_syntaxHighlightCheck->setChecked(true);
    formLayout->addRow(tr("Syntax Highlighting"), m_syntaxHighlightCheck);

    mainLayout->addLayout(formLayout);

    m_scriptEditor = new ScriptEditorWidget();
    // Set a monospaced font
    const QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_scriptEditor->setFont(monoFont);

    auto* scriptHeaderLayout = new QHBoxLayout();
    scriptHeaderLayout->addWidget(new QLabel(tr("Script")));
    scriptHeaderLayout->addStretch();
    m_addExampleButton = new QPushButton(tr("Add Example"));
    scriptHeaderLayout->addWidget(m_addExampleButton);
    mainLayout->addLayout(scriptHeaderLayout);
    mainLayout->addWidget(m_scriptEditor);

    // Populate engine combo
    const auto engines = ScriptEngineRegistry::instance().registeredEngineIds();
    for (const auto& id : engines) {
        m_engineCombo->addItem(id);
    }

    // Connections
    connect(m_scriptEditor, &QPlainTextEdit::textChanged, this, &UniversalScriptPropertiesWidget::onScriptTextChanged);
    connect(m_engineCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &UniversalScriptPropertiesWidget::onEngineIndexChanged);
    connect(m_fanOutCheck, &QCheckBox::toggled, this, &UniversalScriptPropertiesWidget::fanOutChanged);
    connect(m_syntaxHighlightCheck, &QCheckBox::toggled, this, &UniversalScriptPropertiesWidget::onSyntaxHighlightingToggled);
    connect(m_inputPinsEdit, &QLineEdit::editingFinished, this, &UniversalScriptPropertiesWidget::onInputPinsEdited);
    connect(m_outputPinsEdit, &QLineEdit::editingFinished, this, &UniversalScriptPropertiesWidget::onOutputPinsEdited);
    connect(m_addExampleButton, &QPushButton::clicked, this, &UniversalScriptPropertiesWidget::onAddExampleClicked);

    maybeInstallTemplateForEngine(m_engineCombo->currentText(), false);
    updateAddExampleVisibility();
    applySyntaxHighlighter();
}

UniversalScriptPropertiesWidget::~UniversalScriptPropertiesWidget() = default;

void UniversalScriptPropertiesWidget::setScript(const QString& script)
{
    if (m_scriptEditor->toPlainText() != script) {
        QSignalBlocker blocker(m_scriptEditor);
        m_scriptEditor->setPlainText(script);
    }
    updateAddExampleVisibility();
}

void UniversalScriptPropertiesWidget::setEngineId(const QString& engineId)
{
    int index = m_engineCombo->findText(engineId);
    if (index != -1 && m_engineCombo->currentIndex() != index) {
        QSignalBlocker blocker(m_engineCombo);
        m_engineCombo->setCurrentIndex(index);
    }
    maybeInstallTemplateForEngine(engineId, false);
    updateAddExampleVisibility();
    applySyntaxHighlighter();
}

void UniversalScriptPropertiesWidget::setFanOut(bool enabled)
{
    if (m_fanOutCheck->isChecked() != enabled) {
        QSignalBlocker blocker(m_fanOutCheck);
        m_fanOutCheck->setChecked(enabled);
    }
}

void UniversalScriptPropertiesWidget::setFanOutVisible(bool visible)
{
    if (m_fanOutLabel) {
        m_fanOutLabel->setVisible(visible);
    }
    if (m_fanOutCheck) {
        m_fanOutCheck->setVisible(visible);
    }
}

void UniversalScriptPropertiesWidget::setSyntaxHighlighting(bool enabled)
{
    if (m_syntaxHighlightCheck->isChecked() != enabled) {
        QSignalBlocker blocker(m_syntaxHighlightCheck);
        m_syntaxHighlightCheck->setChecked(enabled);
    }
    applySyntaxHighlighter();
}

void UniversalScriptPropertiesWidget::setPinEditorsVisible(bool visible)
{
    if (m_inputPinsEdit) {
        m_inputPinsEdit->setVisible(visible);
    }
    if (m_inputPinsLabel) {
        m_inputPinsLabel->setVisible(visible);
    }
    if (m_outputPinsEdit) {
        m_outputPinsEdit->setVisible(visible);
    }
    if (m_outputPinsLabel) {
        m_outputPinsLabel->setVisible(visible);
    }
}

void UniversalScriptPropertiesWidget::setInputPins(const QStringList& pins)
{
    const QString text = textFromPins(pins);
    if (m_inputPinsEdit->text() != text) {
        QSignalBlocker blocker(m_inputPinsEdit);
        m_inputPinsEdit->setText(text);
    }
}

void UniversalScriptPropertiesWidget::setOutputPins(const QStringList& pins)
{
    const QString text = textFromPins(pins);
    if (m_outputPinsEdit->text() != text) {
        QSignalBlocker blocker(m_outputPinsEdit);
        m_outputPinsEdit->setText(text);
    }
}

QString UniversalScriptPropertiesWidget::script() const
{
    return m_scriptEditor->toPlainText();
}

QString UniversalScriptPropertiesWidget::engineId() const
{
    return m_engineCombo->currentText();
}

QStringList UniversalScriptPropertiesWidget::inputPins() const
{
    return pinsFromText(m_inputPinsEdit->text());
}

QStringList UniversalScriptPropertiesWidget::outputPins() const
{
    return pinsFromText(m_outputPinsEdit->text());
}

void UniversalScriptPropertiesWidget::onScriptTextChanged()
{
    updateAddExampleVisibility();
    emit scriptChanged(m_scriptEditor->toPlainText());
}

void UniversalScriptPropertiesWidget::onEngineIndexChanged(int index)
{
    Q_UNUSED(index);
    const QString engineId = m_engineCombo->currentText();
    maybeInstallTemplateForEngine(engineId, true);
    updateAddExampleVisibility();
    applySyntaxHighlighter();
    emit engineChanged(engineId);
}

void UniversalScriptPropertiesWidget::onSyntaxHighlightingToggled(bool enabled)
{
    applySyntaxHighlighter();
    emit syntaxHighlightingChanged(enabled);
}

void UniversalScriptPropertiesWidget::onInputPinsEdited()
{
    emit inputPinsChanged(inputPins());
}

void UniversalScriptPropertiesWidget::onOutputPinsEdited()
{
    emit outputPinsChanged(outputPins());
}

void UniversalScriptPropertiesWidget::onAddExampleClicked()
{
    if (!m_scriptEditor->toPlainText().trimmed().isEmpty()) {
        updateAddExampleVisibility();
        return;
    }

    m_scriptEditor->setPlainText(UniversalScriptTemplates::forEngine(m_engineCombo->currentText()));
    updateAddExampleVisibility();
}

void UniversalScriptPropertiesWidget::applySyntaxHighlighter()
{
    if (!m_syntaxHighlightCheck->isChecked() || !ScriptSyntaxHighlighter::backendAvailable()) {
        m_highlighter.reset();
        return;
    }

    if (!m_highlighter) {
        m_highlighter = std::make_unique<ScriptSyntaxHighlighter>(m_scriptEditor->document());
    }
    m_highlighter->setEngineId(m_engineCombo->currentText());
}

void UniversalScriptPropertiesWidget::maybeInstallTemplateForEngine(const QString& engineId, bool notify)
{
    const QString current = m_scriptEditor->toPlainText();
    if (current.trimmed().isEmpty()) {
        return;
    }

    if (!UniversalScriptTemplates::isManagedTemplate(current)) {
        return;
    }

    const QString next = UniversalScriptTemplates::forEngine(engineId);
    if (current == next) {
        return;
    }

    {
        QSignalBlocker blocker(m_scriptEditor);
        m_scriptEditor->setPlainText(next);
    }

    if (notify) {
        emit scriptChanged(next);
    }
}

void UniversalScriptPropertiesWidget::updateAddExampleVisibility()
{
    if (!m_addExampleButton) {
        return;
    }
    m_addExampleButton->setVisible(m_scriptEditor->toPlainText().trimmed().isEmpty());
}
