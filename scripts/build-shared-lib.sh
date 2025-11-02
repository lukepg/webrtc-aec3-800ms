#!/bin/bash
# Create shared library from WebRTC static libraries

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WEBRTC_ROOT="$HOME/webrtc"
ARCH="${1:-arm64}"

if [ "$ARCH" == "arm64" ]; then
    ANDROID_ARCH="arm64-v8a"
    TOOLCHAIN_PREFIX="aarch64-linux-android"
else
    ANDROID_ARCH="armeabi-v7a"
    TOOLCHAIN_PREFIX="armv7a-linux-androideabi"
fi

cd "$WEBRTC_ROOT/src"

# Find Android NDK
NDK_ROOT="$HOME/webrtc/src/third_party/android_ndk"
if [ ! -d "$NDK_ROOT" ]; then
    echo "Error: Android NDK not found at $NDK_ROOT"
    exit 1
fi

# Set up toolchain
API_LEVEL=21
TOOLCHAIN="$NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64"
CC="$TOOLCHAIN/bin/${TOOLCHAIN_PREFIX}${API_LEVEL}-clang"
CXX="$TOOLCHAIN/bin/${TOOLCHAIN_PREFIX}${API_LEVEL}-clang++"
AR="$TOOLCHAIN/bin/llvm-ar"

echo "Creating shared library for $ANDROID_ARCH..."
echo "Toolchain: $TOOLCHAIN"

# Create output directory
OUTPUT_DIR="$SCRIPT_DIR/../output/$ANDROID_ARCH"
mkdir -p "$OUTPUT_DIR"

# Collect all required object files
OBJ_DIR="out/$ARCH/obj"

# Link into shared library
$CXX -shared -o "$OUTPUT_DIR/libwebrtc_apms.so" \
    -Wl,--whole-archive \
    "$OBJ_DIR/modules/audio_processing/libaudio_processing.a" \
    "$OBJ_DIR/common_audio/libcommon_audio.a" \
    "$OBJ_DIR/rtc_base/librtc_base.a" \
    -Wl,--no-whole-archive \
    -llog -lm -lc

echo "✓ Shared library created: $OUTPUT_DIR/libwebrtc_apms.so"
ls -lh "$OUTPUT_DIR/libwebrtc_apms.so"
