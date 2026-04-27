#include "IngestInputNode.h"

#include "IngestInputPropertiesWidget.h"
#include "MainWindow.h"
#include "NodeGraphModel.h"
#include "ToolNodeDelegate.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonObject>
#include <QMetaObject>
#include <QMimeData>
#include <QMimeDatabase>
#include <QPixmap>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QUuid>
#include <QUrl>
#include <limits>

namespace {

QString classifyKind(const QString& path, const QString& mimeType)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (mimeType.startsWith(QStringLiteral("image/"))) {
        return QStringLiteral("image");
    }
    if (mimeType == QStringLiteral("application/pdf") || suffix == QStringLiteral("pdf")) {
        return QStringLiteral("pdf");
    }
    if (suffix == QStringLiteral("md") || suffix == QStringLiteral("markdown")) {
        return QStringLiteral("markdown");
    }
    if (mimeType.startsWith(QStringLiteral("text/")) ||
        suffix == QStringLiteral("txt") ||
        suffix == QStringLiteral("json") ||
        suffix == QStringLiteral("xml") ||
        suffix == QStringLiteral("yaml") ||
        suffix == QStringLiteral("yml") ||
        suffix == QStringLiteral("csv") ||
        suffix == QStringLiteral("html") ||
        suffix == QStringLiteral("htm")) {
        return QStringLiteral("text");
    }
    return QStringLiteral("file");
}

QString previewTextForPath(const QString& path, const QString& kind)
{
    if (kind != QStringLiteral("markdown") && kind != QStringLiteral("text")) {
        return {};
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    QString content = stream.read(2000);
    file.close();

    if (content.size() == 2000) {
        content += QStringLiteral("\n...");
    }
    return content;
}

bool readUtf8File(const QString& path, QString* contentOut)
{
    if (!contentOut) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    *contentOut = stream.readAll();
    file.close();
    return true;
}

} // namespace

IngestInputNode::IngestInputNode(QObject* parent)
    : QObject(parent)
{
}

NodeDescriptor IngestInputNode::getDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("ingest-input");
    desc.name = QStringLiteral("Ingest Input");
    desc.category = QStringLiteral("Input / Output");

    const auto addOutput = [&desc](const char* id, const QString& name) {
        PinDefinition out;
        out.direction = PinDirection::Output;
        out.id = QString::fromLatin1(id);
        out.name = name;
        out.type = QStringLiteral("text");
        desc.outputPins.insert(out.id, out);
    };

    addOutput(kOutputMarkdownId, QStringLiteral("Markdown"));
    addOutput(kOutputTextId, QStringLiteral("Text"));
    addOutput(kOutputImageId, QStringLiteral("Image"));
    addOutput(kOutputPdfId, QStringLiteral("PDF"));
    addOutput(kOutputFilePathId, QStringLiteral("File Path"));
    addOutput(kOutputMimeTypeId, QStringLiteral("MIME Type"));
    addOutput(kOutputKindId, QStringLiteral("Kind"));

    return desc;
}

QWidget* IngestInputNode::createConfigurationWidget(QWidget* parent)
{
    if (!m_widget) {
        auto* widget = new IngestInputPropertiesWidget(parent);
        connect(widget, &IngestInputPropertiesWidget::fileChosen,
                this, &IngestInputNode::ingestFile);
        connect(widget, &IngestInputPropertiesWidget::clipboardPasteRequested,
                this, &IngestInputNode::ingestClipboard);
        m_widget = widget;
        updateWidget();
    } else if (parent && m_widget->parent() != parent) {
        m_widget->setParent(parent);
    }

    return m_widget;
}

TokenList IngestInputNode::execute(const TokenList& /*incomingTokens*/)
{
    DataPacket output;

    if (!m_sourcePath.trimmed().isEmpty()) {
        output.insert(QString::fromLatin1(kOutputFilePathId), m_sourcePath);
        output.insert(QString::fromLatin1(kOutputMimeTypeId), m_mimeType);
        output.insert(QString::fromLatin1(kOutputKindId), m_kind);

        if (m_kind == QStringLiteral("markdown") || m_kind == QStringLiteral("text")) {
            QString content;
            if (readUtf8File(m_sourcePath, &content)) {
                const QString pinId = (m_kind == QStringLiteral("markdown"))
                    ? QString::fromLatin1(kOutputMarkdownId)
                    : QString::fromLatin1(kOutputTextId);
                output.insert(pinId, content);
            } else {
                output.insert(QStringLiteral("__error"),
                              QStringLiteral("Failed to read ingested file: %1").arg(m_sourcePath));
            }
        } else if (m_kind == QStringLiteral("image")) {
            output.insert(QString::fromLatin1(kOutputImageId), m_sourcePath);
        } else if (m_kind == QStringLiteral("pdf")) {
            output.insert(QString::fromLatin1(kOutputPdfId), m_sourcePath);
        }
    }

    ExecutionToken token;
    token.data = output;
    token.forceExecution = !m_sourcePath.trimmed().isEmpty();

    return TokenList{token};
}

QJsonObject IngestInputNode::saveState() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("sourcePath"), m_sourcePath);
    obj.insert(QStringLiteral("mimeType"), m_mimeType);
    obj.insert(QStringLiteral("kind"), m_kind);
    return obj;
}

void IngestInputNode::loadState(const QJsonObject& data)
{
    m_sourcePath = data.value(QStringLiteral("sourcePath")).toString();
    m_mimeType = data.value(QStringLiteral("mimeType")).toString();
    m_kind = data.value(QStringLiteral("kind")).toString();

    if (!m_sourcePath.isEmpty() && (m_mimeType.isEmpty() || m_kind.isEmpty())) {
        QMimeDatabase mimeDb;
        m_mimeType = mimeDb.mimeTypeForFile(m_sourcePath).name();
        m_kind = classifyKind(m_sourcePath, m_mimeType);
    }

    updateWidget();
}

