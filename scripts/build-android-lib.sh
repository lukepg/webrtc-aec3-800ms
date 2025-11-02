#!/bin/bash
# Build WebRTC audio processing library for Android with 800ms AEC3 support

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
WEBRTC_ROOT="$HOME/webrtc"
ARCH="${1:-arm64}"  # arm64 or arm

echo "======================================"
echo "Building WebRTC Audio Processing Library"
echo "Architecture: $ARCH"
echo "======================================"

# Validate architecture
if [ "$ARCH" != "arm64" ] && [ "$ARCH" != "arm" ]; then
    echo "Error: Invalid architecture. Must be 'arm64' or 'arm'"
    exit 1
fi

# Set Android architecture name
if [ "$ARCH" == "arm64" ]; then
    ANDROID_ARCH="arm64-v8a"
else
    ANDROID_ARCH="armeabi-v7a"
fi

# Ensure depot_tools is in PATH
export PATH="$HOME/depot_tools:$PATH"

cd "$WEBRTC_ROOT/src"

echo "Generating build configuration for $ARCH..."
gn gen "out/$ARCH" --args="
target_os=\"android\"
target_cpu=\"$ARCH\"
is_debug=false
is_component_build=false
rtc_include_tests=false
rtc_enable_protobuf=false
rtc_build_examples=false
rtc_build_tools=false
use_rtti=true
use_custom_libcxx=false
treat_warnings_as_errors=false
rtc_use_h264=false
rtc_enable_bwe_test_logging=false
"

echo "Building audio processing module..."
# Build the core audio processing library
ninja -C "out/$ARCH" \
    modules/audio_processing \
    common_audio \
    rtc_base \
    api/audio

echo "Build complete!"

# Check if libraries were built
if [ -f "out/$ARCH/obj/modules/audio_processing/libaudio_processing.a" ]; then
    echo "✓ Audio processing library built successfully"
    ls -lh "out/$ARCH/obj/modules/audio_processing/libaudio_processing.a"
else
    echo "✗ Audio processing library not found"
    exit 1
fi

echo ""
echo "======================================"
echo "Build Summary"
echo "======================================"
echo "Architecture: $ANDROID_ARCH"
echo "Output directory: out/$ARCH"
echo ""
echo "Next step: Create JNI wrapper to expose as libwebrtc_apms.so"
echo "======================================"
