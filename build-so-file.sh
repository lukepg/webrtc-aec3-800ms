#!/bin/bash
# Build libwebrtc_apms.so from WebRTC M120 static libraries

set -e

echo "======================================"
echo "Building libwebrtc_apms.so with 800ms AEC3"
echo "======================================"

# Configuration
NDK_VERSION="27.0.12077973"
NDK_ROOT="$HOME/Library/Android/sdk/ndk/$NDK_VERSION"
API_LEVEL=21
ARCH="arm64"
ANDROID_ARCH="arm64-v8a"

# Paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARTIFACTS_DIR="$SCRIPT_DIR/artifacts/webrtc-build-$ANDROID_ARCH"
OUTPUT_DIR="$SCRIPT_DIR/output"

# Check NDK
if [ ! -d "$NDK_ROOT" ]; then
    echo "Error: Android NDK not found at $NDK_ROOT"
    echo "Available NDK versions:"
    ls $HOME/Library/Android/sdk/ndk/
    exit 1
fi

# Extract artifacts if needed
echo "Extracting WebRTC libraries..."
cd "$ARTIFACTS_DIR"
if [ ! -d "modules" ]; then
    tar -xzf webrtc-libs.tar.gz
    echo "✓ Libraries extracted"
else
    echo "✓ Libraries already extracted"
fi

# Set up toolchain
TOOLCHAIN="$NDK_ROOT/toolchains/llvm/prebuilt/darwin-x86_64"
CXX="$TOOLCHAIN/bin/aarch64-linux-android${API_LEVEL}-clang++"
AR="$TOOLCHAIN/bin/llvm-ar"

echo ""
echo "Build configuration:"
echo "  NDK: $NDK_VERSION"
echo "  Architecture: $ANDROID_ARCH"
echo "  API Level: $API_LEVEL"
echo "  Toolchain: $(basename $TOOLCHAIN)"
echo ""

# Create output directory
mkdir -p "$OUTPUT_DIR/$ANDROID_ARCH"

# Find all static libraries
echo "Finding static libraries..."
LIBS=$(find modules/audio_processing -name "*.a" | tr '\n' ' ')
echo "Found $(echo $LIBS | wc -w) libraries"

# Link into shared library
echo ""
echo "Linking shared library..."
$CXX -shared -o "$OUTPUT_DIR/$ANDROID_ARCH/libwebrtc_apms.so" \
    -Wl,--whole-archive \
    $LIBS \
    -Wl,--no-whole-archive \
    -llog -lm -lc \
    -static-libstdc++ \
    -Wl,-soname,libwebrtc_apms.so \
    -Wl,--gc-sections \
    -Wl,--exclude-libs,ALL

echo "✓ Shared library created"
echo ""

# Check result
if [ -f "$OUTPUT_DIR/$ANDROID_ARCH/libwebrtc_apms.so" ]; then
    echo "======================================"
    echo "Build SUCCESS!"
    echo "======================================"
    ls -lh "$OUTPUT_DIR/$ANDROID_ARCH/libwebrtc_apms.so"
    echo ""
    echo "Output: $OUTPUT_DIR/$ANDROID_ARCH/libwebrtc_apms.so"
    echo ""
    echo "Next step:"
    echo "  cp $OUTPUT_DIR/$ANDROID_ARCH/libwebrtc_apms.so \\"
    echo "     ../app/src/main/jniLibs/$ANDROID_ARCH/"
    echo ""
else
    echo "✗ Build failed - library not created"
    exit 1
fi
