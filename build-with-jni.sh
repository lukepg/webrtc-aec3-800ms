#!/bin/bash
# Build libwebrtc_apms.so with JNI wrapper and WebRTC M120 libraries

set -e

echo "======================================"
echo "Building libwebrtc_apms.so with JNI wrapper"
echo "WebRTC M120 + 800ms AEC3 support"
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
JNI_DIR="$SCRIPT_DIR/jni"

# Check NDK
if [ ! -d "$NDK_ROOT" ]; then
    echo "Error: Android NDK not found at $NDK_ROOT"
    exit 1
fi

# Extract WebRTC libraries if needed
echo "Preparing WebRTC libraries..."
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
echo "  JNI Wrapper: webrtc_apm_jni.cpp"
echo ""

# Find WebRTC static libraries
echo "Finding WebRTC static libraries..."
cd "$ARTIFACTS_DIR"
WEBRTC_LIBS=$(find modules/audio_processing -name "*.a" | tr '\n' ' ')
echo "Found $(echo $WEBRTC_LIBS | wc -w) WebRTC libraries"

# Set up include paths for WebRTC headers
# We need to point to the WebRTC source for headers
WEBRTC_SRC="$HOME/webrtc/src"

if [ ! -d "$WEBRTC_SRC" ]; then
    echo ""
    echo "⚠️  Warning: WebRTC source not found at $WEBRTC_SRC"
    echo "The JNI wrapper needs WebRTC headers to compile."
    echo ""
    echo "To fix this, the WebRTC source needs to be available."
    echo "This was downloaded during the GitHub Actions build."
    echo ""
    echo "Options:"
    echo "1. Run the GitHub Actions build again to populate ~/webrtc/src"
    echo "2. Download WebRTC M120 source manually"
    echo "3. Copy headers from the GitHub Actions runner"
    echo ""
    exit 1
fi

# Compile JNI wrapper
echo ""
echo "Compiling JNI wrapper..."
mkdir -p "$OUTPUT_DIR/$ANDROID_ARCH/obj"

$CXX -c "$JNI_DIR/webrtc_apm_jni.cpp" \
    -o "$OUTPUT_DIR/$ANDROID_ARCH/obj/webrtc_apm_jni.o" \
    -I"$WEBRTC_SRC" \
    -I"$WEBRTC_SRC/third_party/abseil-cpp" \
    -std=c++17 \
    -fPIC \
    -DWEBRTC_POSIX \
    -DWEBRTC_ANDROID \
    -DWEBRTC_LINUX \
    -O2

echo "✓ JNI wrapper compiled"

# Link everything into shared library
echo ""
echo "Linking shared library..."
mkdir -p "$OUTPUT_DIR/$ANDROID_ARCH"

$CXX -shared -o "$OUTPUT_DIR/$ANDROID_ARCH/libwebrtc_apms.so" \
    "$OUTPUT_DIR/$ANDROID_ARCH/obj/webrtc_apm_jni.o" \
    -Wl,--whole-archive \
    $WEBRTC_LIBS \
    -Wl,--no-whole-archive \
    -llog -lm -lc \
    -static-libstdc++ \
    -Wl,-soname,libwebrtc_apms.so \
    -Wl,--gc-sections \
    -Wl,--exclude-libs,ALL

echo "✓ Shared library created"
echo ""

# Verify library
if [ -f "$OUTPUT_DIR/$ANDROID_ARCH/libwebrtc_apms.so" ]; then
    echo "======================================"
    echo "Build SUCCESS!"
    echo "======================================"
    ls -lh "$OUTPUT_DIR/$ANDROID_ARCH/libwebrtc_apms.so"
    echo ""

    # Check for JNI symbols
    echo "Checking JNI symbols..."
    nm "$OUTPUT_DIR/$ANDROID_ARCH/libwebrtc_apms.so" | grep "Java_com_webrtc" | head -5
    echo "... (showing first 5 JNI functions)"
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