void IngestInputNode::ingestFile(const QString& path)
{
    if (!ingestLocalFile(path)) {
        return;
    }

    requestImmediateRun();
}

void IngestInputNode::ingestClipboard()
{
    QClipboard* clipboard = QApplication::clipboard();
    if (!clipboard) {
        return;
    }

    const QMimeData* mimeData = clipboard->mimeData();
    if (!mimeData) {
        return;
    }

    const QList<QUrl> urls = mimeData->urls();
    for (const QUrl& url : urls) {
        if (url.isLocalFile() && ingestLocalFile(url.toLocalFile())) {
            requestImmediateRun();
            return;
        }
    }

    const QImage image = clipboard->image();
    if (!image.isNull() && ingestClipboardImage(image)) {
        requestImmediateRun();
        return;
    }

    const QString text = mimeData->text().trimmed();
    if (!text.isEmpty() && ingestClipboardText(text)) {
        requestImmediateRun();
    }
}

bool IngestInputNode::ingestLocalFile(const QString& path)
{
    const QFileInfo fileInfo(path);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }

    QMimeDatabase mimeDb;
    m_sourcePath = fileInfo.absoluteFilePath();
    m_mimeType = mimeDb.mimeTypeForFile(m_sourcePath).name();
    m_kind = classifyKind(m_sourcePath, m_mimeType);
    updateWidget();
    return true;
}

bool IngestInputNode::ingestClipboardImage(const QImage& image)
{
    if (image.isNull()) {
        return false;
    }

    const QString dirPath = cacheDirectoryPath();
    if (dirPath.isEmpty()) {
        return false;
    }
    if (!QDir().mkpath(dirPath)) {
        return false;
    }

    const QString filePath = QDir(dirPath).filePath(
        QStringLiteral("clipboard_%1_%2.png")
            .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss")))
            .arg(QUuid::createUuid().toString(QUuid::Id128)));

    if (!image.save(filePath, "PNG")) {
        return false;
    }

    return ingestLocalFile(filePath);
}

bool IngestInputNode::ingestClipboardText(const QString& text)
{
    const QFileInfo maybePath(text);
    if (maybePath.exists() && maybePath.isFile()) {
        return ingestLocalFile(maybePath.absoluteFilePath());
    }

    const QString dirPath = cacheDirectoryPath();
    if (dirPath.isEmpty()) {
        return false;
    }
    if (!QDir().mkpath(dirPath)) {
        return false;
    }

    const QString filePath = QDir(dirPath).filePath(
        QStringLiteral("clipboard_%1_%2.md")
            .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss")))
            .arg(QUuid::createUuid().toString(QUuid::Id128)));

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    file.write(text.toUtf8());
    if (!file.commit()) {
        return false;
    }

    return ingestLocalFile(filePath);
}

void IngestInputNode::updateWidget()
{
    if (!m_widget) {
        return;
    }

    if (m_sourcePath.isEmpty()) {
        m_widget->clearPayload();
        return;
    }

    QPixmap previewPixmap;
    if (m_kind == QStringLiteral("image")) {
        previewPixmap.load(m_sourcePath);
    }

    m_widget->setPayload(m_kind,
                         m_sourcePath,
                         m_mimeType,
                         previewTextForPath(m_sourcePath, m_kind),
                         previewPixmap);
}

void IngestInputNode::requestImmediateRun()
{
    MainWindow* mainWindow = findMainWindow();
    if (!mainWindow) {
        return;
    }

    const unsigned int nodeId = resolveNodeId(mainWindow);
    if (nodeId == std::numeric_limits<unsigned int>::max()) {
        return;
    }

    QMetaObject::invokeMethod(mainWindow, [mainWindow, nodeId]() {
        mainWindow->runScenarioFromNodeId(nodeId);
    }, Qt::QueuedConnection);
}

MainWindow* IngestInputNode::findMainWindow() const
{
    QWidget* activeWindow = QApplication::activeWindow();
    if (activeWindow) {
        if (auto* mainWindow = qobject_cast<MainWindow*>(activeWindow)) {
            return mainWindow;
        }
    }

    for (QWidget* widget : QApplication::topLevelWidgets()) {
        if (auto* mainWindow = qobject_cast<MainWindow*>(widget)) {
            return mainWindow;
        }
    }

    return nullptr;
}

unsigned int IngestInputNode::resolveNodeId(const MainWindow* mainWindow) const
{
    if (!mainWindow || !mainWindow->graphModel()) {
        return std::numeric_limits<unsigned int>::max();
    }

    IToolNode* selfTool = const_cast<IngestInputNode*>(this);
    for (auto nodeId : mainWindow->graphModel()->allNodeIds()) {
        auto* delegate = mainWindow->graphModel()->delegateModel<ToolNodeDelegate>(nodeId);
        if (!delegate || !delegate->node()) {
            continue;
        }
        if (delegate->node().get() == selfTool) {
            return nodeId;
        }
    }

    return std::numeric_limits<unsigned int>::max();
}

QString IngestInputNode::cacheDirectoryPath() const
{
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (baseDir.isEmpty()) {
        baseDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    if (baseDir.isEmpty()) {
        baseDir = QDir::tempPath();
    }
    if (baseDir.isEmpty()) {
        baseDir = QDir::homePath();
    }

    return QDir(baseDir).filePath(QStringLiteral("ingest"));
}
