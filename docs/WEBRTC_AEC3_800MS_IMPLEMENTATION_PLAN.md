# WebRTC AEC3 800ms Delay Support Implementation Plan

**Date:** November 2, 2025
**Author:** SMB Operations / Luke Poortinga
**Contact:** care@smboperations.ca

---

## Executive Summary

This document outlines the plan to rebuild the WebRTC Audio Processing Module (libwebrtc_apms.so) with extended AEC3 filter support to handle 800ms delays for Bluetooth speakers. Current implementation supports ~200ms delays; Bluetooth speakers typically exhibit 600-800ms delays.

---

## Current State Analysis

### Library Source
- **Current Library:** libwebrtc_apms.so (mail2chromium project)
- **Source:** https://github.com/mail2chromium/Android-Audio-Processing-Using-WebRTC
- **Build Guide:** https://github.com/mail2chromium/Compile_WebRTC_Library_For_Android
- **WebRTC Source:** https://webrtc.googlesource.com/src/
- **Version:** Likely M60-M80 (circa 2016-2019)
- **License:** BSD 3-Clause

### Current File Sizes
| Architecture | Current Size |
|-------------|--------------|
| arm64-v8a | 1.3 MB |
| armeabi-v7a | 902 KB |
| **Total** | **2.2 MB** |

### Current Configuration
**File:** `app/src/main/java/com/smboperations/microphonetospeaker/audio/WebRtcAec3.kt`

```kotlin
apm = Apm(
    true,   // aecExtendFilter - extended filter enabled
    false,  // speechIntelligibilityEnhance
    true,   // delayAgnostic - AEC3 automatic delay estimation
    false,  // beamforming
    true,   // nextGenerationAec - enables AEC3
    true,   // experimentalNs
    false   // experimentalAgc
)
```

### Unused Dependency
- **build.gradle.kts line 131:** `ch.threema:webrtc-android:134.0.0` (15 MB AAR, not used)
- **Recommendation:** Remove to save APK size

---

## Goal

Rebuild `libwebrtc_apms.so` with AEC3 parameters modified to support 800ms echo delays for Bluetooth speakers while maintaining the minimal 2-3 MB footprint.

---

## Implementation Plan

### Phase 1: Environment Setup (Days 1-2)

#### 1.1 Linux Build Environment
- **System:** Ubuntu 20.04+ LTS (VM, WSL2, or native)
- **Requirements:**
  - 50GB free disk space
  - 8GB+ RAM
  - Fast internet connection
  - Python 3.8+

#### 1.2 Install Dependencies
```bash
# Update system
sudo apt-get update
sudo apt-get upgrade

# Install dependencies
sudo apt-get install git curl python3 python3-pip \
    build-essential pkg-config libglib2.0-dev \
    libgtk-3-dev ninja-build openjdk-11-jdk

# Install depot_tools (Google's build toolchain)
cd ~
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
echo 'export PATH="$HOME/depot_tools:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

#### 1.3 Install Android NDK
```bash
# Download Android NDK r25c (or latest)
cd ~
wget https://dl.google.com/android/repository/android-ndk-r25c-linux.zip
unzip android-ndk-r25c-linux.zip
export ANDROID_NDK_HOME=~/android-ndk-r25c
echo 'export ANDROID_NDK_HOME=~/android-ndk-r25c' >> ~/.bashrc
```

---

### Phase 2: Fetch WebRTC Source (Days 2-3)

#### 2.1 Create WebRTC Directory
```bash
mkdir ~/webrtc
cd ~/webrtc
```

#### 2.2 Fetch WebRTC for Android
```bash
# This downloads ~10GB of source
fetch --nohooks webrtc_android

cd src

# Sync dependencies (this takes 1-2 hours)
gclient sync
```

#### 2.3 Select Stable Branch
```bash
# List available branches
git branch -r | grep branch-heads

# Checkout stable branch (M120+ recommended for latest AEC3)
git checkout branch-heads/6099  # Example: M120
gclient sync
```

---

### Phase 3: AEC3 Parameter Analysis (Days 3-4)

#### 3.1 Locate AEC3 Configuration Files
```bash
cd ~/webrtc/src/modules/audio_processing/aec3
```

**Key files to analyze:**
- `aec3_config.h` - Configuration constants
- `aec3_config.cc` - Default configuration implementation
- `block_delay_buffer.h` - Delay buffer management
- `render_delay_buffer.h` - Render signal buffering
- `adaptive_fir_filter.h` - Filter implementation

#### 3.2 Current Delay Parameters

**Typical defaults (to be verified in source):**
```cpp
// Current estimated values (verify in actual source)
constexpr size_t kAdaptiveFilterLength = 12;  // 12 partitions
constexpr size_t kBlockSize = 64;             // 64 samples per block
constexpr int kSampleRateHz = 16000;          // 16kHz

