//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//

#pragma once

#include <QWidget>
#include <QString>

class QComboBox;
class QPlainTextEdit;

/**
 * @brief Properties widget for the Universal Script Node.
 * Allows selecting a script engine and editing the script code.
 */
class ScriptPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    explicit ScriptPropertiesWidget(QWidget* parent = nullptr);
    ~ScriptPropertiesWidget() override = default;

    /**
     * @brief Sets the script content in the editor.
     */
    void setScript(const QString& script);

    /**
     * @brief Sets the selected engine ID in the combo box.
     */
    void setEngineId(const QString& engineId);

    /**
     * @brief Returns the current script content.
     */
    QString script() const;

    /**
     * @brief Returns the current selected engine ID.
     */
    QString engineId() const;

signals:
    /**
     * @brief Emitted when the script text is edited.
     */
    void scriptChanged(const QString& script);

    /**
     * @brief Emitted when the selected engine is changed.
     */
    void engineChanged(const QString& engineId);

private slots:
    void onScriptTextChanged();
    void onEngineIndexChanged(int index);

private:
    QComboBox* m_engineCombo;
    QPlainTextEdit* m_scriptEditor;
};
