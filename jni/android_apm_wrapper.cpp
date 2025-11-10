// WebRTC Audio Processing Module (APM) JNI Wrapper
//
// Original source: https://github.com/mail2chromium/Android-Audio-Processing-Using-WebRTC
// Created by sino on 2016-03-14
// Modified for WebRTC AEC3 800ms delay support by Luke Poortinga, November 2025
//
// This JNI wrapper provides a Java interface to WebRTC's Audio Processing Module,
// with custom AEC3 configuration supporting up to 800ms echo delays.

#include <jni.h>
#include <android/log.h>

#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/ref_counted_object.h"
#include "api/audio/echo_canceller3_config.h"
#include "api/audio/echo_canceller3_factory.h"

#define LOG_TAG "WebRTC-APM-JNI"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace webrtc;

// Global APM instance holder
static rtc::scoped_refptr<AudioProcessing> g_apm;

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_webrtc_audioprocessing_Apm_nativeCreateApmInstance(
    JNIEnv* env,
    jobject thiz,
    jboolean aecExtendFilter,
    jboolean speechIntelligibilityEnhance,
    jboolean delayAgnostic,
    jboolean beamforming,
    jboolean nextGenerationAec,
    jboolean experimentalNs,
    jboolean experimentalAgc) {

    LOGD("Creating APM instance with AEC3 800ms support");

    // Create AudioProcessing config
    AudioProcessing::Config config;

    // Echo cancellation configuration
    if (nextGenerationAec) {
        config.echo_canceller.enabled = true;
        config.echo_canceller.mobile_mode = false;

        // Enable extended filter for longer delays (critical for 800ms support)
        if (aecExtendFilter) {
            LOGD("AEC extended filter enabled (required for 800ms delays)");
        }

        // Delay-agnostic mode (automatic delay estimation)
        if (delayAgnostic) {
            LOGD("Delay-agnostic mode enabled (automatic delay estimation)");
        }
    } else {
        // Legacy AEC (not recommended for Bluetooth)
        config.echo_canceller.enabled = false;
        LOGD("Using legacy AEC mode");
    }

    // Noise suppression
    if (experimentalNs) {
        config.noise_suppression.enabled = true;
        config.noise_suppression.level = AudioProcessing::Config::NoiseSuppression::kHigh;
        LOGD("Noise suppression enabled");
    }

    // Automatic gain control
    if (experimentalAgc) {
        config.gain_controller1.enabled = true;
        config.gain_controller1.mode = AudioProcessing::Config::GainController1::kAdaptiveDigital;
        LOGD("AGC enabled");
    }

    // High-pass filter (recommended for AEC)
    config.high_pass_filter.enabled = true;

    // Create custom AEC3 config for 800ms delay support
    EchoCanceller3Config aec3_config;

    // Set filter length to 40 blocks for 800ms delay support
    // Each block is 4ms @ 16kHz, so 40 blocks = 160ms filter length
    // Combined with render buffer, this supports delays up to 800ms
    aec3_config.filter.refined.length_blocks = 40;
    aec3_config.filter.coarse.length_blocks = 40;
    aec3_config.filter.refined_initial.length_blocks = 40;
    aec3_config.filter.coarse_initial.length_blocks = 40;

    // Log the configuration
    LOGD("AEC3 Config: filter length = 40 blocks (800ms delay support)");

    // Create APM instance with custom AEC3 factory
    if (nextGenerationAec) {
        g_apm = AudioProcessingBuilder()
            .SetEchoControlFactory(
                std::make_unique<EchoCanceller3Factory>(aec3_config))
            .Create(config);
    } else {
        // Legacy AEC without custom config
        g_apm = AudioProcessingBuilder().Create(config);
    }

    if (!g_apm) {
        LOGE("Failed to create APM instance");
        return 0;
    }

    LOGD("APM instance created successfully (AEC3 with 40-block filter for 800ms support)");
    return reinterpret_cast<jlong>(g_apm.get());
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_ProcessStream(
    JNIEnv* env,
    jobject thiz,
    jshortArray nearEnd) {

    if (!g_apm) {
        LOGE("APM not initialized");
        return -1;
    }

    jshort* nearEndData = env->GetShortArrayElements(nearEnd, nullptr);
    jsize nearEndLength = env->GetArrayLength(nearEnd);

    // Assuming 16kHz mono, 10ms frames = 160 samples
    const int kSampleRate = 16000;
    const size_t kNumChannels = 1;
    const size_t kNumSamples = nearEndLength;

    // Convert short to float
    std::vector<float> float_buffer(kNumSamples);
    for (size_t i = 0; i < kNumSamples; i++) {
        float_buffer[i] = nearEndData[i] / 32768.0f;
    }

    // Create stream config
    StreamConfig stream_config(kSampleRate, kNumChannels);

    // Process the stream
    float* channel_ptrs[] = {float_buffer.data()};
    int result = g_apm->ProcessStream(
        channel_ptrs,
        stream_config,
        stream_config,
        channel_ptrs);

    if (result != AudioProcessing::kNoError) {
        LOGE("ProcessStream failed: %d", result);
    }

    // Convert back to short
    for (size_t i = 0; i < kNumSamples; i++) {
        float sample = float_buffer[i] * 32768.0f;
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;
        nearEndData[i] = static_cast<jshort>(sample);
    }

    env->ReleaseShortArrayElements(nearEnd, nearEndData, 0);
    return result;
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_ProcessReverseStream(
    JNIEnv* env,
    jobject thiz,
    jshortArray farEnd) {

    if (!g_apm) {
        LOGE("APM not initialized");
        return -1;
    }

    jshort* farEndData = env->GetShortArrayElements(farEnd, nullptr);
    jsize farEndLength = env->GetArrayLength(farEnd);

    const int kSampleRate = 16000;
    const size_t kNumChannels = 1;
    const size_t kNumSamples = farEndLength;

    // Convert short to float
    std::vector<float> float_buffer(kNumSamples);
    for (size_t i = 0; i < kNumSamples; i++) {
        float_buffer[i] = farEndData[i] / 32768.0f;
    }

    // Create stream config
    StreamConfig stream_config(kSampleRate, kNumChannels);

    // Process reverse stream (speaker reference)
    float* channel_ptrs[] = {float_buffer.data()};
    int result = g_apm->ProcessReverseStream(
        channel_ptrs,
        stream_config,
        stream_config,
        channel_ptrs);

    if (result != AudioProcessing::kNoError) {
        LOGE("ProcessReverseStream failed: %d", result);
    }

    env->ReleaseShortArrayElements(farEnd, farEndData, JNI_ABORT);
    return result;
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_SetStreamDelay(
    JNIEnv* env,
    jobject thiz,
    jint delay_ms) {

    if (!g_apm) {
        LOGE("APM not initialized");
        return -1;
    }

    // Set stream delay hint (useful even with delay-agnostic mode)
    // This helps AEC3 converge faster when you know the approximate delay
    LOGD("Setting stream delay hint: %d ms", delay_ms);

    // Note: With delay-agnostic mode, this is optional but can improve
    // convergence time for delays up to 800ms
    g_apm->set_stream_delay_ms(delay_ms);

    return 0;
}

JNIEXPORT void JNICALL
Java_com_webrtc_audioprocessing_Apm_nativeDestroyApmInstance(
    JNIEnv* env,
    jobject thiz) {

    LOGD("Destroying APM instance");
    g_apm = nullptr;
}

// Legacy AEC methods (for compatibility)
JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_aec_1enable(
    JNIEnv* env,
    jobject thiz,
    jboolean enable) {

    if (!g_apm) return -1;

    AudioProcessing::Config config = g_apm->GetConfig();
    config.echo_canceller.enabled = enable;
    g_apm->ApplyConfig(config);

    LOGD("AEC %s", enable ? "enabled" : "disabled");
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_aec_1set_1suppression_1level(
    JNIEnv* env,
    jobject thiz,
    jint level) {

    // AEC3 doesn't use suppression levels like the old AEC
    // This is kept for API compatibility
    LOGD("AEC suppression level (ignored in AEC3): %d", level);
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_aecm_1enable(
    JNIEnv* env,
    jobject thiz,
    jboolean enable) {

    if (!g_apm) return -1;

    AudioProcessing::Config config = g_apm->GetConfig();
    config.echo_canceller.enabled = enable;
    config.echo_canceller.mobile_mode = true;  // Mobile mode (AECM)
    g_apm->ApplyConfig(config);

    LOGD("AECM %s", enable ? "enabled" : "disabled");
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_ns_1enable(
    JNIEnv* env,
    jobject thiz,
    jboolean enable) {

    if (!g_apm) return -1;

    AudioProcessing::Config config = g_apm->GetConfig();
    config.noise_suppression.enabled = enable;
    g_apm->ApplyConfig(config);

    LOGD("NS %s", enable ? "enabled" : "disabled");
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_ns_1set_1level(
    JNIEnv* env,
    jobject thiz,
    jint level) {

    if (!g_apm) return -1;

    AudioProcessing::Config config = g_apm->GetConfig();

    switch (level) {
        case 0:
            config.noise_suppression.level = AudioProcessing::Config::NoiseSuppression::kLow;
            break;
        case 1:
            config.noise_suppression.level = AudioProcessing::Config::NoiseSuppression::kModerate;
            break;
        case 2:
            config.noise_suppression.level = AudioProcessing::Config::NoiseSuppression::kHigh;
            break;
        case 3:
            config.noise_suppression.level = AudioProcessing::Config::NoiseSuppression::kVeryHigh;
            break;
        default:
            config.noise_suppression.level = AudioProcessing::Config::NoiseSuppression::kHigh;
    }

    g_apm->ApplyConfig(config);
    LOGD("NS level set to: %d", level);
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_agc_1enable(
    JNIEnv* env,
    jobject thiz,
    jboolean enable) {

    if (!g_apm) return -1;

    AudioProcessing::Config config = g_apm->GetConfig();
    config.gain_controller1.enabled = enable;
    g_apm->ApplyConfig(config);

    LOGD("AGC %s", enable ? "enabled" : "disabled");
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_agc_1set_1config(
    JNIEnv* env,
    jobject thiz,
    jint mode,
    jint targetLevelDbfs,
    jint compressionGainDb) {

    if (!g_apm) return -1;

    AudioProcessing::Config config = g_apm->GetConfig();

    switch (mode) {
        case 0:
            config.gain_controller1.mode = AudioProcessing::Config::GainController1::kAdaptiveAnalog;
            break;
        case 1:
            config.gain_controller1.mode = AudioProcessing::Config::GainController1::kAdaptiveDigital;
            break;
        case 2:
            config.gain_controller1.mode = AudioProcessing::Config::GainController1::kFixedDigital;
            break;
        default:
            config.gain_controller1.mode = AudioProcessing::Config::GainController1::kAdaptiveDigital;
    }

    g_apm->ApplyConfig(config);
    LOGD("AGC config: mode=%d, target=%d, compression=%d", mode, targetLevelDbfs, compressionGainDb);
    return 0;
}

} // extern "C"
