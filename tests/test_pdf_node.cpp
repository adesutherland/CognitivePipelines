#include <gtest/gtest.h>

#include <QApplication>
#include <QTemporaryFile>
#include <QtNodes/internal/Definitions.hpp>

#include "test_app.h"
#include "NodeGraphModel.h"
#include "PdfToImageNode.h"
#include "ToolNodeDelegate.h"

using namespace QtNodes;

// Ensure a QApplication exists for widget-based operations
static QApplication* ensureApp()
{
    return sharedTestApp();
}

TEST(PdfToImageNodeTest, Registration)
{
    ensureApp();

    NodeGraphModel model;

    // Attempt to create a PdfToImageNode via the registry
    NodeId nodeId = model.addNode(QStringLiteral("pdf-to-image"));

    // Verify the node was created successfully
    ASSERT_NE(nodeId, InvalidNodeId);

    // Optionally verify it's wrapped in a ToolNodeDelegate
    auto* delegate = model.delegateModel<ToolNodeDelegate>(nodeId);
    ASSERT_NE(delegate, nullptr);

    // Verify the underlying connector is a PdfToImageNode
    auto connector = delegate->connector();
    ASSERT_TRUE(connector);
    auto* pdfNode = dynamic_cast<PdfToImageNode*>(connector.get());
    ASSERT_NE(pdfNode, nullptr);
}

TEST(PdfToImageNodeTest, SourceModeExecution_MissingFile)
{
    ensureApp();

    // Use a non-existent PDF path to test error handling
    const QString nonExistentPath = QStringLiteral("/tmp/nonexistent_test_file.pdf");

    // Create a PdfToImageNode instance
    PdfToImageNode node;

    // Configure the node's internal PDF path using loadState
    QJsonObject state;
    state.insert(QStringLiteral("pdf_path"), nonExistentPath);
    node.loadState(state);

    // Execute with empty input (Source Mode) via V3 token API
    DataPacket emptyInput;
    ExecutionToken token;
    token.data = emptyInput;
    TokenList tokens;
    tokens.push_back(std::move(token));

    const TokenList outTokens = node.execute(tokens);
    ASSERT_FALSE(outTokens.empty());
    DataPacket output = outTokens.front().data;

    // Verify the output is empty (graceful error handling)
    // When the PDF file doesn't exist or can't be loaded, the node should return an empty packet
    const QString outputPinId = QString::fromLatin1(PdfToImageNode::kImagePathPinId);
    EXPECT_FALSE(output.contains(outputPinId) && !output.value(outputPinId).toString().isEmpty());
}

TEST(PdfToImageNodeTest, SaveLoadState)
{
    ensureApp();

    // Create a PdfToImageNode instance
    PdfToImageNode node;

    // Set a PDF path via loadState
    const QString testPath = QStringLiteral("/path/to/test.pdf");
    QJsonObject stateIn;
    stateIn.insert(QStringLiteral("pdf_path"), testPath);
    node.loadState(stateIn);

    // Save the state
    QJsonObject stateOut = node.saveState();

    // Verify the state was saved correctly
    ASSERT_TRUE(stateOut.contains(QStringLiteral("pdf_path")));
    EXPECT_EQ(stateOut.value(QStringLiteral("pdf_path")).toString(), testPath);
}
