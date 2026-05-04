#include "TextChunkerNode.h"
#include "TextChunkerPropertiesWidget.h"

#include "retrieval/chunking/TextChunker.h"

#include <QJsonObject>

namespace {
void addInput(NodeDescriptor& desc, const QString& id, const QString& name)
{
    PinDefinition pin;
    pin.direction = PinDirection::Input;
    pin.id = id;
    pin.name = name;
    pin.type = QStringLiteral("text");
    desc.inputPins.insert(pin.id, pin);
}

void addOutput(NodeDescriptor& desc, const QString& id, const QString& name)
{
    PinDefinition pin;
    pin.direction = PinDirection::Output;
    pin.id = id;
    pin.name = name;
    pin.type = QStringLiteral("text");
    desc.outputPins.insert(pin.id, pin);
}

FileType fileTypeFromString(const QString& value)
{
    const QString v = value.trimmed().toLower();
    if (v == QStringLiteral("cpp")) return FileType::CodeCpp;
    if (v == QStringLiteral("python")) return FileType::CodePython;
    if (v == QStringLiteral("rexx")) return FileType::CodeRexx;
    if (v == QStringLiteral("sql")) return FileType::CodeSql;
    if (v == QStringLiteral("shell")) return FileType::CodeShell;
    if (v == QStringLiteral("cobol")) return FileType::CodeCobol;
    if (v == QStringLiteral("markdown")) return FileType::CodeMarkdown;
    if (v == QStringLiteral("yaml")) return FileType::CodeYaml;
    return FileType::PlainText;
}

QString normalizeFileType(const QString& value)
{
    const QString v = value.trimmed().toLower();
    if (v == QStringLiteral("cpp") ||
        v == QStringLiteral("python") ||
        v == QStringLiteral("rexx") ||
        v == QStringLiteral("sql") ||
        v == QStringLiteral("shell") ||
        v == QStringLiteral("cobol") ||
        v == QStringLiteral("markdown") ||
        v == QStringLiteral("yaml")) {
        return v;
    }
    return QStringLiteral("plain");
}
}

TextChunkerNode::TextChunkerNode(QObject* parent)
    : QObject(parent)
{
}

NodeDescriptor TextChunkerNode::getDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("text-chunker");
    desc.name = QStringLiteral("Text Chunker");
    desc.category = QStringLiteral("Text Utilities");

    addInput(desc, QString::fromLatin1(kInputTextId), QStringLiteral("Text"));
    addOutput(desc, QString::fromLatin1(kOutputChunksId), QStringLiteral("Chunks"));
    addOutput(desc, QString::fromLatin1(kOutputCountId), QStringLiteral("Count"));
    addOutput(desc, QString::fromLatin1(kOutputSummaryId), QStringLiteral("Summary"));
    addOutput(desc, QString::fromLatin1(kOutputTextId), QStringLiteral("Text"));

    return desc;
}

QWidget* TextChunkerNode::createConfigurationWidget(QWidget* parent)
{
    return new TextChunkerPropertiesWidget(this, parent);
}

bool TextChunkerNode::isReady(const QVariantMap& inputs, int incomingConnectionsCount) const
{
    Q_UNUSED(incomingConnectionsCount);
    return inputs.contains(QString::fromLatin1(kInputTextId));
}

TokenList TextChunkerNode::execute(const TokenList& incomingTokens)
{
    DataPacket inputs;
    for (const auto& token : incomingTokens) {
        for (auto it = token.data.cbegin(); it != token.data.cend(); ++it) {
            inputs.insert(it.key(), it.value());
        }
    }

    const QString text = inputs.value(QString::fromLatin1(kInputTextId)).toString();
    const QStringList chunks = TextChunker::split(text, m_chunkSize, m_chunkOverlap, fileTypeFromString(m_fileType));

    QVariantList chunkList;
    chunkList.reserve(chunks.size());
    for (const QString& chunk : chunks) {
        chunkList.append(chunk);
    }

    QVariantMap summary;
    summary.insert(QStringLiteral("chunk_size"), m_chunkSize);
    summary.insert(QStringLiteral("chunk_overlap"), m_chunkOverlap);
    summary.insert(QStringLiteral("file_type"), m_fileType);
    summary.insert(QStringLiteral("count"), chunkList.size());

    DataPacket output;
    output.insert(QString::fromLatin1(kOutputChunksId), chunkList);
    output.insert(QString::fromLatin1(kOutputTextId), chunkList);
    output.insert(QString::fromLatin1(kOutputCountId), chunkList.size());
    output.insert(QString::fromLatin1(kOutputSummaryId), summary);

    ExecutionToken token;
    token.data = output;
    return TokenList{std::move(token)};
}

QJsonObject TextChunkerNode::saveState() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("chunk_size"), m_chunkSize);
    obj.insert(QStringLiteral("chunk_overlap"), m_chunkOverlap);
    obj.insert(QStringLiteral("file_type"), m_fileType);
    return obj;
}

void TextChunkerNode::loadState(const QJsonObject& data)
{
    if (data.contains(QStringLiteral("chunk_size"))) {
        setChunkSize(data.value(QStringLiteral("chunk_size")).toInt(m_chunkSize));
    }
    if (data.contains(QStringLiteral("chunk_overlap"))) {
        setChunkOverlap(data.value(QStringLiteral("chunk_overlap")).toInt(m_chunkOverlap));
    }
    if (data.contains(QStringLiteral("file_type"))) {
        setFileType(data.value(QStringLiteral("file_type")).toString(m_fileType));
    }
}

void TextChunkerNode::setChunkSize(int value)
{
    const int clamped = qBound(1, value, 1000000);
    if (clamped == m_chunkSize) {
        emit chunkSizeChanged(m_chunkSize);
        return;
    }
    m_chunkSize = clamped;
    if (m_chunkOverlap >= m_chunkSize) {
        m_chunkOverlap = qMax(0, m_chunkSize - 1);
        emit chunkOverlapChanged(m_chunkOverlap);
    }
    emit chunkSizeChanged(m_chunkSize);
}

void TextChunkerNode::setChunkOverlap(int value)
{
    const int clamped = qBound(0, value, qMax(0, m_chunkSize - 1));
    if (clamped == m_chunkOverlap) {
        emit chunkOverlapChanged(m_chunkOverlap);
        return;
    }
    m_chunkOverlap = clamped;
    emit chunkOverlapChanged(m_chunkOverlap);
}

void TextChunkerNode::setFileType(const QString& fileType)
{
    const QString normalized = normalizeFileType(fileType);
    if (normalized == m_fileType) {
        emit fileTypeChanged(m_fileType);
        return;
    }
    m_fileType = normalized;
    emit fileTypeChanged(m_fileType);
}