// Current tail length calculation:
// Tail = (kAdaptiveFilterLength * kBlockSize) / (kSampleRateHz / 1000)
// Tail = (12 * 64) / 16 = 48ms per partition * 12 = ~576ms theoretical
// But effective handling is typically 30-50% of theoretical
```

#### 3.3 Calculate Required Modifications

**Target: 800ms effective delay handling**

Assuming we need 2x theoretical length for effective handling:
- Target theoretical tail: 1600ms
- Required at 16kHz: 1600ms * 16 samples/ms = 25,600 samples
- Block size: 64 samples
- Required partitions: 25,600 / 64 = 400 partitions

**However**, memory constraints require a balanced approach:
- **Conservative target:** 32-40 partitions (1280-1600ms theoretical, 640-800ms effective)

---

### Phase 4: Source Code Modifications (Days 5-8)

#### 4.1 Modify AEC3 Configuration

**File: `modules/audio_processing/aec3/aec3_config.h`**

Increase filter length:
```cpp
// Find and modify these constants (exact names may vary)
constexpr size_t kAdaptiveFilterLength = 40;  // Increased from 12 to 40
constexpr size_t kMaxRenderBufferSizeMs = 1200;  // Increased from ~400
```

**File: `modules/audio_processing/aec3/render_delay_buffer.h`**

Increase delay buffer capacity:
```cpp
// Increase maximum delay buffer size
static constexpr size_t kMaxDelay = 1000;  // milliseconds
```

#### 4.2 Update Block Delay Buffer

**File: `modules/audio_processing/aec3/block_delay_buffer.h`**

```cpp
// Increase buffer capacity for longer delays
static constexpr size_t kMaxBufferSize = 250;  // blocks (was ~80)
```

#### 4.3 Adjust Convergence Parameters

**File: `modules/audio_processing/aec3/aec3_config.cc`**

```cpp
// Slower convergence for longer filters (more stable)
filter.leakage = 0.0005f;  // Was ~0.001f
filter.error_floor = 2.0f;  // Was ~1.0f
```

#### 4.4 Memory Impact Analysis

**Estimated memory increase:**
- Old filter: 12 partitions * 64 samples * 2 bytes * 2 (real/imag) = ~3 KB
- New filter: 40 partitions * 64 samples * 2 bytes * 2 = ~10 KB
- Additional buffers: ~5 KB
- **Total increase: ~12 KB per AEC instance** (negligible on modern devices)

---

### Phase 5: Build JNI Wrapper (Days 8-9)

#### 5.1 Clone mail2chromium Wrapper
```bash
cd ~/webrtc
git clone https://github.com/mail2chromium/Android-Audio-Processing-Using-WebRTC.git
cd Android-Audio-Processing-Using-WebRTC
```

#### 5.2 Update Wrapper for New WebRTC Version

**File: `app/src/main/jni/android_apm_wrapper.cpp`**

Check for API compatibility:
- Verify APM creation methods
- Check SetStreamDelay signature
- Ensure ProcessStream/ProcessReverseStream match

#### 5.3 Create Android.mk or CMakeLists.txt

**Option A: Android.mk** (if using ndk-build)
```makefile
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := webrtc_apms
LOCAL_SRC_FILES := android_apm_wrapper.cpp
LOCAL_C_INCLUDES := $(HOME)/webrtc/src \
                    $(HOME)/webrtc/src/modules/audio_processing/include
LOCAL_LDLIBS := -llog
LOCAL_STATIC_LIBRARIES := libwebrtc
include $(BUILD_SHARED_LIBRARY)
```

**Option B: Use WebRTC's build system** (Recommended)
```bash
# Add GN build target for APM wrapper
# Edit BUILD.gn in modules/audio_processing/
```

---

### Phase 6: Build for Android (Days 9-12)

#### 6.1 Generate Build Files
```bash
cd ~/webrtc/src

# Generate for arm64-v8a
gn gen out/arm64 --args='
  target_os="android"
  target_cpu="arm64"
  is_debug=false
  is_component_build=false
  rtc_include_tests=false
  rtc_enable_protobuf=false
