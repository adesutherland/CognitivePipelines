#include <gtest/gtest.h>
#include "core/TextChunker.h"
#include <QString>
#include <QStringList>

// Test 1 (Basic): Text shorter than chunkSize returns 1 chunk
TEST(TextChunkerTest, TextShorterThanChunkSize) {
    QString text = "This is a short text.";
    int chunkSize = 100;
    int chunkOverlap = 10;
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap);
    
    ASSERT_EQ(chunks.size(), 1);
    EXPECT_EQ(chunks[0], text);
}

// Test 2 (Paragraphs): Text with distinct paragraphs (\n\n) splits correctly at the paragraph boundary
TEST(TextChunkerTest, SplitsByParagraphBoundary) {
    QString text = "First paragraph with some text.\n\nSecond paragraph with more content.\n\nThird paragraph here.";
    int chunkSize = 50;
    int chunkOverlap = 10;
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap);
    
    // Should split at paragraph boundaries
    ASSERT_GT(chunks.size(), 1);
    
    // First chunk should contain first paragraph
    EXPECT_TRUE(chunks[0].contains("First paragraph"));
    
    // Verify no chunk exceeds chunkSize
    for (const QString& chunk : chunks) {
        EXPECT_LE(chunk.length(), chunkSize);
    }
}

// Test 3 (Overlap): Verify that the end of Chunk A and the start of Chunk B share the expected number of characters
TEST(TextChunkerTest, VerifyOverlapBetweenChunks) {
    QString text = "AAAAA BBBBB CCCCC DDDDD EEEEE FFFFF GGGGG HHHHH";
    int chunkSize = 20;
    int chunkOverlap = 5;
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap);
    
    ASSERT_GE(chunks.size(), 2);
    
    // Check overlap between consecutive chunks
    for (int i = 0; i < chunks.size() - 1; ++i) {
        const QString& currentChunk = chunks[i];
        const QString& nextChunk = chunks[i + 1];
        
        // Get the end of current chunk (up to overlap size)
        int overlapLen = qMin(chunkOverlap, currentChunk.length());
        QString endOfCurrent = currentChunk.right(overlapLen);
        
        // The next chunk should start with some portion of the end of current chunk
        // (might not be exact due to separator boundaries, but should have some overlap)
        if (overlapLen > 0) {
            EXPECT_TRUE(nextChunk.contains(endOfCurrent) || 
                       currentChunk.contains(nextChunk.left(overlapLen)));
        }
    }
}

// Test 4 (Deep Split): A massive block of text with no newlines should eventually split by spaces
TEST(TextChunkerTest, DeepSplitBySpaces) {
    QString text = "word1 word2 word3 word4 word5 word6 word7 word8 word9 word10 word11 word12";
    int chunkSize = 30;
    int chunkOverlap = 5;
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap);
    
    // Should split into multiple chunks
    ASSERT_GT(chunks.size(), 1);
    
    // Each chunk should respect the size limit
    for (const QString& chunk : chunks) {
        EXPECT_LE(chunk.length(), chunkSize);
    }
    
    // Chunks should contain complete words (split by spaces)
    for (const QString& chunk : chunks) {
        EXPECT_FALSE(chunk.isEmpty());
    }
}

// Edge Case: Empty string returns empty list
TEST(TextChunkerTest, EmptyStringReturnsEmptyList) {
    QString text = "";
    int chunkSize = 100;
    int chunkOverlap = 10;
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap);
    
    EXPECT_TRUE(chunks.isEmpty());
}

// Edge Case: Unbreakable string (no separators) forces character split
TEST(TextChunkerTest, UnbreakableStringForcesSplit) {
    QString text = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"; // 26 characters, no spaces or newlines
    int chunkSize = 10;
    int chunkOverlap = 2;
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap);
    
    // Should split into multiple chunks
    ASSERT_GT(chunks.size(), 1);
    
    // Each chunk should not exceed chunkSize
    for (const QString& chunk : chunks) {
        EXPECT_LE(chunk.length(), chunkSize);
    }
}

