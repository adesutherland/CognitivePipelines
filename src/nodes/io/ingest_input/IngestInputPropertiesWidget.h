#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QPlainTextEdit;
class QPixmap;

class IngestInputPropertiesWidget : public QWidget {
    Q_OBJECT
public:
    explicit IngestInputPropertiesWidget(QWidget* parent = nullptr);
    ~IngestInputPropertiesWidget() override = default;

    void setPayload(const QString& kind,
                    const QString& sourcePath,
                    const QString& mimeType,
                    const QString& previewText,
                    const QPixmap& previewPixmap);
    void clearPayload();
    void setStatusMessage(const QString& message);

signals:
    void fileChosen(const QString& path);
    void clipboardPasteRequested();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void updatePreviewVisibility(bool hasTextPreview, bool hasImagePreview);

    QLabel* m_dropLabel {nullptr};
    QLabel* m_statusLabel {nullptr};
    QLabel* m_runStatusLabel {nullptr};
    QLabel* m_pathLabel {nullptr};
    QLabel* m_imagePreviewLabel {nullptr};
    QPlainTextEdit* m_textPreviewEdit {nullptr};
    QPushButton* m_selectButton {nullptr};
    QPushButton* m_pasteButton {nullptr};
};
