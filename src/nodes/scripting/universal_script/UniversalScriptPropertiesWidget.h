//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//

#pragma once

#include <QWidget>
#include <QString>
#include <memory>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QCheckBox;
class QPushButton;
class QLabel;
class ScriptSyntaxHighlighter;

/**
 * @brief Properties widget for the Universal Script Node.
 * Allows selecting a script engine and editing the script code.
 */
class UniversalScriptPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    explicit UniversalScriptPropertiesWidget(QWidget* parent = nullptr);
    ~UniversalScriptPropertiesWidget() override;

    /**
     * @brief Sets the script content in the editor.
     */
    void setScript(const QString& script);

    /**
     * @brief Sets the selected engine ID in the combo box.
     */
    void setEngineId(const QString& engineId);

    /**
     * @brief Sets the fan-out state.
     */
    void setFanOut(bool enabled);

    void setFanOutVisible(bool visible);

    /**
     * @brief Sets whether syntax highlighting should be applied to the editor.
     */
    void setSyntaxHighlighting(bool enabled);

    void setPinEditorsVisible(bool visible);
    void setInputPins(const QStringList& pins);
    void setOutputPins(const QStringList& pins);

    /**
     * @brief Returns the current script content.
     */
    QString script() const;

    /**
     * @brief Returns the current selected engine ID.
     */
    QString engineId() const;
    QStringList inputPins() const;
    QStringList outputPins() const;

signals:
    /**
     * @brief Emitted when the script text is edited.
     */
    void scriptChanged(const QString& script);

    /**
     * @brief Emitted when the selected engine is changed.
     */
    void engineChanged(const QString& engineId);

    /**
     * @brief Emitted when the fan-out state is changed.
     */
    void fanOutChanged(bool enabled);

    void inputPinsChanged(const QStringList& pins);
    void outputPinsChanged(const QStringList& pins);

    /**
     * @brief Emitted when syntax highlighting is toggled.
     */
    void syntaxHighlightingChanged(bool enabled);

private slots:
    void onScriptTextChanged();
    void onEngineIndexChanged(int index);
    void onSyntaxHighlightingToggled(bool enabled);
    void onInputPinsEdited();
    void onOutputPinsEdited();
    void onAddExampleClicked();

private:
    void applySyntaxHighlighter();
    void maybeInstallTemplateForEngine(const QString& engineId, bool notify);
    void updateAddExampleVisibility();

    QComboBox* m_engineCombo;
    QLabel* m_fanOutLabel;
    QCheckBox* m_fanOutCheck;
    QCheckBox* m_syntaxHighlightCheck;
    QLabel* m_inputPinsLabel;
    QLineEdit* m_inputPinsEdit;
    QLabel* m_outputPinsLabel;
    QLineEdit* m_outputPinsEdit;
    QPushButton* m_addExampleButton;
    QPlainTextEdit* m_scriptEditor;
    std::unique_ptr<ScriptSyntaxHighlighter> m_highlighter;
};
