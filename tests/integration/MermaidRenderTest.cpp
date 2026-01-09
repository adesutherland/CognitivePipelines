#include <QtTest>
#include <QSignalSpy>
#include <QApplication>
#include <QFileInfo>
#include <QDir>
#include "MermaidNode.h"
#include "IToolConnector.h"

class MermaidRenderTest : public QObject {
    Q_OBJECT

private slots:
    void testSimpleRender() {
        MermaidNode node;
        
        ExecutionToken token;
        token.data.insert(QString::fromLatin1(MermaidNode::kInputCode), QStringLiteral("graph TD; A-->B;"));
        
        TokenList inputs;
        inputs.push_back(token);
        
        QSignalSpy spy(&node, &MermaidNode::finished);
        
        // Execute the node. 
        // Note: MermaidNode::execute is currently synchronous but emits the finished() signal.
        // We use a separate thread or just rely on the synchronous execution for now.
        TokenList outputs = node.execute(inputs);
        
        // Even if synchronous, we check the spy. If it were async, we would wait.
        if (spy.count() == 0) {
            spy.wait(10000); // Wait up to 10 seconds
        }
        
        QCOMPARE(spy.count(), 1);
        
        QCOMPARE(outputs.size(), 1ULL);
        DataPacket out = outputs.front().data;
        
        QVERIFY2(!out.contains(QStringLiteral("__error")), qPrintable(out.value(QStringLiteral("__error")).toString()));
        
        QString imagePath = out.value(QString::fromLatin1(MermaidNode::kOutputImage)).toString();
        QVERIFY(!imagePath.isEmpty());
        
        QFileInfo fileInfo(imagePath);
        QVERIFY2(fileInfo.exists(), qPrintable(QStringLiteral("Output file does not exist: %1").arg(imagePath)));
        QVERIFY2(fileInfo.size() > 0, qPrintable(QStringLiteral("Output file is empty: %1").arg(imagePath)));
    }
};

int runMermaidRenderTest(int argc, char** argv) {
    MermaidRenderTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "MermaidRenderTest.moc"
