#include <gtest/gtest.h>

#include <QApplication>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>

#include "TextInputNode.h"
#include "TextInputPropertiesWidget.h"
#include "PromptBuilderNode.h"
#include "PromptBuilderPropertiesWidget.h"

// Ensure a QApplication exists for widget-based property editors used by nodes.
static QApplication* ensureApp()
{
    static QApplication* app = nullptr;
    if (!app) {
        int argc = 0;
        char* argv[] = { nullptr };
        app = new QApplication(argc, argv);
    }
    return app;
}

TEST(TextInputNodeTest, EmitsConfiguredTextViaExecute)
{
    ensureApp();

    TextInputNode node;

    // Simulate user setting text through the properties widget (as done in the UI)
    QWidget* w = node.createConfigurationWidget(nullptr);
    ASSERT_NE(w, nullptr);
    auto* props = dynamic_cast<TextInputPropertiesWidget*>(w);
    ASSERT_NE(props, nullptr);

    const QString kText = QStringLiteral("Hello unit tests");
    props->setText(kText);

    // Process queued signals to propagate widget->node updates
    QApplication::processEvents();

    // Execute and verify output packet
    QFuture<DataPacket> fut = node.Execute({});
    fut.waitForFinished();
    const DataPacket out = fut.result();

    ASSERT_TRUE(out.contains(QString::fromLatin1(TextInputNode::kOutputId)));
    EXPECT_EQ(out.value(QString::fromLatin1(TextInputNode::kOutputId)).toString(), kText);

    delete w; // cleanup widget
}

TEST(PromptBuilderNodeTest, FormatsTemplateWithInput)
{
    ensureApp();

    PromptBuilderNode node;

    // Configure template via the properties widget
    QWidget* w = node.createConfigurationWidget(nullptr);
    ASSERT_NE(w, nullptr);
    auto* props = dynamic_cast<PromptBuilderPropertiesWidget*>(w);
    ASSERT_NE(props, nullptr);

    const QString kTpl = QStringLiteral("Hi {input}! This is {input}.");
    props->setTemplateText(kTpl);
    QApplication::processEvents();

    // Build input packet and execute
    DataPacket in;
    in.insert(QString::fromLatin1(PromptBuilderNode::kInputId), QStringLiteral("Alice"));

    QFuture<DataPacket> fut = node.Execute(in);
    fut.waitForFinished();

    const DataPacket out = fut.result();
    ASSERT_TRUE(out.contains(QString::fromLatin1(PromptBuilderNode::kOutputId)));
    EXPECT_EQ(out.value(QString::fromLatin1(PromptBuilderNode::kOutputId)).toString(),
              QStringLiteral("Hi Alice! This is Alice."));

    delete w;
}
