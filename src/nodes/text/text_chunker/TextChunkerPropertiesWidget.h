#pragma once

#include <QWidget>

class QComboBox;
class QSpinBox;
class TextChunkerNode;

class TextChunkerPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    explicit TextChunkerPropertiesWidget(TextChunkerNode* node, QWidget* parent = nullptr);
    ~TextChunkerPropertiesWidget() override = default;

public slots:
    void setChunkSize(int value);
    void setChunkOverlap(int value);
    void setFileType(const QString& fileType);

private:
    TextChunkerNode* m_node {nullptr};
    QSpinBox* m_chunkSizeSpin {nullptr};
    QSpinBox* m_chunkOverlapSpin {nullptr};
    QComboBox* m_fileTypeCombo {nullptr};
};