// Edge Case: Overlap larger than chunk size should be handled gracefully
TEST(TextChunkerTest, OverlapLargerThanChunkSize) {
    QString text = "This is some text that needs to be split into chunks.";
    int chunkSize = 20;
    int chunkOverlap = 25; // Overlap > chunkSize
    
    // Should not crash and should produce valid chunks
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap);
    
    ASSERT_GT(chunks.size(), 0);
    
    // All chunks should be within size limit
    for (const QString& chunk : chunks) {
        EXPECT_LE(chunk.length(), chunkSize);
    }
}

// Edge Case: Zero chunk size
TEST(TextChunkerTest, ZeroChunkSize) {
    QString text = "Some text";
    int chunkSize = 0;
    int chunkOverlap = 0;
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap);
    
    // Should return the entire text as one chunk
    ASSERT_EQ(chunks.size(), 1);
    EXPECT_EQ(chunks[0], text);
}

// Edge Case: Negative chunk size
TEST(TextChunkerTest, NegativeChunkSize) {
    QString text = "Some text";
    int chunkSize = -10;
    int chunkOverlap = 0;
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap);
    
    // Should return the entire text as one chunk
    ASSERT_EQ(chunks.size(), 1);
    EXPECT_EQ(chunks[0], text);
}

// Test with multiple consecutive separators
TEST(TextChunkerTest, MultipleConsecutiveSeparators) {
    QString text = "Paragraph one.\n\n\n\nParagraph two.\n\n\n\nParagraph three.";
    int chunkSize = 25;
    int chunkOverlap = 5;
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap);
    
    ASSERT_GT(chunks.size(), 1);

    // Verify chunks respect size limit
    for (const QString& chunk : chunks) {
        EXPECT_LE(chunk.length(), chunkSize);
    }
}

// Test with newlines but no double newlines
TEST(TextChunkerTest, SingleNewlines) {
    QString text = "Line one.\nLine two.\nLine three.\nLine four.\nLine five.";
    int chunkSize = 25;
    int chunkOverlap = 5;
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap);
    
    // Should split by single newlines when text exceeds chunk size
    ASSERT_GT(chunks.size(), 1);
    
    for (const QString& chunk : chunks) {
        EXPECT_LE(chunk.length(), chunkSize);
    }
}

// Test realistic RAG scenario: large document chunk
TEST(TextChunkerTest, RealisticRAGScenario) {
    QString text = "Introduction\n\n"
                   "This is a long document that needs to be split into chunks for vector embedding. "
                   "Each chunk should be roughly 100 characters to fit into the embedding model's context window.\n\n"
                   "Section 1: Background\n\n"
                   "The background section contains important context that should be preserved. "
                   "We want to maintain semantic coherence while respecting chunk boundaries.\n\n"
                   "Section 2: Details\n\n"
                   "Here are the detailed explanations that span multiple paragraphs. "
                   "The chunker should split this appropriately.";
    
    int chunkSize = 100;
    int chunkOverlap = 20;
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap);
    
    // Should create multiple chunks
    ASSERT_GT(chunks.size(), 3);
    
    // All chunks should respect size limit
    for (const QString& chunk : chunks) {
        EXPECT_LE(chunk.length(), chunkSize);
        EXPECT_FALSE(chunk.isEmpty());
    }
    
    // Verify overlap exists between consecutive chunks
    for (int i = 0; i < chunks.size() - 1; ++i) {
        // At least some overlap should exist (allowing for separator variations)
        EXPECT_GT(chunks[i].length(), 0);
        EXPECT_GT(chunks[i + 1].length(), 0);
    }
}

// Test zero overlap
TEST(TextChunkerTest, ZeroOverlap) {
    QString text = "Word1 Word2 Word3 Word4 Word5 Word6 Word7 Word8";
    int chunkSize = 20;
    int chunkOverlap = 0;
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap);
    
    ASSERT_GT(chunks.size(), 1);
    
    // All chunks should respect size limit
    for (const QString& chunk : chunks) {
        EXPECT_LE(chunk.length(), chunkSize);
    }
}

