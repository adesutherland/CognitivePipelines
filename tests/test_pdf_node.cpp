#include <gtest/gtest.h>
#include <QApplication>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include "PdfToImageNode.h"
#include "test_app.h"

// Ensure a QApplication exists for PDF rendering
static void ensureApp()
{
    sharedTestApp();
}

TEST(PdfToImageNodeTest, PinContract)
{
    PdfToImageNode node;
    NodeDescriptor desc = node.getDescriptor();

    const QString inPinId = QString::fromLatin1(PdfToImageNode::kPdfPathPinId);
    ASSERT_TRUE(desc.inputPins.contains(inPinId));
    EXPECT_EQ(desc.inputPins[inPinId].type, QStringLiteral("text")) 
        << "PdfToImageNode input pin type should be standardized to 'text'";

    const QString outPinId = QString::fromLatin1(PdfToImageNode::kImagePathPinId);
    ASSERT_TRUE(desc.outputPins.contains(outPinId));
    EXPECT_EQ(desc.outputPins[outPinId].type, QStringLiteral("text"))
        << "PdfToImageNode output pin type should be standardized to 'text'";
}

TEST(PdfToImageNodeTest, PathHandling)
{
    ensureApp();

    // Create a dummy PDF file
    QTemporaryFile tempPdf;
    tempPdf.setFileTemplate(QDir::tempPath() + "/test_XXXXXX.pdf");
    ASSERT_TRUE(tempPdf.open());
    
    {
        QTextStream out(&tempPdf);
        out << "%PDF-1.1\n"
            << "1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n"
            << "2 0 obj << /Type /Pages /Kids [3 0 R] /Count 1 >> endobj\n"
            << "3 0 obj << /Type /Page /Parent 2 0 R /MediaBox [0 0 100 100] /Resources << >> /Contents 4 0 R >> endobj\n"
            << "4 0 obj << /Length 3 >> stream\nq Q\nendstream endobj\n"
            << "xref\n0 5\n0000000000 65535 f\n0000000009 00000 n\n0000000058 00000 n\n0000000115 00000 n\n0000000223 00000 n\n"
            << "trailer << /Size 5 /Root 1 0 R >>\n"
            << "startxref\n271\n%%EOF";
    }
    QString pdfPath = tempPdf.fileName();
    tempPdf.close();

    PdfToImageNode node;

    // Mimic input from TextInputNode
    DataPacket inputData;
    inputData.insert(QString::fromLatin1(PdfToImageNode::kPdfPathPinId), pdfPath);

    ExecutionToken inToken;
    inToken.data = inputData;
    TokenList inTokens;
    inTokens.push_back(std::move(inToken));

    // Execute the node
    TokenList outTokens = node.execute(inTokens);

    ASSERT_FALSE(outTokens.empty());
    DataPacket output = outTokens.front().data;

    const QString outPinId = QString::fromLatin1(PdfToImageNode::kImagePathPinId);
    ASSERT_TRUE(output.contains(outPinId)) << "Output should contain an image path";
    
    QString imagePath = output.value(outPinId).toString();
    EXPECT_FALSE(imagePath.isEmpty());
    EXPECT_TRUE(QFileInfo::exists(imagePath));
}
