#!/bin/bash

# debug_release.sh
# Persona: macOS Release Engineer
# Task: Diagnostic investigation of signed/notarized app file-access failures.

APP_PATH="${1:-/Applications/CognitivePipelines.app}"

echo "🔍 Investigating: $APP_PATH"

if [ ! -d "$APP_PATH" ]; then
    echo "❌ Error: App bundle not found at $APP_PATH"
    echo "Usage: ./debug_release.sh [/path/to/CognitivePipelines.app]"
    exit 1
fi

INFOPLIST="$APP_PATH/Contents/Info.plist"
BINARY_PATH="$APP_PATH/Contents/MacOS/CognitivePipelines"

echo ""
echo "--- Plist Inspection ---"
if [ -f "$INFOPLIST" ]; then
    # Use plutil to print the NSDesktopFolderUsageDescription key
    DESKTOP_USAGE=$(plutil -extract NSDesktopFolderUsageDescription xml1 -o - "$INFOPLIST" 2>/dev/null)
    if [ $? -eq 0 ]; then
        VAL=$(plutil -extract NSDesktopFolderUsageDescription raw "$INFOPLIST")
        echo "✅ NSDesktopFolderUsageDescription found: \"$VAL\""
    else
        echo "⚠️ Warning: NSDesktopFolderUsageDescription is MISSING from Info.plist"
    fi
else
    echo "❌ Error: Info.plist not found at $INFOPLIST"
fi

echo ""
echo "--- Entitlement Inspection ---"
if [ -f "$BINARY_PATH" ]; then
    echo "Inspecting entitlements for: $BINARY_PATH"
    codesign -d --entitlements - "$BINARY_PATH"
else
    echo "❌ Error: Binary not found at $BINARY_PATH"
fi

echo ""
echo "--- Framework Audit ---"
FRAMEWORKS_DIR="$APP_PATH/Contents/Frameworks"
PLUGINS_DIR="$APP_PATH/Contents/PlugIns"

echo "Checking Frameworks ($FRAMEWORKS_DIR)..."
if [ -d "$FRAMEWORKS_DIR" ]; then
    PDF_FRAMEWORK=$(find "$FRAMEWORKS_DIR" -name "*Pdf*" -maxdepth 2)
    if [ -n "$PDF_FRAMEWORK" ]; then
        echo "✅ Found PDF Framework(s):"
        echo "$PDF_FRAMEWORK" | sed 's/^/  /'
    else
        echo "❌ QtPdf NOT found in Frameworks"
    fi
else
    echo "⚠️ Frameworks directory missing."
fi

echo "Checking PlugIns ($PLUGINS_DIR)..."
if [ -d "$PLUGINS_DIR" ]; then
    PDF_PLUGIN=$(find "$PLUGINS_DIR" -name "*pdf*" -o -name "*PDF*")
    if [ -n "$PDF_PLUGIN" ]; then
        echo "✅ Found PDF Plugin(s):"
        echo "$PDF_PLUGIN" | sed 's/^/  /'
    else
        echo "❌ PDF plugins NOT found in PlugIns"
    fi

    IMAGE_PLUGIN=$(find "$PLUGINS_DIR" -name "imageformats" -type d)
    if [ -n "$IMAGE_PLUGIN" ]; then
        echo "✅ Found imageformats directory:"
        echo "$IMAGE_PLUGIN" | sed 's/^/  /'
    else
        echo "❌ imageformats NOT found in PlugIns"
    fi
else
    echo "⚠️ PlugIns directory missing."
fi

echo ""
echo "🚀 Launching App in Console Mode..."
echo "Instruct the user: Reproduce the bug (try to open a PDF) and look at the logs above."
echo "Press Ctrl+C to stop."
echo ""

if [ -f "$BINARY_PATH" ]; then
    "$BINARY_PATH"
else
    echo "❌ Cannot launch: Binary missing."
fi