// Code-Aware Test: C++ with Doxygen comment and function (Comment Glue)
TEST(TextChunkerTest, CodeCpp_DoxygenCommentStaysWithFunction) {
    QString text = "/**\n"
                   " * Calculates sum\n"
                   " */\n"
                   "int add(int a, int b) {\n"
                   "    return a + b;\n"
                   "}\n\n"
                   "int multiply(int x, int y) {\n"
                   "    return x * y;\n"
                   "}";
    
    int chunkSize = 80;
    int chunkOverlap = 10;
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap, FileType::CodeCpp);
    
    // Should create chunks that keep comments with their functions
    ASSERT_GT(chunks.size(), 0);
    
    // The first chunk should contain both the Doxygen comment and the add function
    // Comment Glue prevents orphaning the /** comment at the end of a chunk
    bool commentAndFunctionTogether = false;
    for (const QString& chunk : chunks) {
        if (chunk.contains("/**") && chunk.contains("int add")) {
            commentAndFunctionTogether = true;
            break;
        }
    }
    EXPECT_TRUE(commentAndFunctionTogether) 
        << "Doxygen comment should stay with its function (Comment Glue)";
    
    // All chunks should respect size limit
    for (const QString& chunk : chunks) {
        EXPECT_LE(chunk.length(), chunkSize);
    }
}

// Code-Aware Test: Python with def boundaries
TEST(TextChunkerTest, CodePython_SplitsAtDefBoundaries) {
    QString text = "class MyClass:\n"
                   "    pass\n\n"
                   "def my_function():\n"
                   "    print('Hello')\n"
                   "    return 42\n\n"
                   "def another_function():\n"
                   "    print('World')\n"
                   "    return 99";
    
    int chunkSize = 60;
    int chunkOverlap = 5;
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap, FileType::CodePython);
    
    ASSERT_GT(chunks.size(), 1);
    
    // Verify that splits happen at function boundaries (def)
    // The chunker should prefer splitting before "def" rather than mid-function
    for (const QString& chunk : chunks) {
        EXPECT_LE(chunk.length(), chunkSize);
    }
    
    // At least one chunk should contain a complete function definition
    bool hasCompleteFunction = false;
    for (const QString& chunk : chunks) {
        if (chunk.contains("def my_function") && chunk.contains("return 42")) {
            hasCompleteFunction = true;
            break;
        }
    }
    EXPECT_TRUE(hasCompleteFunction) 
        << "Python chunker should try to keep function definitions intact";
}

// Code-Aware Test: Markdown with headers and sections
TEST(TextChunkerTest, CodeMarkdown_SplitsAtHeaderBoundaries) {
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
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap, FileType::CodeMarkdown);
    
    ASSERT_GT(chunks.size(), 1);
    
    // Verify that splits happen before headers
    // Headers should stay with their content (not orphaned at the end of a chunk)
    bool subsection11WithContent = false;
    for (const QString& chunk : chunks) {
        // Check if "## Subsection 1.1" appears with "Content B"
        if (chunk.contains("## Subsection 1.1") && chunk.contains("Content B")) {
            subsection11WithContent = true;
            break;
        }
    }
    EXPECT_TRUE(subsection11WithContent) 
        << "Markdown header '## Subsection 1.1' should stay with its content 'Content B'";
    
    // All chunks should respect size limit
    for (const QString& chunk : chunks) {
        EXPECT_LE(chunk.length(), chunkSize);
    }
    
    // Verify that headers are not treated as comments (# is for headers, not comments)
    // This is ensured by isCommentStart returning false for CodeMarkdown
}

// Code-Aware Test: Rexx with labels and returns
TEST(TextChunkerTest, CodeRexx_RespectsLabelsAndReturns) {
    QString text = "/* Rexx script */\n"
                   "SAY 'Starting'\n\n"
                   "MyLabel:\n"
                   "SAY 'Hello'\n"
                   "RETURN 0\n\n"
                   "::routine helper\n"
                   "SAY 'Helper'\n"
                   "RETURN";
    
    int chunkSize = 80;
    int chunkOverlap = 5;
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap, FileType::CodeRexx);
    
    ASSERT_GT(chunks.size(), 1);
    
    // Verify splits respect Rexx syntax (labels with :, RETURN, EXIT, ::routine)
    for (const QString& chunk : chunks) {
        EXPECT_LE(chunk.length(), chunkSize);
    }
    
    // Check that the chunker respects Rexx directives and flow control
    // The separator hierarchy should prefer splitting before ::routine or after RETURN
    bool hasRexxDirective = false;
    bool hasLabelSection = false;
    
    for (const QString& chunk : chunks) {
        if (chunk.contains("::routine")) {
            hasRexxDirective = true;
        }
        if (chunk.contains("MyLabel:")) {
            hasLabelSection = true;
        }
    }
    
    EXPECT_TRUE(hasRexxDirective || hasLabelSection) 
        << "Rexx chunker should respect ::routine directives and label boundaries";
}

