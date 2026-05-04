#include "TextChunkerPropertiesWidget.h"

#include "TextChunkerNode.h"

#include <QComboBox>
#include <QFormLayout>
#include <QSpinBox>

TextChunkerPropertiesWidget::TextChunkerPropertiesWidget(TextChunkerNode* node, QWidget* parent)
    : QWidget(parent)
    , m_node(node)
{
    auto* form = new QFormLayout(this);

    m_chunkSizeSpin = new QSpinBox(this);
    m_chunkSizeSpin->setRange(1, 1000000);
    form->addRow(tr("Chunk size"), m_chunkSizeSpin);

    m_chunkOverlapSpin = new QSpinBox(this);
    m_chunkOverlapSpin->setRange(0, 999999);
    form->addRow(tr("Overlap"), m_chunkOverlapSpin);

    m_fileTypeCombo = new QComboBox(this);
    m_fileTypeCombo->addItem(tr("Plain text"), QStringLiteral("plain"));
    m_fileTypeCombo->addItem(tr("C / C++ / C-family"), QStringLiteral("cpp"));
    m_fileTypeCombo->addItem(tr("Python"), QStringLiteral("python"));
    m_fileTypeCombo->addItem(tr("Rexx"), QStringLiteral("rexx"));
    m_fileTypeCombo->addItem(tr("SQL"), QStringLiteral("sql"));
    m_fileTypeCombo->addItem(tr("Shell"), QStringLiteral("shell"));
    m_fileTypeCombo->addItem(tr("Cobol"), QStringLiteral("cobol"));
    m_fileTypeCombo->addItem(tr("Markdown"), QStringLiteral("markdown"));
    m_fileTypeCombo->addItem(tr("YAML / HCL"), QStringLiteral("yaml"));
    form->addRow(tr("File type"), m_fileTypeCombo);

    if (m_node) {
        setChunkSize(m_node->chunkSize());
        setChunkOverlap(m_node->chunkOverlap());
        setFileType(m_node->fileType());

        connect(m_chunkSizeSpin, &QSpinBox::valueChanged,
                m_node, &TextChunkerNode::setChunkSize);
        connect(m_chunkOverlapSpin, &QSpinBox::valueChanged,
                m_node, &TextChunkerNode::setChunkOverlap);
        connect(m_fileTypeCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
            if (m_node) {
                m_node->setFileType(m_fileTypeCombo->itemData(index).toString());
            }
        });

        connect(m_node, &TextChunkerNode::chunkSizeChanged,
                this, &TextChunkerPropertiesWidget::setChunkSize);
        connect(m_node, &TextChunkerNode::chunkOverlapChanged,
                this, &TextChunkerPropertiesWidget::setChunkOverlap);
        connect(m_node, &TextChunkerNode::fileTypeChanged,
                this, &TextChunkerPropertiesWidget::setFileType);
    }
}

void TextChunkerPropertiesWidget::setChunkSize(int value)
{
    if (m_chunkSizeSpin && m_chunkSizeSpin->value() != value) {
        m_chunkSizeSpin->setValue(value);
    }
    if (m_chunkOverlapSpin) {
        m_chunkOverlapSpin->setMaximum(qMax(0, value - 1));
    }
}

void TextChunkerPropertiesWidget::setChunkOverlap(int value)
{
    if (m_chunkOverlapSpin && m_chunkOverlapSpin->value() != value) {
        m_chunkOverlapSpin->setValue(value);
    }
}

void TextChunkerPropertiesWidget::setFileType(const QString& fileType)
{
    if (!m_fileTypeCombo) {
        return;
    }
    const int index = m_fileTypeCombo->findData(fileType);
    if (index >= 0 && index != m_fileTypeCombo->currentIndex()) {
        m_fileTypeCombo->setCurrentIndex(index);
    }
}
