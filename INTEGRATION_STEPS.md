# Quick Integration Steps

Once the build completes successfully, follow these steps to integrate the 800ms library into your app.

## Step 1: Download Build Artifacts

```bash
cd /Users/luke/Dropbox/AIDev/MicrophoneToSpeaker/webrtc-aec3-800ms-build

# Download artifacts from GitHub Actions
gh run download 19021945871 --repo lukepg/webrtc-aec3-800ms -D artifacts/

# Verify downloads
ls -lh artifacts/webrtc-build-*/libwebrtc_apms.so
```

Expected output:
```
artifacts/webrtc-build-arm64-v8a/libwebrtc_apms.so   (1.5-2.0 MB)
artifacts/webrtc-build-armeabi-v7a/libwebrtc_apms.so (1.5-2.0 MB)
```

## Step 2: Backup Current Library

```bash
cd /Users/luke/Dropbox/AIDev/MicrophoneToSpeaker

# Backup existing library (if not already backed up)
cp app/src/main/jniLibs/arm64-v8a/libwebrtc_apms.so \
   app/src/main/jniLibs/arm64-v8a/libwebrtc_apms.so.backup-standard
```

## Step 3: Install New Library

```bash
# Copy 800ms library for arm64-v8a
cp webrtc-aec3-800ms-build/artifacts/webrtc-build-arm64-v8a/libwebrtc_apms.so \
   app/src/main/jniLibs/arm64-v8a/

# Verify
ls -lh app/src/main/jniLibs/arm64-v8a/libwebrtc_apms.so
```

## Step 4: Rebuild App

```bash
# Clean build to ensure library is refreshed
./gradlew clean

# Build debug APK
./gradlew assembleDebug

# Or build and install directly to connected device
./gradlew installDebug
```

## Step 5: Test Basic Functionality

1. **Launch app** on device
2. **Check logcat** for library loading:
   ```bash
   adb logcat | grep -E "(webrtc|AEC3|APM)"
   ```

   Should see:
   - ✅ "WebRTC library loaded successfully"
   - ✅ No UnsatisfiedLinkError
   - ✅ APM instance created

3. **Enable audio passthrough**
   - Should work normally with phone speaker
   - No crashes or errors

## Step 6: Test with Bluetooth Speaker

1. **Connect Bluetooth speaker** (Yamaha YAS-207 or similar)

2. **Enable AEC3 in app settings**
   - Turn on echo cancellation
   - Select "WebRTC AEC3" method

3. **Start audio passthrough**

4. **Monitor performance:**
   ```bash
   adb logcat | grep -E "AEC3|delay|convergence"
   ```

5. **Check for:**
   - ✅ Delay hint applied (400ms for Bluetooth)
   - ✅ Convergence time (should be 3-5 seconds)
   - ✅ Echo reduction quality
   - ✅ No audio artifacts or distortion

## Step 7: Verify 800ms Support

Test with high-delay scenarios:

1. **Measure actual delay:**
   - Use delay calibration feature
   - Check detected delay in logcat
   - Should handle up to 800ms

2. **Compare with old library:**
   - Old library: struggles above ~200ms
   - New library: should handle 600-800ms

3. **Listen for echo reduction:**
   - Speak normally into mic
   - Listen for echo on speaker
   - Should be significantly reduced

## Troubleshooting

### If App Crashes on Launch

```bash
# Check logcat for exact error
adb logcat -d | grep -A 20 "FATAL EXCEPTION"

# Common causes:
# 1. Missing JNI symbols -> Rebuild library with JNI wrapper
# 2. ABI mismatch -> Ensure arm64-v8a matches device architecture
# 3. Library corrupted -> Re-download artifacts
```

**Solution:**
```bash
# Restore backup and report issue
cp app/src/main/jniLibs/arm64-v8a/libwebrtc_apms.so.backup-standard \
   app/src/main/jniLibs/arm64-v8a/libwebrtc_apms.so
```

### If Library Loads But Functions Fail

```bash
# Verify JNI symbols are present
nm -D app/src/main/jniLibs/arm64-v8a/libwebrtc_apms.so | grep "Java_com_webrtc"
```

Should show many symbols like:
```
Java_com_webrtc_audioprocessing_Apm_nativeCreateApmInstance
Java_com_webrtc_audioprocessing_Apm_ProcessStream
Java_com_webrtc_audioprocessing_Apm_ProcessReverseStream
...
```

If missing → Library wasn't built with JNI wrapper

### If Echo Cancellation Doesn't Work

1. **Check AEC3 is actually enabled:**
   ```bash
   adb logcat | grep "echo_cancellation.*enabled"
   ```

2. **Verify delay hint is set:**
   ```bash
   adb logcat | grep "delay hint"
   ```

3. **Check for errors:**
   ```bash
   adb logcat | grep -E "ERROR|WARN" | grep -i "aec\|webrtc"
   ```

4. **Try adjusting delay hint:**
   ```kotlin
   // In code, try different values
   aec3.init(estimatedDelayMs = 600)  // Higher for more delay
   ```

### If CPU Usage Too High

Monitor CPU usage:
```bash
adb shell top | grep "microphonetospeaker"
```

Expected:
- Phone speaker: 5-8% CPU
- Bluetooth with AEC3: 10-15% CPU
- With 800ms filters: 12-18% CPU

If significantly higher:
- Check sample rate (should be 16kHz, not 48kHz)
- Verify only one AEC instance running
- Check for audio feedback loops

## Success Indicators

✅ **Basic Integration:**
- App launches without crashes
- Library loads successfully
- Audio passthrough works

✅ **Echo Cancellation:**
- AEC3 initializes correctly
- Delay hint applied (check logs)
- No UnsatisfiedLinkError

✅ **800ms Support:**
- Handles Bluetooth speaker delays (600ms+)
- Quick convergence (3-5 seconds)
- Good echo reduction quality

✅ **Performance:**
- CPU usage acceptable (10-18%)
- No audio glitches
- Stable during extended use

## Quick Commands Reference

```bash
# Download artifacts
gh run download 19021945871 --repo lukepg/webrtc-aec3-800ms -D artifacts/

# Install library
cp webrtc-aec3-800ms-build/artifacts/webrtc-build-arm64-v8a/libwebrtc_apms.so \
   app/src/main/jniLibs/arm64-v8a/

# Build and install
./gradlew clean installDebug

# Monitor logs
adb logcat | grep -E "(webrtc|AEC3|delay|convergence)"

# Check CPU
adb shell top | grep "microphonetospeaker"

# Restore backup if needed
cp app/src/main/jniLibs/arm64-v8a/libwebrtc_apms.so.backup-standard \
   app/src/main/jniLibs/arm64-v8a/libwebrtc_apms.so && ./gradlew installDebug
```

## Next Steps After Testing

1. **Document results** in WEBRTC_800MS_STATUS.md
2. **Update version** in app/build.gradle.kts
3. **Create changelog** entry
4. **Consider Play Store release** if tests pass
5. **Update app description** to mention 800ms support

## Files Modified

- `app/src/main/jniLibs/arm64-v8a/libwebrtc_apms.so` - New library
- (Optional) `app/src/main/jniLibs/armeabi-v7a/libwebrtc_apms.so` - For 32-bit devices

## Rollback Plan

If you need to revert:

```bash
# Restore standard library
cp app/src/main/jniLibs/arm64-v8a/libwebrtc_apms.so.backup-standard \
   app/src/main/jniLibs/arm64-v8a/libwebrtc_apms.so

# Rebuild
./gradlew clean installDebug
```

---

**Ready to integrate once build completes!**