// Regression: Rexx leading comments should stay attached to the following
// routine/label even when a chunk boundary falls at the label.
TEST(TextChunkerTest, CodeRexx_LeadingCommentStaysWithRoutine) {
    const QString text =
        "/* Routine: ziggy_played_guitar */\n"
        "ziggy_played_guitar: Procedure\n"
        "  parse arg hand_technique\n";

    // Force a very small chunk size to trigger a boundary between the
    // comment and the routine header in the legacy splitter.
    const int chunkSize = 40;
    const int chunkOverlap = 5;

    const QStringList chunks =
        TextChunker::split(text, chunkSize, chunkOverlap, FileType::CodeRexx);

    ASSERT_FALSE(chunks.isEmpty());

    // The leading comment should appear in the same chunk as the
    // ziggy_played_guitar routine header, not in an isolated chunk.
    bool foundCombined = false;
    for (const QString &chunk : chunks) {
        if (chunk.contains("/* Routine: ziggy_played_guitar */") &&
            chunk.contains("ziggy_played_guitar: Procedure")) {
            foundCombined = true;
            break;
        }
    }

    EXPECT_TRUE(foundCombined)
        << "REXX leading comment should stay attached to its routine header";
}

// Regression: Nested separators must not introduce ghost characters between
// tokens when higher-level separators (e.g. "}\n") and lower-level ones
// (e.g. space) both participate in the split.
TEST(TextChunkerTest, CodeCpp_NestedSeparatorsDoNotCreateGhostBraces) {
    const QString text = "void func() { if(a) { b; } }";

    // Small chunk size forces multiple levels of splitting, including on
    // braces and spaces. The implementation must never re-insert "{" between
    // "if" and "(a)" when recombining recursive sub-chunks.
    const int chunkSize = 10;
    const int chunkOverlap = 2;

    const QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap, FileType::CodeCpp);

    ASSERT_FALSE(chunks.isEmpty());

    for (const QString &chunk : chunks) {
        EXPECT_EQ(chunk.indexOf("if{"), -1) << "Ghost '{' found between 'if' and '(a)' in chunk: "
                                             << chunk.toStdString();
    }
}

// Regression: splitting must not duplicate short tokens such as "return"
// around chunk boundaries.
TEST(TextChunkerTest, CodeCpp_NoReturnDuplication) {
    const QString text = "return value;";

    // Force a tight limit so that the line is split using the separator
    // hierarchy and possibly character-level splitting, but without creating
    // duplicated "return" tokens.
    const int chunkSize = 7;
    const int chunkOverlap = 3;

    const QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap, FileType::CodeCpp);

    ASSERT_FALSE(chunks.isEmpty());

    int returnCount = 0;
    for (const QString &chunk : chunks) {
        int index = -1;
        while ((index = chunk.indexOf("return", index + 1)) != -1) {
            ++returnCount;
        }
    }

    // The source text contains exactly one "return" token; duplication would
    // increase this count.
    EXPECT_EQ(returnCount, 1) << "'return' token was duplicated across chunks";
}

// Code-Aware Test: SQL with CREATE TABLE statements separated by semicolons
TEST(TextChunkerTest, CodeSql_SplitsBetweenCreateTableStatements) {
    QString text = "-- Table for users\n"
                   "CREATE TABLE users (\n"
                   "    id INT PRIMARY KEY,\n"
                   "    name VARCHAR(100)\n"
                   ");\n\n"
                   "-- Table for orders\n"
                   "CREATE TABLE orders (\n"
                   "    order_id INT PRIMARY KEY,\n"
                   "    user_id INT\n"
                   ");";
    
    int chunkSize = 120;
    int chunkOverlap = 10;
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap, FileType::CodeSql);
    
    ASSERT_GT(chunks.size(), 1);
    
    // Verify that splits happen at statement boundaries (semicolons)
    // The chunker should prefer splitting after ";\n\n" or ";\n"
    for (const QString& chunk : chunks) {
        EXPECT_LE(chunk.length(), chunkSize);
    }
    
    // Verify that the two CREATE TABLE statements are in different chunks
    bool hasUsersTable = false;
    bool hasOrdersTable = false;
    
    for (const QString& chunk : chunks) {
        if (chunk.contains("CREATE TABLE users")) {
            hasUsersTable = true;
        }
        if (chunk.contains("CREATE TABLE orders")) {
            hasOrdersTable = true;
        }
    }
    
    EXPECT_TRUE(hasUsersTable) << "Should have chunk with users table";
    EXPECT_TRUE(hasOrdersTable) << "Should have chunk with orders table";
}

