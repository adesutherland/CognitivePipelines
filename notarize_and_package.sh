#!/bin/bash
set -e  # Exit immediately if any command fails

# ==============================================================================
# 🛠️ CONFIGURATION
# ==============================================================================
APP_NAME="CognitivePipelines"
BUILD_DIR="cmake-build-release"                 # The folder where your CMake build output lives
APP_PATH="${BUILD_DIR}/bin/${APP_NAME}.app"
ZIP_PATH="${BUILD_DIR}/${APP_NAME}.zip"
DMG_PATH="${BUILD_DIR}/${APP_NAME}.dmg"

# 🔑 NOTARIZATION CREDENTIALS
# The name you used with: xcrun notarytool store-credentials "AC_PASSWORD" ...
KEYCHAIN_PROFILE="CognitivePipelinesCredentials"

# ==============================================================================
# 0. PRE-FLIGHT CHECKS
# ==============================================================================
echo "🚀 Starting Packaging Pipeline for ${APP_NAME}..."

if [ ! -d "${APP_PATH}" ]; then
    echo "❌ Error: App bundle not found at ${APP_PATH}"
    echo "   Please run your CMake build first (e.g., 'cmake --build ${BUILD_DIR} --target macos_deploy_all')"
    exit 1
fi

# ==============================================================================
# 1. VERIFY HARDENED RUNTIME
# ==============================================================================
echo "🔍 Verifying Code Signature & Hardened Runtime..."

# Check if the 'runtime' flag (0x10000) is present in the code signature
if codesign -dvvv --entitlements - "${APP_PATH}" 2>&1 | grep -q "flags=0x10000(runtime)"; then
    echo "✅ Hardened Runtime is ENABLED."
else
    echo "❌ Error: Hardened Runtime is MISSING."
    echo "   Ensure your entitlements file does NOT include 'com.apple.security.app-sandbox'."
    echo "   Ensure CMake is signing with the '--options runtime' flag."
    exit 1
fi

# Check if the signature is a Developer ID (required for notarization)
if codesign -dvvv "${APP_PATH}" 2>&1 | grep -q "Authority=Developer ID Application"; then
    echo "✅ Signed with Developer ID Application."
else
    echo "❌ Error: App is NOT signed with a Developer ID Application certificate."
    echo "   Notarization requires a Developer ID Application certificate."
    echo "   Current signature details:"
    codesign -dvvv "${APP_PATH}" 2>&1 | grep "Authority" || echo "   (Ad-Hoc or unsigned)"
    echo "   Check your MACOS_SIGNING_IDENTITY in CMake or environment."
    exit 1
fi

# ==============================================================================
# 2. ZIP FOR NOTARIZATION
# ==============================================================================
echo "📦 Creating ZIP archive for Apple Notary Service..."
# We use ditto because it preserves resource forks/symlinks better than zip
/usr/bin/ditto -c -k --keepParent "${APP_PATH}" "${ZIP_PATH}"

# ==============================================================================
# 3. SUBMIT TO APPLE (NOTARIZE)
# ==============================================================================
echo "📤 Uploading to Apple for Notarization..."
echo "   (This usually takes 1-5 minutes. Please wait...)"

# Submits and waits for the result automatically
# We capture the output to check the status
submission_out=$(xcrun notarytool submit "${ZIP_PATH}" \
    --keychain-profile "${KEYCHAIN_PROFILE}" \
    --wait 2>&1)

echo "${submission_out}"

if echo "${submission_out}" | grep -q "status: Accepted"; then
    echo "✅ Notarization Approved!"
else
    echo "❌ Error: Notarization Failed."
    submission_id=$(echo "${submission_out}" | grep "id:" | awk '{print $2}')
    if [ -n "${submission_id}" ]; then
        echo "🔍 Fetching log for submission ID: ${submission_id}..."
        xcrun notarytool log "${submission_id}" --keychain-profile "${KEYCHAIN_PROFILE}"
    fi
    exit 1
fi

# ==============================================================================
# 4. STAPLE TICKET
# ==============================================================================
echo "📎 Stapling Notarization Ticket to App..."
# This allows the app to run offline immediately after download
xcrun stapler staple "${APP_PATH}"

# ==============================================================================
# 5. CREATE DMG (The Final Artifact)
# ==============================================================================
echo "💿 Creating Installer DMG..."

# 5a. Prepare a temporary staging folder for the DMG contents
STAGING_DIR="${BUILD_DIR}/dmg_staging"
rm -rf "${STAGING_DIR}"
mkdir -p "${STAGING_DIR}"

# 5b. Copy the Stapled App into staging
cp -R "${APP_PATH}" "${STAGING_DIR}/"

# 5c. Create a symlink to /Applications so users can drag-and-drop
ln -s /Applications "${STAGING_DIR}/Applications"

# 5d. Build the DMG using hdiutil
# -srcfolder: The source content
# -volname: The name displayed when mounted
# -ov: Overwrite existing file
# -format UDZO: Compressed image format
rm -f "${DMG_PATH}" # Remove old DMG if exists

hdiutil create \
    -volname "${APP_NAME}" \
    -srcfolder "${STAGING_DIR}" \
    -ov -format UDZO \
    "${DMG_PATH}"

# Cleanup staging
rm -rf "${STAGING_DIR}"
rm -f "${ZIP_PATH}"

echo ""
echo "🎉 RELEASE COMPLETE!"
echo "---------------------------------------------------"
echo "✅ App: ${APP_PATH}"
echo "✅ DMG: ${DMG_PATH}"
echo "---------------------------------------------------"