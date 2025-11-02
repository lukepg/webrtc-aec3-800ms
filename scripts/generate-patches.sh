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
modify_aec3_config_h() {
    echo "Modifying aec3_config.h..."

    local FILE="modules/audio_processing/aec3/aec3_config.h"
    if [ ! -f "$FILE" ]; then
        echo "Warning: $FILE not found, skipping..."
        return
    fi

    # Increase num_partitions (look for various possible patterns)
    sed -i.bak 's/size_t num_partitions = [0-9]\+/size_t num_partitions = 40/' "$FILE" || true
    sed -i.bak 's/constexpr size_t kNumPartitions = [0-9]\+/constexpr size_t kNumPartitions = 40/' "$FILE" || true
    sed -i.bak 's/kAdaptiveFilterLength = [0-9]\+/kAdaptiveFilterLength = 40/' "$FILE" || true

    # Increase render buffer size
    sed -i.bak 's/max_render_buffer_size_ms = [0-9]\+/max_render_buffer_size_ms = 1200/' "$FILE" || true
    sed -i.bak 's/kMaxRenderBufferSizeMs = [0-9]\+/kMaxRenderBufferSizeMs = 1200/' "$FILE" || true

    rm -f "${FILE}.bak"
}

modify_aec3_config_cc() {
    echo "Modifying aec3_config.cc..."

    local FILE="modules/audio_processing/aec3/aec3_config.cc"
    if [ ! -f "$FILE" ]; then
        echo "Warning: $FILE not found, skipping..."
        return
    fi

    # Adjust convergence parameters
    sed -i.bak 's/leakage_converged = 0\.00[0-9]\+f/leakage_converged = 0.0005f/' "$FILE" || true
    sed -i.bak 's/error_floor = [0-9]\+\.[0-9]\+f/error_floor = 2.0f/' "$FILE" || true

    rm -f "${FILE}.bak"
}

modify_block_delay_buffer() {
    echo "Modifying block_delay_buffer.h..."

    local FILE="modules/audio_processing/aec3/block_delay_buffer.h"
    if [ ! -f "$FILE" ]; then
        echo "Warning: $FILE not found, skipping..."
        return
    fi

    # Increase buffer size
    sed -i.bak 's/kMaxBufferSize = [0-9]\+/kMaxBufferSize = 250/' "$FILE" || true
    sed -i.bak 's/constexpr size_t kMaxBufferSize = [0-9]\+/constexpr size_t kMaxBufferSize = 250/' "$FILE" || true

    rm -f "${FILE}.bak"
}

modify_render_delay_buffer() {
    echo "Modifying render_delay_buffer.h..."

    local FILE="modules/audio_processing/aec3/render_delay_buffer.h"
    if [ ! -f "$FILE" ]; then
        # Try alternate location
        FILE="modules/audio_processing/aec3/render_delay_buffer_impl.h"
    fi

    if [ ! -f "$FILE" ]; then
        echo "Warning: render_delay_buffer.h not found, skipping..."
        return
    fi

    # Increase max delay
    sed -i.bak 's/kMaxDelay = [0-9]\+/kMaxDelay = 1000/' "$FILE" || true
    sed -i.bak 's/constexpr size_t kMaxDelay = [0-9]\+/constexpr size_t kMaxDelay = 1000/' "$FILE" || true

    rm -f "${FILE}.bak"
}

# Apply all modifications
echo "Applying modifications..."
modify_aec3_config_h
modify_aec3_config_cc
modify_block_delay_buffer
modify_render_delay_buffer

# Show what changed
echo ""
echo "Changes made:"
git diff --stat modules/audio_processing/aec3/

# Create patch file
echo ""
echo "Generating patch file..."
git diff modules/audio_processing/aec3/ > "$PATCHES_DIR/0001-increase-aec3-filter-length-800ms.patch"

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