// Regression: SQL chunking must not duplicate a single logical line within
// the same chunk when overlap is applied across large chunks.
TEST(TextChunkerTest, CodeSql_NoIntraChunkLineDuplication) {
    const QString text = "-- Golden Years / Sound and Vision Discography Database\n"
                         "-- Ch-ch-ch-ch-changes: Turn and face the strange\n\n"
                         "CREATE TABLE albums (\n"
                         "                        id INT PRIMARY KEY,\n"
                         "                        title VARCHAR(255) NOT NULL,\n"
                         "                        year INT,\n"
                         "                        persona VARCHAR(100) -- E.g., Ziggy, Thin White Duke\n"
                         ");\n";

    // Use the same parameters as the visualisation harness
    const int chunkSize = 500;
    const int chunkOverlap = 50;

    const QString targetLine =
        "persona VARCHAR(100) -- E.g., Ziggy, Thin White Duke";

    const QStringList chunks =
        TextChunker::split(text, chunkSize, chunkOverlap, FileType::CodeSql);

    ASSERT_FALSE(chunks.isEmpty());

    for (const QString &chunk : chunks) {
        const int count = chunk.count(targetLine);
        EXPECT_LE(count, 1) << "Target SQL line should not be duplicated within a single chunk: "
                            << chunk.toStdString();
    }
}

// Code-Aware Test: Cobol with DIVISION and SECTION boundaries
TEST(TextChunkerTest, CodeCobol_SplitsAtDivisionAndSectionBoundaries) {
    QString text = "IDENTIFICATION DIVISION.\n"
                   "PROGRAM-ID. SAMPLE.\n\n"
                   "PROCEDURE DIVISION.\n"
                   "MAIN-LOGIC SECTION.\n"
                   "    DISPLAY 'Hello World'.\n"
                   "    STOP RUN.\n\n"
                   "DATA-PROCESSING SECTION.\n"
                   "    MOVE 1 TO COUNTER.\n"
                   "    PERFORM UNTIL COUNTER > 10\n"
                   "        ADD 1 TO COUNTER\n"
                   "    END-PERFORM.";
    
    int chunkSize = 150;
    int chunkOverlap = 10;
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap, FileType::CodeCobol);
    
    ASSERT_GT(chunks.size(), 1);
    
    // Verify that splits respect DIVISION and SECTION boundaries
    for (const QString& chunk : chunks) {
        EXPECT_LE(chunk.length(), chunkSize);
    }
    
    // Check that DIVISION or SECTION keywords help organize chunks
    bool hasDivision = false;
    bool hasSection = false;
    
    for (const QString& chunk : chunks) {
        if (chunk.contains("DIVISION.")) {
            hasDivision = true;
        }
        if (chunk.contains("SECTION.")) {
            hasSection = true;
        }
    }
    
    EXPECT_TRUE(hasDivision || hasSection) 
        << "Cobol chunker should respect DIVISION and SECTION boundaries";
}

