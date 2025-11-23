#!/bin/bash

# Query the database and format as markdown table
sqlite3 /Users/adrian/CLionProjects/CognitivePipelines/tests/test_data/rag.db <<'EOF'
.mode markdown
SELECT id, file_path, chunk_index, content FROM fragments LIMIT 5;
EOF
