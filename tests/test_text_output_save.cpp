//
// Cognitive Pipeline Application - Unit Test for TextOutput Save Behavior
//
// Verifies that TextOutputNode content is NOT saved to JSON files.
// Output nodes should only display runtime results, not persist them.
//

#include <gtest/gtest.h>

#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "test_app.h"
#include "NodeGraphModel.h"
#include "ExecutionEngine.h"
#include "ToolNodeDelegate.h"
#include "TextInputNode.h"
#include "TextOutputNode.h"

using namespace QtNodes;

static QApplication* ensureApp_TextOutputSave()
{
    return sharedTestApp();
}

// Helper: run engine and wait up to timeout
static bool runEngineAndWait(ExecutionEngine& engine, int timeoutMs = 5000)
{
    bool finished = false;
    QObject::connect(&engine, &ExecutionEngine::pipelineFinished, &engine, [&](const DataPacket&) {
        finished = true;
    });
    engine.run();
    QEventLoop loop;
    QTimer timeout; timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&engine, &ExecutionEngine::pipelineFinished, &loop, &QEventLoop::quit);
    timeout.start(timeoutMs);
    loop.exec();
    return finished;
}

TEST(TextOutputSaveTest, OutputContentShouldNotBeSaved)
{
    ensureApp_TextOutputSave();

    NodeGraphModel model;

    // Create a simple pipeline: TextInput -> TextOutput
    NodeId inputId = model.addNode(QStringLiteral("text-input"));
    NodeId outputId = model.addNode(QStringLiteral("text-output"));

    ASSERT_NE(inputId, InvalidNodeId);
    ASSERT_NE(outputId, InvalidNodeId);

    // Connect input to output
    model.addConnection(ConnectionId{ inputId, 0u, outputId, 0u });

    // Configure input text
    auto* inDel = model.delegateModel<ToolNodeDelegate>(inputId);
    ASSERT_NE(inDel, nullptr);
    auto inConn = inDel->connector();
    ASSERT_TRUE(inConn);
    auto* inputTool = dynamic_cast<TextInputNode*>(inConn.get());
    ASSERT_NE(inputTool, nullptr);
    const QString kInputText = QStringLiteral("Test output content that should NOT be saved");
    inputTool->setText(kInputText);

    // Create output widget to receive the data
    auto* outDel = model.delegateModel<ToolNodeDelegate>(outputId);
    ASSERT_NE(outDel, nullptr);
    auto outConn = outDel->connector();
    ASSERT_TRUE(outConn);
    auto* outputTool = dynamic_cast<TextOutputNode*>(outConn.get());
    ASSERT_NE(outputTool, nullptr);
    QWidget* widget = outputTool->createConfigurationWidget(nullptr);
    ASSERT_NE(widget, nullptr);

    // Execute the pipeline
    ExecutionEngine engine(&model);
    ASSERT_TRUE(runEngineAndWait(engine));

    // Verify that the output widget received the text (runtime behavior working)
    auto* edit = widget->findChild<QTextEdit*>();
    ASSERT_NE(edit, nullptr);
    EXPECT_EQ(edit->toPlainText(), kInputText);

    // Now save the model to JSON
    QJsonObject savedJson = model.save();

    // Parse the saved nodes
    QJsonArray nodesArray = savedJson.value(QStringLiteral("nodes")).toArray();
    ASSERT_FALSE(nodesArray.isEmpty());

    // Find the TextOutputNode in the saved data
    QJsonObject outputNodeJson;
    bool foundOutputNode = false;
    for (const QJsonValue& nodeVal : nodesArray) {
        QJsonObject nodeObj = nodeVal.toObject();
        QJsonObject internalData = nodeObj.value(QStringLiteral("internal-data")).toObject();
        QString modelName = internalData.value(QStringLiteral("model-name")).toString();
        if (modelName == QStringLiteral("text-output")) {
            outputNodeJson = internalData;
            foundOutputNode = true;
            break;
        }
    }

    ASSERT_TRUE(foundOutputNode) << "TextOutputNode not found in saved JSON";

    // The bug: currently the "text" field contains the output content
    // Expected behavior: "text" field should be empty or not present
    QString savedText = outputNodeJson.value(QStringLiteral("text")).toString();
    
    EXPECT_TRUE(savedText.isEmpty()) 
        << "TextOutputNode should NOT save its content. Found: " 
        << savedText.toStdString();

    delete widget;
}

TEST(TextOutputSaveTest, OutputContentClearedBeforeSave)
{
    ensureApp_TextOutputSave();

    NodeGraphModel model;

    // Create pipeline: TextInput -> TextOutput
    NodeId inputId = model.addNode(QStringLiteral("text-input"));
    NodeId outputId = model.addNode(QStringLiteral("text-output"));

    model.addConnection(ConnectionId{ inputId, 0u, outputId, 0u });

    // Configure input
    auto* inDel = model.delegateModel<ToolNodeDelegate>(inputId);
    auto inConn = inDel->connector();
    auto* inputTool = dynamic_cast<TextInputNode*>(inConn.get());
    const QString kInputText = QStringLiteral("Runtime data");
    inputTool->setText(kInputText);

    // Get output node
    auto* outDel = model.delegateModel<ToolNodeDelegate>(outputId);
    auto outConn = outDel->connector();
    auto* outputTool = dynamic_cast<TextOutputNode*>(outConn.get());
    QWidget* widget = outputTool->createConfigurationWidget(nullptr);

    // Execute
    ExecutionEngine engine(&model);
    ASSERT_TRUE(runEngineAndWait(engine));

    // Verify output received data
    auto* edit = widget->findChild<QTextEdit*>();
    EXPECT_EQ(edit->toPlainText(), kInputText);

    // Clear output manually (simulating what MainWindow::onSaveAs should do)
    outputTool->clearOutput();

    // Verify the widget is now empty
    EXPECT_TRUE(edit->toPlainText().isEmpty()) 
        << "clearOutput() should clear the widget display";

    // Save the model
    QJsonObject savedJson = model.save();

    // Find the TextOutputNode in saved data
    QJsonArray nodesArray = savedJson.value(QStringLiteral("nodes")).toArray();
    QString savedText;
    for (const QJsonValue& nodeVal : nodesArray) {
        QJsonObject nodeObj = nodeVal.toObject();
        QJsonObject internalData = nodeObj.value(QStringLiteral("internal-data")).toObject();
        if (internalData.value(QStringLiteral("model-name")).toString() == QStringLiteral("text-output")) {
            savedText = internalData.value(QStringLiteral("text")).toString();
            break;
        }
    }

    // After clearOutput(), the saved state should be empty
    EXPECT_TRUE(savedText.isEmpty()) 
        << "After clearOutput(), saved state should be empty. Found: " 
        << savedText.toStdString();

    delete widget;
}