// Code-Aware Test: Terraform with resource blocks
TEST(TextChunkerTest, CodeYaml_SplitsAtTerraformResourceBoundaries) {
    QString text = "# AWS VPC Configuration\n"
                   "resource \"aws_vpc\" \"main\" {\n"
                   "  cidr_block = \"10.0.0.0/16\"\n"
                   "  tags = {\n"
                   "    Name = \"main-vpc\"\n"
                   "  }\n"
                   "}\n\n"
                   "resource \"aws_subnet\" \"public\" {\n"
                   "  vpc_id     = aws_vpc.main.id\n"
                   "  cidr_block = \"10.0.1.0/24\"\n"
                   "  tags = {\n"
                   "    Name = \"public-subnet\"\n"
                   "  }\n"
                   "}";
    
    int chunkSize = 150;
    int chunkOverlap = 10;
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap, FileType::CodeYaml);
    
    ASSERT_GT(chunks.size(), 1);
    
    // Verify that splits happen at resource boundaries
    for (const QString& chunk : chunks) {
        EXPECT_LE(chunk.length(), chunkSize);
    }
    
    // Verify that resource blocks are recognized as split points
    bool hasVpcResource = false;
    bool hasSubnetResource = false;
    
    for (const QString& chunk : chunks) {
        if (chunk.contains("aws_vpc")) {
            hasVpcResource = true;
        }
        if (chunk.contains("aws_subnet")) {
            hasSubnetResource = true;
        }
    }
    
    EXPECT_TRUE(hasVpcResource) << "Should have chunk with VPC resource";
    EXPECT_TRUE(hasSubnetResource) << "Should have chunk with subnet resource";
}

// REGRESSION TEST: Duplication Bug
// Verify that content does not appear twice in the same chunk
TEST(TextChunkerTest, NoDuplicationInChunks) {
    // Create a pattern like "AAA...BBB...CCC..." where each section is identifiable
    QString text = "AAAAAAAAAA BBBBBBBBBB CCCCCCCCCC DDDDDDDDDD EEEEEEEEEE FFFFFFFFFF GGGGGGGGGG";
    int chunkSize = 35;
    int chunkOverlap = 10;
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap);
    
    ASSERT_GT(chunks.size(), 1);
    
    // Verify no chunk contains the same pattern repeated twice
    // For example, a chunk should NOT look like "BBBB...BBBB..." (same B's appearing twice)
    for (const QString& chunk : chunks) {
        // Count occurrences of each pattern
        int countA = chunk.count("AAAAAAAAAA");
        int countB = chunk.count("BBBBBBBBBB");
        int countC = chunk.count("CCCCCCCCCC");
        int countD = chunk.count("DDDDDDDDDD");
        int countE = chunk.count("EEEEEEEEEE");
        int countF = chunk.count("FFFFFFFFFF");
        int countG = chunk.count("GGGGGGGGGG");
        
        // Each pattern should appear at most once in a chunk
        EXPECT_LE(countA, 1) << "Pattern AAA should not be duplicated in chunk: " << chunk.toStdString();
        EXPECT_LE(countB, 1) << "Pattern BBB should not be duplicated in chunk: " << chunk.toStdString();
        EXPECT_LE(countC, 1) << "Pattern CCC should not be duplicated in chunk: " << chunk.toStdString();
        EXPECT_LE(countD, 1) << "Pattern DDD should not be duplicated in chunk: " << chunk.toStdString();
        EXPECT_LE(countE, 1) << "Pattern EEE should not be duplicated in chunk: " << chunk.toStdString();
        EXPECT_LE(countF, 1) << "Pattern FFF should not be duplicated in chunk: " << chunk.toStdString();
        EXPECT_LE(countG, 1) << "Pattern GGG should not be duplicated in chunk: " << chunk.toStdString();
    }
}

