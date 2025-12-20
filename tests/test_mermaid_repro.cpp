#include <gtest/gtest.h>

#include <QApplication>
#include <QColor>
#include <QDebug>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>
#include <QStringList>

#include "MermaidRenderService.h"

static QApplication* ensureAppForMermaid()
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

class TestMermaidRepro : public QObject {
    Q_OBJECT
private slots:
    void testWideDiagramTruncation();
};

void TestMermaidRepro::testWideDiagramTruncation()
{
    ensureAppForMermaid();

    QStringList segments;
    segments << QStringLiteral("graph LR");
    for (int i = 1; i <= 80; ++i) {
        segments << QStringLiteral("N%1[\"Node %1 with a deliberately long label to widen the diagram\"]-->")
                        .arg(i)
                  + QStringLiteral("N%2[\"Node %2 with a deliberately long label to widen the diagram\"]")
                        .arg(i + 1);
    }
    const QString mermaidCode = segments.join(QStringLiteral("; "));

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Temporary directory for render output could not be created");

    const QString outputPath = tempDir.filePath(QStringLiteral("wide_mermaid.png"));

    const auto result = MermaidRenderService::instance().renderMermaid(mermaidCode, outputPath, 1.0);
    QVERIFY2(result.ok, qPrintable(QStringLiteral("Render failed: %1").arg(result.error)));

    QImage image(outputPath);
    QVERIFY2(!image.isNull(), "Rendered image could not be loaded");

    const int renderedWidth = image.width();
    const int renderedHeight = image.height();
    qInfo().noquote() << "Mermaid wide render size:" << renderedWidth << "x" << renderedHeight;

    // Expect a very wide render (thousands of pixels); current bug captures the pre-resize viewport (~640-1024px).
    QVERIFY2(renderedWidth > 8000,
             qPrintable(QStringLiteral("Rendered width too small (likely truncated): %1").arg(renderedWidth)));

    // Ensure content reaches the far right; truncation shows up as blank/white space beyond the old viewport.
    int rightmostInk = -1;
    const auto isInk = [](const QColor& c) {
        return c.alpha() > 0 && (c.red() < 250 || c.green() < 250 || c.blue() < 250);
    };
    for (int y = 0; y < renderedHeight; ++y) {
        for (int x = renderedWidth - 1; x >= 0; --x) {
            if (isInk(image.pixelColor(x, y))) {
                rightmostInk = std::max(rightmostInk, x);
                break;
            }
        }
    }

    QVERIFY2(rightmostInk >= 0, "No visible content detected in rendered image");
    QVERIFY2(rightmostInk > renderedWidth - 500,
             qPrintable(QStringLiteral("Content stops too early; rightmost non-white pixel at %1 of %2")
                            .arg(rightmostInk)
                            .arg(renderedWidth)));
}

TEST(MermaidRenderServiceRepro, WideDiagramTruncation)
{
    ensureAppForMermaid();

    TestMermaidRepro testCase;
    const int qtResult = QTest::qExec(&testCase);

    EXPECT_EQ(qtResult, 0);
}

#include "test_mermaid_repro.moc"
