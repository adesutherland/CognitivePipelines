#include <gtest/gtest.h>

#include <QApplication>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

#include "test_app.h"
#include "MainWindow.h"
#include "NodeGraphModel.h"
#include "ToolNodeDelegate.h"
#include "IngestInputNode.h"

using namespace QtNodes;

namespace {

QApplication* ensureApp()
{
    return sharedTestApp();
}

bool writeTextFile(const QString& path, const QString& content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << content;
    file.close();
    return true;
}

} // namespace

TEST(IngestInputNodeTest, RegistrationAndMarkdownRouting)
{
    ensureApp();

    NodeGraphModel model;
    const NodeId nodeId = model.addNode(QStringLiteral("ingest-input"));
    ASSERT_NE(nodeId, InvalidNodeId);

    auto* delegate = model.delegateModel<ToolNodeDelegate>(nodeId);
    ASSERT_NE(delegate, nullptr);
    auto node = delegate->node();
    ASSERT_TRUE(node);

    auto* ingestNode = dynamic_cast<IngestInputNode*>(node.get());
    ASSERT_NE(ingestNode, nullptr);

    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    const QString markdownPath = tempDir.filePath(QStringLiteral("note.md"));
    const QString markdownContent = QStringLiteral("# Field Note\n\nThis came from the ingest node.");
    ASSERT_TRUE(writeTextFile(markdownPath, markdownContent));

    ingestNode->ingestFile(markdownPath);

    TokenList emptyInputs;
    const TokenList outTokens = ingestNode->execute(emptyInputs);
    ASSERT_FALSE(outTokens.empty());

    const DataPacket& output = outTokens.front().data;
    EXPECT_EQ(output.value(QStringLiteral("kind")).toString(), QStringLiteral("markdown"));
    EXPECT_EQ(output.value(QStringLiteral("file_path")).toString(), QFileInfo(markdownPath).absoluteFilePath());
    EXPECT_EQ(output.value(QStringLiteral("markdown")).toString(), markdownContent);
    EXPECT_FALSE(output.contains(QStringLiteral("pdf")));
    EXPECT_FALSE(output.contains(QStringLiteral("image")));
    EXPECT_TRUE(outTokens.front().forceExecution);
}

TEST(IngestInputNodeTest, PdfFilesRouteToPdfPin)
{
    ensureApp();

    IngestInputNode node;

    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    const QString pdfPath = tempDir.filePath(QStringLiteral("scan.pdf"));
    ASSERT_TRUE(writeTextFile(pdfPath, QStringLiteral("%PDF-1.7\nfake pdf body")));

    node.ingestFile(pdfPath);

    TokenList emptyInputs;
    const TokenList outTokens = node.execute(emptyInputs);
    ASSERT_FALSE(outTokens.empty());

    const DataPacket& output = outTokens.front().data;
    EXPECT_EQ(output.value(QStringLiteral("kind")).toString(), QStringLiteral("pdf"));
    EXPECT_EQ(output.value(QStringLiteral("pdf")).toString(), QFileInfo(pdfPath).absoluteFilePath());
    EXPECT_FALSE(output.contains(QStringLiteral("markdown")));
}

TEST(IngestInputNodeTest, IngestFileTriggersImmediateRunWhenHostedInMainWindow)
{
    ensureApp();

    MainWindow window;
    NodeGraphModel* model = window.graphModel();
    ASSERT_NE(model, nullptr);

    const NodeId nodeId = model->addNode(QStringLiteral("ingest-input"));
    ASSERT_NE(nodeId, InvalidNodeId);

    auto* delegate = model->delegateModel<ToolNodeDelegate>(nodeId);
    ASSERT_NE(delegate, nullptr);
    auto node = delegate->node();
    ASSERT_TRUE(node);

    auto* ingestNode = dynamic_cast<IngestInputNode*>(node.get());
    ASSERT_NE(ingestNode, nullptr);

    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    const QString markdownPath = tempDir.filePath(QStringLiteral("autorun.md"));
    const QString markdownContent = QStringLiteral("# Auto Run\n\nImmediate execution test.");
    ASSERT_TRUE(writeTextFile(markdownPath, markdownContent));

    DataPacket finalOutput;
    bool finished = false;
    QObject::connect(window.executionEngine(), &ExecutionEngine::pipelineFinished, &window,
                     [&](const DataPacket& packet) {
                         finished = true;
                         finalOutput = packet;
                     });

    ingestNode->ingestFile(markdownPath);

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(window.executionEngine(), &ExecutionEngine::pipelineFinished, &loop, &QEventLoop::quit);
    timeout.start(5000);
    loop.exec();

    ASSERT_TRUE(finished);
    EXPECT_EQ(finalOutput.value(QStringLiteral("kind")).toString(), QStringLiteral("markdown"));
    EXPECT_EQ(finalOutput.value(QStringLiteral("markdown")).toString(), markdownContent);
}