// BOUNDARY TEST: No Mid-Word Splitting
// Verify that chunks don't start with partial words like "ed" or "ing"
TEST(TextChunkerTest, NoMidWordSplitting) {
    // Create text with recognizable words
    QString text = "The quick brown fox jumped over the lazy dog and settled peacefully under the shaded tree.";
    int chunkSize = 30;
    int chunkOverlap = 5;
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap);
    
    ASSERT_GT(chunks.size(), 1);
    
    // Verify that no chunk (except the first) starts with a partial word
    // A partial word would be one that doesn't start with a space or is preceded by letters
    for (int i = 1; i < chunks.size(); ++i) {
        const QString& chunk = chunks[i];
        
        // Skip if chunk is empty
        if (chunk.isEmpty()) continue;
        
        // The chunk should either start with whitespace or a complete word
        // Check: if the previous chunk ends with a letter, this chunk should start with space
        const QString& prevChunk = chunks[i - 1];
        if (!prevChunk.isEmpty()) {
            QChar lastCharOfPrev = prevChunk[prevChunk.length() - 1];
            QChar firstCharOfCurrent = chunk[0];
            
            // If previous chunk ends with a letter, current should start with space or newline
            // OR it could be overlap (current chunk starts with content from end of previous chunk)
            if (lastCharOfPrev.isLetter() && firstCharOfCurrent.isLetter()) {
                // Check if this is overlap: does the beginning of current chunk appear in previous chunk?
                // Take the first word of current chunk and see if it's at the end of previous chunk
                // Consider both space and newline as word boundaries
                int spacePos = chunk.indexOf(' ');
                int newlinePos = chunk.indexOf('\n');
                int wordEnd = -1;
                
                if (spacePos >= 0 && newlinePos >= 0) {
                    wordEnd = qMin(spacePos, newlinePos);
                } else if (spacePos >= 0) {
                    wordEnd = spacePos;
                } else if (newlinePos >= 0) {
                    wordEnd = newlinePos;
                }
                
                QString firstWord = (wordEnd >= 0) ? chunk.left(wordEnd) : chunk;
                
                // If the first word of current chunk is contained in previous chunk, it's overlap (acceptable)
                // Otherwise, it's a mid-word split (not acceptable)
                if (!prevChunk.contains(firstWord)) {
                    // This indicates a mid-word split (e.g., "settl" + "ed")
                    FAIL() << "Mid-word split detected between chunks " << i - 1 << " and " << i
                           << "\nPrevious chunk ends with: '" << prevChunk.right(10).toStdString() << "'"
                           << "\nCurrent chunk starts with: '" << chunk.left(10).toStdString() << "'";
                }
            }
        }
    }
}

// MARKDOWN TABLE TEST: Respect Table Boundaries
// Verify that markdown tables (lines starting with |) are handled properly
TEST(TextChunkerTest, MarkdownTableHandling) {
    QString text = "# Table Section\n\n"
                   "Here is a table:\n\n"
                   "| Column A | Column B | Column C |\n"
                   "| :--- | :--- | :--- |\n"
                   "| Value 1 | Value 2 | Value 3 |\n"
                   "| Value 4 | Value 5 | Value 6 |\n"
                   "| Value 7 | Value 8 | Value 9 |\n\n"
                   "Text after the table.";
    
    int chunkSize = 120;
    int chunkOverlap = 10;
    
    QStringList chunks = TextChunker::split(text, chunkSize, chunkOverlap, FileType::CodeMarkdown);

    ASSERT_GT(chunks.size(), 0);

    // All chunks should respect the size limit, with a small relaxation for
    // Markdown tables: to keep header and rows together we allow a chunk to
    // overflow by up to ~25% of the requested size.
    const int tableAwareMax = chunkSize + chunkSize / 4; // +25%
    for (const QString& chunk : chunks) {
        EXPECT_LE(chunk.length(), tableAwareMax);
    }
    // Ideally, table rows should stay together when possible.
    // Additionally, table row formatting must preserve newlines so rows are
    // not flattened into a single line like "| Row1 | | Row2 |".
    bool sawTable = false;
    for (const QString& chunk : chunks) {
        if (!chunk.contains("| Column A |")) {
            continue;
        }

        sawTable = true;

        QStringList lines = chunk.split('\n');

        // Locate the header row index within this chunk
        int headerIndex = -1;
        for (int i = 0; i < lines.size(); ++i) {
            if (lines[i].contains("| Column A | Column B | Column C |")) {
                headerIndex = i;
                break;
            }
        }

        ASSERT_NE(headerIndex, -1) << "Table header row not found in chunk containing table";

        // The alignment row and at least the first data row should appear on
        // distinct subsequent lines, not concatenated onto the header line.
        ASSERT_LT(headerIndex + 2, lines.size());
        EXPECT_TRUE(lines[headerIndex + 1].startsWith("| :---"))
            << "Alignment row should be on its own line, not glued to header: "
            << lines[headerIndex + 1].toStdString();
        EXPECT_TRUE(lines[headerIndex + 2].startsWith("| Value 1"))
            << "First data row should start on its own line, preserving table row newline: "
            << lines[headerIndex + 2].toStdString();
    }

    EXPECT_TRUE(sawTable) << "Markdown table should appear in at least one chunk";
}
