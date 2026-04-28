#pragma once

#include <QObject>
#include <QPointer>

#include "IToolNode.h"

class IngestInputPropertiesWidget;
class MainWindow;
class QImage;

class IngestInputNode : public QObject, public IToolNode {
    Q_OBJECT
    Q_INTERFACES(IToolNode)
public:
    explicit IngestInputNode(QObject* parent = nullptr);
    ~IngestInputNode() override = default;

    NodeDescriptor getDescriptor() const override;
    QWidget* createConfigurationWidget(QWidget* parent) override;
    TokenList execute(const TokenList& incomingTokens) override;
    QJsonObject saveState() const override;
    void loadState(const QJsonObject& data) override;

    static constexpr const char* kOutputMarkdownId = "markdown";
    static constexpr const char* kOutputTextId = "text";
    static constexpr const char* kOutputImageId = "image";
    static constexpr const char* kOutputPdfId = "pdf";
    static constexpr const char* kOutputFilePathId = "file_path";
    static constexpr const char* kOutputMimeTypeId = "mime_type";
    static constexpr const char* kOutputKindId = "kind";

public slots:
    void ingestFile(const QString& path);
    void ingestClipboard();

private:
    bool ingestLocalFile(const QString& path);
    bool ingestClipboardImage(const QImage& image);
    bool ingestClipboardText(const QString& text);
    void updateWidget();
    void setStatusMessage(const QString& message);
    void requestImmediateRun();
    MainWindow* findMainWindow() const;
    unsigned int resolveNodeId(const MainWindow* mainWindow) const;

    QString cacheDirectoryPath() const;

    QString m_sourcePath;
    QString m_mimeType;
    QString m_kind;
    QPointer<IngestInputPropertiesWidget> m_widget;
};
