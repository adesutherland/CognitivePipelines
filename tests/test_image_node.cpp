#include <gtest/gtest.h>

#include <QApplication>
#include <QTemporaryFile>
#include <QtNodes/internal/Definitions.hpp>

#include "NodeGraphModel.h"
#include "ImageNode.h"
#include "ToolNodeDelegate.h"

using namespace QtNodes;

// Ensure a QApplication exists for widget-based operations
static QApplication* ensureApp()
{
    static QApplication* app = nullptr;
    if (!app) {
        static int argc = 1;
        static char appName[] = "unit_tests";
        static char* argv[] = { appName, nullptr };
        app = new QApplication(argc, argv);
    }
    return app;
}

TEST(ImageNodeTest, Registration)
{
    ensureApp();

    NodeGraphModel model;

    // Attempt to create an ImageNode via the registry
    NodeId nodeId = model.addNode(QStringLiteral("image-node"));

    // Verify the node was created successfully
    ASSERT_NE(nodeId, InvalidNodeId);

    // Optionally verify it's wrapped in a ToolNodeDelegate
    auto* delegate = model.delegateModel<ToolNodeDelegate>(nodeId);
    ASSERT_NE(delegate, nullptr);

    // Verify the underlying connector is an ImageNode
    auto connector = delegate->connector();
    ASSERT_TRUE(connector);
    auto* imageNode = dynamic_cast<ImageNode*>(connector.get());
    ASSERT_NE(imageNode, nullptr);
}

TEST(ImageNodeTest, SourceModeExecution)
{
    ensureApp();

    // Create a temporary file to simulate an image file
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    const QString imagePath = tempFile.fileName();
    tempFile.close(); // Close but keep the file for the test

    // Create an ImageNode instance
    ImageNode node;

    // Configure the node's internal image path using loadState
    QJsonObject state;
    state.insert(QStringLiteral("imagePath"), imagePath);
    node.loadState(state);

    // Execute with empty input (Source Mode)
    DataPacket emptyInput;
    QFuture<DataPacket> future = node.Execute(emptyInput);
    future.waitForFinished();
    DataPacket output = future.result();

    // Verify the output contains the correct image path
    const QString pinId = QString::fromLatin1(ImageNode::kImagePinId);
    ASSERT_TRUE(output.contains(pinId));
    EXPECT_EQ(output.value(pinId).toString(), imagePath);
}

TEST(ImageNodeTest, PassThroughModeExecution)
{
    ensureApp();

    // Create an ImageNode instance
    ImageNode node;

    // Prepare input with an upstream image path
    const QString upstreamPath = QStringLiteral("/path/to/upstream.png");
    DataPacket input;
    const QString pinId = QString::fromLatin1(ImageNode::kImagePinId);
    input.insert(pinId, upstreamPath);

    // Execute with input (Pass-Through Mode)
    QFuture<DataPacket> future = node.Execute(input);
    future.waitForFinished();
    DataPacket output = future.result();

    // Verify the output contains the upstream path (input takes precedence)
    ASSERT_TRUE(output.contains(pinId));
    EXPECT_EQ(output.value(pinId).toString(), upstreamPath);
}