'

# Generate for armeabi-v7a
gn gen out/arm --args='
  target_os="android"
  target_cpu="arm"
  is_debug=false
  is_component_build=false
  rtc_include_tests=false
  rtc_enable_protobuf=false
'
```

#### 6.2 Build WebRTC APM
```bash
# Build arm64-v8a (takes 30-60 minutes)
ninja -C out/arm64 audio_processing

# Build armeabi-v7a (takes 30-60 minutes)
ninja -C out/arm audio_processing
```

#### 6.3 Build Custom Wrapper

**If using separate wrapper:**
```bash
cd ~/Android-Audio-Processing-Using-WebRTC
ndk-build NDK_PROJECT_PATH=. \
          APP_BUILD_SCRIPT=Android.mk \
          APP_ABI="arm64-v8a armeabi-v7a" \
          WEBRTC_SRC=~/webrtc/src \
          WEBRTC_OUT_ARM64=~/webrtc/src/out/arm64 \
          WEBRTC_OUT_ARM=~/webrtc/src/out/arm
```

#### 6.4 Extract Libraries
```bash
# Libraries will be in:
# libs/arm64-v8a/libwebrtc_apms.so
# libs/armeabi-v7a/libwebrtc_apms.so

# Verify sizes (should be ~2-3 MB)
ls -lh libs/*/libwebrtc_apms.so
```

---

### Phase 7: Integration (Days 13-14)

#### 7.1 Backup Current Libraries
```bash
cd /Users/luke/Dropbox/AIDev/MicrophoneToSpeaker
mkdir -p backup
cp app/src/main/jniLibs/arm64-v8a/libwebrtc_apms.so backup/
cp app/src/main/jniLibs/armeabi-v7a/libwebrtc_apms.so backup/
```

#### 7.2 Replace Libraries
```bash
# Copy new libraries
cp ~/Android-Audio-Processing-Using-WebRTC/libs/arm64-v8a/libwebrtc_apms.so \
   app/src/main/jniLibs/arm64-v8a/

cp ~/Android-Audio-Processing-Using-WebRTC/libs/armeabi-v7a/libwebrtc_apms.so \
   app/src/main/jniLibs/armeabi-v7a/
```

#### 7.3 Remove Unused Threema Dependency

**File: `app/build.gradle.kts`**
```kotlin
// Remove line 131:
// implementation("ch.threema:webrtc-android:134.0.0")
```

#### 7.4 Update Apm.java if Needed

Check if JNI signatures changed. If API is compatible, no changes needed.

---

### Phase 8: Testing (Days 14-17)

#### 8.1 Build and Run
```bash
./gradlew clean assembleDebug
adb install app/build/outputs/apk/debug/app-debug.apk
```

#### 8.2 Basic Functionality Tests
- Verify app launches
- Test AEC3 initialization
- Process audio without crashes
- Check logcat for errors

#### 8.3 Echo Delay Testing

**Use EchoDelayCalibrator:**
```kotlin
// In app code, measure actual delay
val calibrator = EchoDelayCalibrator(sampleRate = 16000)
calibrator.startCalibration()
// ... play calibration signal
val measuredDelay = calibrator.getDetectedDelay()
Log.d("AEC3Test", "Measured delay: ${measuredDelay}ms")
```

#### 8.4 Bluetooth Speaker Testing
- Connect Bluetooth speaker (typical delay: 600-800ms)
- Enable AEC3 in app
- Test with voice input
- Evaluate echo cancellation quality

#### 8.5 Performance Testing
- Monitor CPU usage (should be <10% on modern devices)
- Check memory consumption
- Test on low-end device (e.g., Android 8.0, 2GB RAM)
- Measure battery impact (30-minute continuous use)

#### 8.6 Convergence Testing
- Measure time for AEC3 to adapt (expect 10-15 seconds for 800ms filter)
- Test with varying delay conditions
- Verify stability during movement (changing delay)

---

### Phase 9: Optimization (Days 18-19)

#### 9.1 Performance Issues Mitigation

If CPU usage is too high:
```cpp
// Reduce FFT size or use lower sample rate for filter
// In aec3_config.h:
constexpr size_t kBlockSize = 48;  // Instead of 64
```

If convergence is too slow:
```cpp
// Adjust step size
filter.step_size = 0.002f;  // Increase from default
```

#### 9.2 Add Manual Delay Hint

**File: `WebRtcAec3.kt`**
```kotlin
fun setStreamDelay(delayMs: Int): Boolean {
    if (!ready || apm == null) return false

    try {
        val result = apm?.SetStreamDelay(delayMs) ?: -1
        if (result == 0) {
            Timber.d("Stream delay set to ${delayMs}ms")
            return true
        }
        Timber.w("Failed to set stream delay: $result")
        return false
    } catch (e: Exception) {
        Timber.e(e, "Error setting stream delay")
        return false
    }
}

// Call after measuring delay
fun initWithDelay(estimatedDelayMs: Int): Boolean {
    if (!init()) return false
    return setStreamDelay(estimatedDelayMs)
}
```

#### 9.3 Adaptive Configuration

Add device-specific configuration:
```kotlin
private fun getOptimalFilterConfig(): FilterConfig {
    val cpuCores = Runtime.getRuntime().availableProcessors()
    val totalMem = ActivityManager.getMemoryInfo().totalMem

    return when {
        cpuCores >= 8 && totalMem > 4_000_000_000 -> FilterConfig.HIGH_PERFORMANCE
        cpuCores >= 4 && totalMem > 2_000_000_000 -> FilterConfig.BALANCED
        else -> FilterConfig.LOW_END
    }
}
```

---

### Phase 10: Documentation (Day 20)

#### 10.1 Create Build Documentation

**File: `docs/WEBRTC_BUILD_INSTRUCTIONS.md`**
- Document exact build steps
- List all dependencies with versions
- Include troubleshooting section
- Add rebuild instructions for future updates

#### 10.2 Update Project Documentation

**File: `CLAUDE.md`**
Add section:
```markdown
## WebRTC AEC3 Configuration

This project uses a custom-built WebRTC Audio Processing Module with extended AEC3 filter support:

- **Maximum supported delay:** 800ms (optimized for Bluetooth speakers)
- **Filter partitions:** 40 (increased from standard 12)
- **Convergence time:** 10-15 seconds
- **Source:** https://github.com/mail2chromium/Android-Audio-Processing-Using-WebRTC
- **Build instructions:** See docs/WEBRTC_BUILD_INSTRUCTIONS.md
```

**File: `WebRtcAec3.kt`**
Add comments:
```kotlin
/**
 * WebRTC AEC3 Echo Cancellation with Extended Delay Support
 *
 * This implementation uses a custom-built libwebrtc_apms.so with:
 * - Extended filter length (40 partitions)
 * - Support for up to 800ms delay
 * - Optimized for Bluetooth speaker scenarios (600-800ms typical delay)
 *
 * Convergence time: 10-15 seconds for full adaptation
 * CPU usage: ~5-10% on modern devices
 * Memory footprint: ~10KB per AEC instance
 */
