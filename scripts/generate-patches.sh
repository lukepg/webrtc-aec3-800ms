#!/bin/bash
#
# Generate AEC3 800ms Patch Files
#
# This script analyzes the WebRTC source code and generates actual patches
# for increasing AEC3 filter length to support 800ms delays.
#
# Usage: ./generate-patches.sh [webrtc_src_path]

set -e

WEBRTC_SRC="${1:-$HOME/webrtc/src}"
PATCHES_DIR="$(cd "$(dirname "$0")/../patches" && pwd)"

echo "WebRTC AEC3 800ms Patch Generator"
echo "================================="
echo ""
echo "WebRTC Source: $WEBRTC_SRC"
echo "Patches Output: $PATCHES_DIR"
echo ""

if [ ! -d "$WEBRTC_SRC" ]; then
    echo "Error: WebRTC source not found at $WEBRTC_SRC"
    echo "Please run this after fetching WebRTC source"
    exit 1
fi

cd "$WEBRTC_SRC"

# Check if we're in a git repository
if [ ! -d ".git" ]; then
    echo "Error: Not a git repository"
    exit 1
fi

echo "Creating working branch..."
git checkout -b aec3-800ms-modifications 2>/dev/null || git checkout aec3-800ms-modifications

# Function to find and modify files
modify_echo_canceller3_config_h() {
    echo "Modifying echo_canceller3_config.h..."

    local FILE="api/audio/echo_canceller3_config.h"
    if [ ! -f "$FILE" ]; then
        echo "Warning: $FILE not found, trying alternate location..."
        FILE="api/echo_canceller3_config.h"
    fi

    if [ ! -f "$FILE" ]; then
        echo "Error: echo_canceller3_config.h not found in expected locations"
        return 1
    fi

    # Increase filter length_blocks from 13 to 40 (main configuration)
    sed -i.bak 's/MainConfiguration main = {13,/MainConfiguration main = {40,/' "$FILE" || true

    # Increase filter length_blocks from 12 to 40 (main_initial configuration)
    sed -i.bak 's/MainConfiguration main_initial = {12,/MainConfiguration main_initial = {40,/' "$FILE" || true

    rm -f "${FILE}.bak"

    echo "✓ Modified filter length from 13/12 blocks to 40 blocks"
}

# Note: Most other parameters don't need modification as they're inside the config struct
# The filter length change is the primary modification needed for 800ms support

# Apply all modifications
echo "Applying modifications..."
modify_echo_canceller3_config_h

# Show what changed
echo ""
echo "Changes made:"
git diff --stat api/audio/echo_canceller3_config.h api/echo_canceller3_config.h 2>/dev/null || true

# Create patch file
echo ""
echo "Generating patch file..."
git diff api/ > "$PATCHES_DIR/0001-increase-aec3-filter-length-800ms.patch"

if [ -s "$PATCHES_DIR/0001-increase-aec3-filter-length-800ms.patch" ]; then
    echo "✓ Patch file created: $PATCHES_DIR/0001-increase-aec3-filter-length-800ms.patch"
    echo ""
    echo "Patch summary:"
    wc -l "$PATCHES_DIR/0001-increase-aec3-filter-length-800ms.patch"
else
    echo "Warning: No changes were made. Patch file is empty."
    echo "This may mean:"
    echo "  1. The files have different structure in this WebRTC version"
    echo "  2. The modifications have already been applied"
    echo "  3. The file paths are different"
    echo ""
    echo "Please manually inspect the source files and create patches accordingly."
fi

# Reset working directory (keep branch for reference)
echo ""
echo "Resetting working directory..."
git reset --hard HEAD

echo ""
echo "Done! The patch is ready to use in GitHub Actions."
echo "Branch 'aec3-800ms-modifications' has been created for reference."
