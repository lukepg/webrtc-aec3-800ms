# Integration Guide: WebRTC AEC3 800ms into Android Project

This guide shows how to integrate the custom-built `libwebrtc_apms.so` libraries with 800ms delay support into your Android project.

## Prerequisites

- Android Studio Arctic Fox or later
- Android Gradle Plugin 7.0+
- Min SDK 21 (Android 5.0)
- Target SDK 33+

## Step 1: Download Pre-built Libraries

### Option A: From GitHub Releases (Recommended)

```bash
# Download latest release
cd ~/Downloads
curl -L https://github.com/YOUR_USERNAME/webrtc-aec3-800ms-build/releases/latest/download/libwebrtc_apms-800ms.tar.gz | tar xz
```

### Option B: From GitHub Actions Artifacts

1. Go to [Actions tab](https://github.com/YOUR_USERNAME/webrtc-aec3-800ms-build/actions)
2. Click on the latest successful workflow run
3. Download artifacts: `libwebrtc_apms-arm64-v8a` and `libwebrtc_apms-armeabi-v7a`

## Step 2: Copy Libraries to Your Project

```bash
# Navigate to your Android project
cd /path/to/your/android/project

# Create jniLibs directories if they don't exist
mkdir -p app/src/main/jniLibs/arm64-v8a
mkdir -p app/src/main/jniLibs/armeabi-v7a

# Copy the libraries
cp ~/Downloads/jniLibs/arm64-v8a/libwebrtc_apms.so app/src/main/jniLibs/arm64-v8a/
cp ~/Downloads/jniLibs/armeabi-v7a/libwebrtc_apms.so app/src/main/jniLibs/armeabi-v7a/

# Verify
ls -lh app/src/main/jniLibs/*/libwebrtc_apms.so
```

Expected output:
```
-rw-r--r-- 1 user user 1.3M ... app/src/main/jniLibs/arm64-v8a/libwebrtc_apms.so
-rw-r--r-- 1 user user 902K ... app/src/main/jniLibs/armeabi-v7a/libwebrtc_apms.so
```

## Step 3: Add Java JNI Wrapper

### Option A: If you already have Apm.java

If you already have `com/webrtc/audioprocessing/Apm.java` in your project (from the original mail2chromium implementation), **no changes needed**. The new `.so` files are binary-compatible.

### Option B: If you don't have Apm.java

Create `app/src/main/java/com/webrtc/audioprocessing/Apm.java`:

```java
package com.webrtc.audioprocessing;

/**
 * WebRTC Audio Processing Module (APM) JNI Wrapper
 *
 * Modified for 800ms delay support
 * Compatible with libwebrtc_apms.so built from WebRTC with extended AEC3 filters
 */
public class Apm {

    static {
        System.loadLibrary("webrtc_apms");
    }

    private long nativeApmInstance = 0;

    /**
     * Create APM instance with configuration
     *
     * @param aecExtendFilter Enable extended filter (REQUIRED for 800ms support)
     * @param speechIntelligibilityEnhance Enable speech intelligibility enhancement
     * @param delayAgnostic Enable delay-agnostic mode (automatic delay estimation)
     * @param beamforming Enable beamforming
     * @param nextGenerationAec Enable AEC3 (vs legacy AEC)
     * @param experimentalNs Enable experimental noise suppression
     * @param experimentalAgc Enable experimental automatic gain control
     */
    public Apm(boolean aecExtendFilter,
               boolean speechIntelligibilityEnhance,
               boolean delayAgnostic,
               boolean beamforming,
               boolean nextGenerationAec,
               boolean experimentalNs,
               boolean experimentalAgc) {

        nativeApmInstance = nativeCreateApmInstance(
            aecExtendFilter,
            speechIntelligibilityEnhance,
            delayAgnostic,
            beamforming,
            nextGenerationAec,
            experimentalNs,
            experimentalAgc
        );
    }

    /**
     * Process microphone input stream (near-end audio)
     * This removes echo from the microphone signal
     *
     * @param nearEnd 16kHz mono audio, 10ms frames (160 samples)
     * @return 0 on success, negative on error
     */
    public native int ProcessStream(short[] nearEnd);

    /**
     * Process speaker reference stream (far-end audio)
     * This is the audio being played through speakers
     *
     * @param farEnd 16kHz mono audio, 10ms frames (160 samples)
     * @return 0 on success, negative on error
     */
    public native int ProcessReverseStream(short[] farEnd);

    /**
     * Set stream delay hint (optional but recommended for 800ms delays)
     * Helps AEC3 converge faster when you know approximate delay
     *
     * @param delay_ms Delay in milliseconds (e.g., 600 for Bluetooth)
     * @return 0 on success, negative on error
     */
    public native int SetStreamDelay(int delay_ms);

    // Legacy AEC methods (for compatibility)
    public native int aec_enable(boolean enable);
    public native int aec_set_suppression_level(int level);
    public native int aecm_enable(boolean enable);
    public native int ns_enable(boolean enable);
    public native int ns_set_level(int level);
    public native int agc_enable(boolean enable);
    public native int agc_set_config(int mode, int targetLevelDbfs, int compressionGainDb);

    // Native instance management
    private native long nativeCreateApmInstance(
        boolean aecExtendFilter,
        boolean speechIntelligibilityEnhance,
        boolean delayAgnostic,
        boolean beamforming,
        boolean nextGenerationAec,
        boolean experimentalNs,
        boolean experimentalAgc
    );

    private native void nativeDestroyApmInstance();

    /**
     * Clean up native resources
     */
    public void dispose() {
        if (nativeApmInstance != 0) {
            nativeDestroyApmInstance();
            nativeApmInstance = 0;
        }
    }

    @Override
    protected void finalize() throws Throwable {
        dispose();
        super.finalize();
    }
}
```

## Step 4: Create Kotlin Wrapper (Recommended)

Create `app/src/main/java/com/yourpackage/audio/WebRtcAec3.kt`:

```kotlin
package com.yourpackage.audio

import com.webrtc.audioprocessing.Apm
import timber.log.Timber

/**
 * WebRTC AEC3 Echo Cancellation with 800ms Delay Support
 *
 * Optimized for Bluetooth speakers with typical delays of 600-800ms
 */
class WebRtcAec3(private val sampleRate: Int) {

    companion object {
        private const val FRAME_SIZE_MS = 10
        private const val REQUIRED_SAMPLE_RATE = 16000
    }

    private var apm: Apm? = null
    private var ready = false

    /**
     * Initialize AEC3 with 800ms delay support
     *
     * @param estimatedDelayMs Optional delay hint (e.g., 600 for Bluetooth)
     * @return true if initialization successful
     */
    fun init(estimatedDelayMs: Int = 0): Boolean {
        if (sampleRate != REQUIRED_SAMPLE_RATE) {
            Timber.e("AEC3 requires 16kHz sample rate, got $sampleRate Hz")
            return false
        }

        try {
            apm = Apm(
                true,   // aecExtendFilter - REQUIRED for 800ms support
                false,  // speechIntelligibilityEnhance
                true,   // delayAgnostic - automatic delay estimation
                false,  // beamforming
                true,   // nextGenerationAec - enables AEC3
                true,   // experimentalNs
                false   // experimentalAgc
            )

            // Set delay hint if provided (helps convergence)
            if (estimatedDelayMs > 0) {
                val result = apm?.SetStreamDelay(estimatedDelayMs) ?: -1
                Timber.d("Set delay hint to ${estimatedDelayMs}ms: result=$result")
            } else {
                Timber.d("No delay hint provided, using automatic estimation")
            }

            ready = true
            Timber.i("WebRTC AEC3 initialized with 800ms support")
            return true

        } catch (e: Exception) {
            Timber.e(e, "Failed to initialize AEC3")
            return false
        }
    }

    /**
     * Process audio frame
     *
     * @param micInput Microphone input (160 samples @ 16kHz)
     * @param speakerReference Speaker output reference (160 samples @ 16kHz)
     * @param output Output buffer for processed audio (echo removed)
     * @return 0 on success, negative on error
     */
    fun processFrame(
        micInput: ShortArray,
        speakerReference: ShortArray,
        output: ShortArray
    ): Int {
        if (!ready || apm == null) {
            Timber.w("AEC3 not initialized")
            return -1
        }

        try {
            // Process speaker reference (far-end)
            val reverseResult = apm?.ProcessReverseStream(speakerReference) ?: -1
            if (reverseResult != 0) {
                Timber.w("ProcessReverseStream failed: $reverseResult")
                return reverseResult
            }

            // Process microphone input (near-end) - echo will be removed
            System.arraycopy(micInput, 0, output, 0, micInput.size)
            val streamResult = apm?.ProcessStream(output) ?: -1
            if (streamResult != 0) {
                Timber.w("ProcessStream failed: $streamResult")
                return streamResult
            }

            return 0

        } catch (e: Exception) {
            Timber.e(e, "Error processing AEC3 frame")
            return -1
        }
    }

    /**
     * Update delay hint (call when measured delay changes)
     *
     * @param delayMs New delay in milliseconds
     */
    fun updateDelay(delayMs: Int) {
        if (ready && apm != null) {
            apm?.SetStreamDelay(delayMs)
            Timber.d("Updated delay hint to ${delayMs}ms")
        }
    }

    /**
     * Clean up resources
     */
    fun release() {
        apm?.dispose()
        apm = null
        ready = false
        Timber.d("AEC3 released")
    }
}
```

## Step 5: Use in Your Audio Processing Pipeline

```kotlin
class AudioProcessor(private val sampleRate: Int) {

    private val aec3 = WebRtcAec3(sampleRate = 16000)
    private var measuredDelay = 0

    fun start() {
        // Initialize AEC3
        if (!aec3.init(estimatedDelayMs = 600)) {
            Log.e("Audio", "Failed to initialize AEC3")
            return
        }

        // Optional: Measure actual delay
        measureDelayAsync()

        // Start audio processing
        startAudioCapture()
    }

    private fun measureDelayAsync() {
        // Use EchoDelayCalibrator or similar
        lifecycleScope.launch(Dispatchers.IO) {
            val calibrator = EchoDelayCalibrator(sampleRate = 16000)
            calibrator.startCalibration()
            // ... play calibration signal
            measuredDelay = calibrator.getDetectedDelay()

            // Update AEC3 with measured delay
            aec3.updateDelay(measuredDelay)
            Log.d("Audio", "Measured delay: ${measuredDelay}ms, updated AEC3")
        }
    }

    fun processAudioFrame(micData: ShortArray, speakerData: ShortArray): ShortArray {
        val output = ShortArray(micData.size)

        // Process with AEC3
        val result = aec3.processFrame(micData, speakerData, output)

        if (result != 0) {
            Log.w("Audio", "AEC3 processing failed, using original")
            return micData  // Fallback to unprocessed
        }

        return output  // Echo-cancelled audio
    }

    fun stop() {
        aec3.release()
    }
}
```

## Step 6: Update build.gradle.kts (Optional)

If you had the unused Threema dependency, remove it:

```kotlin
dependencies {
    // Remove this line (if present):
    // implementation("ch.threema:webrtc-android:134.0.0")

    // Your other dependencies...
}
```

This saves ~12 MB in APK size!

## Step 7: Test

### Basic Functionality Test

```kotlin
@Test
fun testAec3Initialization() {
    val aec3 = WebRtcAec3(sampleRate = 16000)
    assertTrue(aec3.init())

    val micInput = ShortArray(160) { 0 }
    val speakerRef = ShortArray(160) { 0 }
    val output = ShortArray(160)

    val result = aec3.processFrame(micInput, speakerRef, output)
    assertEquals(0, result)

    aec3.release()
}
```

### Real-World Bluetooth Test

1. Connect Bluetooth speaker
2. Enable AEC3 in your app
3. Play music through speaker
4. Speak into microphone
5. Verify echo is suppressed in output

### Performance Test

```kotlin
@Test
fun testAec3Performance() {
    val aec3 = WebRtcAec3(sampleRate = 16000)
    aec3.init(estimatedDelayMs = 600)

    val micInput = ShortArray(160) { (it * 100).toShort() }
    val speakerRef = ShortArray(160) { (it * 50).toShort() }
    val output = ShortArray(160)

    // Measure processing time
    val iterations = 1000
    val startTime = System.nanoTime()

    repeat(iterations) {
        aec3.processFrame(micInput, speakerRef, output)
    }

    val endTime = System.nanoTime()
    val avgTimeMs = (endTime - startTime) / iterations / 1_000_000.0

    Log.d("Perf", "Average processing time: $avgTimeMs ms per frame")
    assertTrue(avgTimeMs < 1.0)  // Should be < 1ms per 10ms frame

    aec3.release()
}
```

## Troubleshooting

### Library Not Found

**Error:** `UnsatisfiedLinkError: dalvik.system.PathClassLoader couldn't find "libwebrtc_apms.so"`

**Solution:**
```bash
# Verify files are in correct location
ls -R app/src/main/jniLibs/

# Should show:
# app/src/main/jniLibs/arm64-v8a/libwebrtc_apms.so
# app/src/main/jniLibs/armeabi-v7a/libwebrtc_apms.so

# Clean and rebuild
./gradlew clean assembleDebug
```

### ABI Mismatch

**Error:** Library built for wrong architecture

**Solution:**
```kotlin
// In build.gradle.kts
android {
    defaultConfig {
        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a")
        }
    }
}
```

### Poor Echo Cancellation

**Possible causes:**
1. Delay > 800ms (use delay calibrator to verify)
2. Sample rate not 16kHz
3. Delay hint not provided
4. Convergence not complete (wait 10-15 seconds)

**Solutions:**
```kotlin
// Measure delay
val calibrator = EchoDelayCalibrator(16000)
val delay = calibrator.getDetectedDelay()
Log.d("AEC", "Measured delay: $delay ms")

// Provide delay hint
aec3.init(estimatedDelayMs = delay)

// Wait for convergence
delay(15000)  // 15 seconds
```

### High CPU Usage

**If CPU usage is too high:**

```kotlin
// Option 1: Reduce sample rate (if possible)
// Option 2: Use adaptive quality
val deviceCores = Runtime.getRuntime().availableProcessors()
val useLowPower = deviceCores < 4

if (useLowPower) {
    // Fall back to simpler echo cancellation
    useGainReductionAec()
} else {
    // Use full AEC3
    aec3.init()
}
```

## Performance Expectations

| Device Class | CPU Usage | Echo Suppression | Convergence Time |
|--------------|-----------|------------------|------------------|
| Flagship (2023+) | 5-8% | >25 dB | 10 seconds |
| Mid-range (2021+) | 10-15% | >20 dB | 12 seconds |
| Low-end (2019+) | 15-20% | >15 dB | 15 seconds |

## Next Steps

- Monitor crash reports for `UnsatisfiedLinkError`
- Test on various Bluetooth devices
- Measure actual delay for different device types
- Consider adding user-adjustable quality settings
- Profile CPU usage on target devices

## Support

- **Build Issues:** [Create issue](https://github.com/YOUR_USERNAME/webrtc-aec3-800ms-build/issues)
- **Integration Help:** See [implementation plan](WEBRTC_AEC3_800MS_IMPLEMENTATION_PLAN.md)
- **WebRTC Questions:** [WebRTC discussion group](https://groups.google.com/g/discuss-webrtc)