```

#### 10.3 Add Source Attribution

**File: `app/src/main/java/com/webrtc/audioprocessing/Apm.java`**
```java
/**
 * WebRTC Audio Processing Module (APM) JNI Wrapper
 *
 * Original wrapper: https://github.com/mail2chromium/Android-Audio-Processing-Using-WebRTC
 * Created by sino on 2016-03-14
 *
 * Native library compiled from WebRTC source with custom AEC3 configuration:
 * - Source: https://webrtc.googlesource.com/src/
 * - Build guide: https://github.com/mail2chromium/Compile_WebRTC_Library_For_Android
 * - Modified for 800ms delay support (November 2025)
 *
 * License: BSD 3-Clause (WebRTC project license)
 *
 * Custom modifications:
 * - Increased AEC3 filter partitions from 12 to 40
 * - Extended render buffer to support 800ms delays
 * - Optimized for Bluetooth speaker echo cancellation
 */
```

---

## Deliverables Checklist

- [ ] Custom libwebrtc_apms.so for arm64-v8a (~2-3 MB)
- [ ] Custom libwebrtc_apms.so for armeabi-v7a (~2-3 MB)
- [ ] Removed unused Threema dependency (saves ~12 MB)
- [ ] Updated Apm.java with source attribution
- [ ] Enhanced WebRtcAec3.kt with delay configuration
- [ ] Build documentation (WEBRTC_BUILD_INSTRUCTIONS.md)
- [ ] Performance benchmarks
- [ ] Test results with Bluetooth speakers
- [ ] Updated CLAUDE.md
- [ ] Backup of original libraries

---

## Risk Mitigation

### Risk: Build Complexity
**Mitigation:**
- Follow mail2chromium guide precisely
- Use Docker container for consistent environment
- Document each step for reproducibility

### Risk: API Incompatibility
**Mitigation:**
- Test Apm.java wrapper early in build process
- Keep backup of working libraries
- Version lock WebRTC branch for rebuilds

### Risk: Performance Impact
**Mitigation:**
- Profile on low-end devices early
- Implement adaptive configuration
- Add fallback to shorter filter if needed

### Risk: Convergence Time Too Long
**Mitigation:**
- Implement manual delay hint (SetStreamDelay)
- Use EchoDelayCalibrator for quick measurement
- Provide user feedback during convergence

### Risk: Memory Constraints
**Mitigation:**
- Monitor memory usage on 2GB RAM devices
- Implement graceful degradation
- Test with memory stress

---

## Alternative Approaches

### Alternative 1: Use Threema Library Properly
**Pros:**
- Modern codebase (M134)
- Actively maintained
- May already support longer delays

**Cons:**
- 10 MB APK size increase
- Requires code refactoring
- Includes many unused features

**Verdict:** Not recommended due to bloat

### Alternative 2: Hybrid AEC System
**Implementation:**
```kotlin
class HybridAec {
    private val webrtcAec = WebRtcAec3(sampleRate)
    private val gainReductionAec = GainReductionAec(sampleRate)

