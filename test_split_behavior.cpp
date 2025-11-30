#include <QString>
#include <QStringList>
#include <iostream>

int main() {
    QString text = "Content A\n\n## Subsection 1.1\nContent B\n\n## Subsection 1.2\nContent C";
    
    std::cout << "Original text:\n" << text.toStdString() << "\n\n";
    
    // Test split with "\n## "
    QStringList splits = text.split("\n## ", Qt::KeepEmptyParts);
    
    std::cout << "After split by '\\n## ':\n";
    for (int i = 0; i < splits.size(); ++i) {
        std::cout << "[" << i << "] = '" << splits[i].toStdString() << "'\n";
    }
    
    std::cout << "\nIf we rejoin with separator '\\n## ':\n";
    QString rejoined = splits.join("\n## ");
    std::cout << rejoined.toStdString() << "\n";
    
    std::cout << "\n=== ISSUE: First split keeps '\\n\\n', but subsequent splits lose '## ' ===\n";
    
    return 0;
}
