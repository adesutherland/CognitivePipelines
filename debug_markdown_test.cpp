#include <QCoreApplication>
#include <QDebug>
#include "src/core/TextChunker.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    QString text = "# Section 1\n"
                   "Content A with some text that describes the first section.\n\n"
                   "## Subsection 1.1\n"
                   "Content B with detailed information about subsection 1.1.\n\n"
                   "## Subsection 1.2\n"
                   "Content C with more details.\n\n"
                   "# Section 2\n"
                   "Content D for the second major section.";
    
    int chunkSize = 100;
    int chunkOverlap = 10;
    
    qDebug() << "Input text length:" << text.length();
    qDebug() << "Chunk size:" << chunkSize;
    qDebug() << "Chunk overlap:" << chunkOverlap;
    qDebug() << "\n=== INPUT TEXT ===";
    qDebug().noquote() << text;
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap, FileType::CodeMarkdown);
    
    qDebug() << "\n=== CHUNKS ===";
    qDebug() << "Total chunks:" << chunks.size();
    
    for (int i = 0; i < chunks.size(); ++i) {
        qDebug() << "\n[CHUNK" << i << "] Length:" << chunks[i].length();
        qDebug().noquote() << "---START---";
        qDebug().noquote() << chunks[i];
        qDebug().noquote() << "---END---";
        
        // Check what's in this chunk
        bool hasSubsection11 = chunks[i].contains("## Subsection 1.1");
        bool hasContentB = chunks[i].contains("Content B");
        
        if (hasSubsection11 || hasContentB) {
            qDebug() << "  Contains '## Subsection 1.1':" << hasSubsection11;
            qDebug() << "  Contains 'Content B':" << hasContentB;
            qDebug() << "  BOTH TOGETHER:" << (hasSubsection11 && hasContentB);
        }
    }
    
    return 0;
}