    fun processFrame(mic: ShortArray, ref: ShortArray, out: ShortArray, delay: Int): Int {
        return if (delay < 400) {
            webrtcAec.processFrame(mic, ref, out)
        } else {
            gainReductionAec.processFrame(mic, ref, out)
        }
    }
}
```

**Pros:**
- Works now with existing code
- No build complexity

**Cons:**
- Doesn't improve AEC3 specifically
- Switching between modes may be jarring

**Verdict:** Good fallback if custom build fails

---

## Success Criteria

1. **Functional:**
   - [ ] App builds and runs without crashes
   - [ ] AEC3 initializes successfully
   - [ ] Audio processing works correctly

2. **Performance:**
   - [ ] Handles 600-800ms Bluetooth delays effectively
   - [ ] Echo suppression >20dB with Bluetooth speakers
   - [ ] CPU usage <10% on mid-range devices
   - [ ] Convergence within 15 seconds

3. **Quality:**
   - [ ] No audio artifacts or distortion
   - [ ] Stable during movement/delay changes
   - [ ] Better than GainReductionAec for music/complex audio

4. **Efficiency:**
   - [ ] APK size increase <1 MB (2.2 MB → 3.0 MB max)
   - [ ] Memory increase <15 KB
   - [ ] Battery impact <2% over baseline

---

## Timeline Summary

| Phase | Duration | Tasks |
|-------|----------|-------|
| 1-2 | 2-3 days | Environment setup, fetch WebRTC source |
| 3 | 1 day | Analyze AEC3 parameters |
| 4 | 3 days | Modify source code |
| 5 | 1 day | Build JNI wrapper |
| 6 | 3 days | Build for Android |
| 7 | 1 day | Integration |
| 8 | 3 days | Testing |
| 9 | 2 days | Optimization |
| 10 | 1 day | Documentation |
| **Total** | **17-20 days** | |

---

## Resources

### Documentation
- WebRTC Official: https://webrtc.org/
- WebRTC Source: https://webrtc.googlesource.com/src/
- Build Guide: https://github.com/mail2chromium/Compile_WebRTC_Library_For_Android
- mail2chromium Wrapper: https://github.com/mail2chromium/Android-Audio-Processing-Using-WebRTC

### Tools
- depot_tools: https://chromium.googlesource.com/chromium/tools/depot_tools.git
- Android NDK: https://developer.android.com/ndk/downloads
- GN Build System: https://gn.googlesource.com/gn/

### References
- AEC3 Paper: https://arxiv.org/abs/1803.10814
- WebRTC APM Module: https://webrtc.googlesource.com/src/+/refs/heads/main/modules/audio_processing/

---

## Conclusion

This implementation plan provides a comprehensive roadmap to rebuild the WebRTC Audio Processing Module with 800ms delay support while maintaining the minimal 2-3 MB footprint. The approach balances performance, quality, and maintainability.

**Estimated Total Effort:** 17-20 days
**Expected APK Size Impact:** +0.5-1.0 MB
**Performance Impact:** +5% CPU, +10KB memory
**Quality Improvement:** Effective echo cancellation for Bluetooth speakers (600-800ms delay)

---

**Next Steps:** Begin Phase 1 - Environment Setup
