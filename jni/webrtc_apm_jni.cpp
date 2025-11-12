// WebRTC Audio Processing Module (APM) JNI Wrapper for M120
// Compatible with com.webrtc.audioprocessing.Apm Java class
// Supports 800ms AEC3 delay via extended filter configuration
//
// Author: Luke Poortinga, SMB Operations
// Date: November 2, 2025
// WebRTC Version: M120 (branch-heads/6099)

#include <jni.h>
#include <android/log.h>
#include <memory>
#include <cstring>

// WebRTC M120 headers
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/ref_counted_object.h"
#include "common_audio/resampler/include/resampler.h"
#include "api/audio/echo_canceller3_config.h"
#include "api/audio/echo_canceller3_factory.h"

#define LOG_TAG "WebRTC-APM"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace webrtc;

// Context structure to hold APM instance and configuration
struct ApmContext {
    rtc::scoped_refptr<AudioProcessing> apm;
    std::unique_ptr<Resampler> resampler;

    // Audio configuration
    int sample_rate_hz = 16000;
    int num_channels = 1;

    // Stream configuration
    StreamConfig input_config;
    StreamConfig output_config;

    ApmContext() {
        input_config = StreamConfig(sample_rate_hz, num_channels);
        output_config = StreamConfig(sample_rate_hz, num_channels);
    }
};

// Helper functions
static void SetContext(JNIEnv* env, jobject thiz, ApmContext* ctx) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "objData", "J");
    env->SetLongField(thiz, fid, reinterpret_cast<jlong>(ctx));
}

static ApmContext* GetContext(JNIEnv* env, jobject thiz) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "objData", "J");
    return reinterpret_cast<ApmContext*>(env->GetLongField(thiz, fid));
}

extern "C" {

// ============================================================================
// AEC3 Configuration Helper
// ============================================================================

/**
 * Create custom AEC3 configuration based on suppression level
 *
 * Suppression levels control the enr_suppress parameter in MaskingThresholds:
 * - Lower values = more aggressive suppression (more echo removed, but may affect speech)
 * - Higher values = less aggressive suppression (preserves more speech, but more echo)
 *
 * @param suppressionLevel 0=Low, 1=Moderate, 2=High (aggressive)
 * @return EchoCanceller3Config with customized suppression settings
 */
static EchoCanceller3Config CreateAec3Config(int suppressionLevel) {
    EchoCanceller3Config config;

    // CRITICAL: Maintain 800ms filter support from patch
    config.filter.refined.length_blocks = 40;  // 800ms support
    config.filter.coarse.length_blocks = 40;

    // Configure suppression based on user's preference
    // enr_suppress controls how aggressively echo is removed
    switch (suppressionLevel) {
        case 0:  // Low suppression - preserves more speech quality
            config.suppressor.normal_tuning.mask_lf.enr_suppress = 0.5f;   // Less aggressive low-freq
            config.suppressor.normal_tuning.mask_hf.enr_suppress = 0.15f;  // Less aggressive high-freq
            LOGI("AEC3 suppression: Low (enr_suppress: lf=0.5, hf=0.15)");
            break;

        case 1:  // Moderate suppression - balanced approach
            config.suppressor.normal_tuning.mask_lf.enr_suppress = 0.4f;   // Default low-freq
            config.suppressor.normal_tuning.mask_hf.enr_suppress = 0.1f;   // Default high-freq
            LOGI("AEC3 suppression: Moderate (enr_suppress: lf=0.4, hf=0.1)");
            break;

        case 2:  // High suppression - aggressive echo removal
        default:
            config.suppressor.normal_tuning.mask_lf.enr_suppress = 0.3f;   // More aggressive low-freq
            config.suppressor.normal_tuning.mask_hf.enr_suppress = 0.07f;  // More aggressive high-freq
            LOGI("AEC3 suppression: High (enr_suppress: lf=0.3, hf=0.07)");
            break;
    }

    return config;
}

// ============================================================================
// APM Lifecycle
// ============================================================================

JNIEXPORT jboolean JNICALL
Java_com_webrtc_audioprocessing_Apm_nativeCreateApmInstance(
    JNIEnv* env,
    jobject thiz,
    jboolean aecExtendFilter,
    jboolean speechIntelligibilityEnhance,
    jboolean delayAgnostic,
    jboolean beamforming,
    jboolean nextGenerationAec,
    jboolean experimentalNs,
    jboolean experimentalAgc,
    jint aecSuppressionLevel) {

    LOGI("Creating APM instance (AEC3 800ms support, M120)");
    LOGD("  aecExtendFilter=%d, delayAgnostic=%d, nextGenAec=%d, suppressionLevel=%d",
         aecExtendFilter, delayAgnostic, nextGenerationAec, aecSuppressionLevel);

    // Create context
    ApmContext* ctx = new ApmContext();

    // Configure AudioProcessing
    AudioProcessing::Config config;

    // AEC3 (Echo Cancellation 3) - Modern, handles high delays
    if (nextGenerationAec) {
        config.echo_canceller.enabled = true;
        config.echo_canceller.mobile_mode = false;  // Use full AEC3, not mobile

        // Create custom AEC3 configuration with user's suppression level
        EchoCanceller3Config aec3_config = CreateAec3Config(aecSuppressionLevel);

        // Build APM with custom AEC3 factory
        // IMPORTANT: Pass config to factory constructor, then call Create() with NO arguments
        ctx->apm = AudioProcessingBuilder()
            .SetEchoControlFactory(std::make_unique<EchoCanceller3Factory>(aec3_config))
            .Create();  // ← Must be .Create() with no arguments!

        LOGI("AEC3 enabled (delay-agnostic mode, 800ms support, custom suppression)");
    } else {
        // Create APM without custom AEC3 config
        ctx->apm = AudioProcessingBuilder().Create();

        config.echo_canceller.enabled = false;
        LOGD("AEC3 disabled (legacy mode)");
    }

    if (!ctx->apm) {
        LOGE("Failed to create APM instance");
        delete ctx;
        return JNI_FALSE;
    }

    // Noise suppression
    if (experimentalNs) {
        config.noise_suppression.enabled = true;
        config.noise_suppression.level = AudioProcessing::Config::NoiseSuppression::kHigh;
        LOGD("Noise suppression enabled (High)");
    }

    // Automatic Gain Control
    if (experimentalAgc) {
        config.gain_controller1.enabled = true;
        config.gain_controller1.mode = AudioProcessing::Config::GainController1::kAdaptiveDigital;
        LOGD("AGC enabled (Adaptive Digital)");
    }

    // High-pass filter (removes low-frequency rumble, improves AEC)
    config.high_pass_filter.enabled = true;

    // Apply AudioProcessing configuration (M120 API)
    ctx->apm->ApplyConfig(config);

    // Store context in Java object
    SetContext(env, thiz, ctx);

    LOGI("APM instance created successfully");
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_webrtc_audioprocessing_Apm_nativeFreeApmInstance(
    JNIEnv* env,
    jobject thiz) {

    ApmContext* ctx = GetContext(env, thiz);
    if (ctx) {
        LOGI("Freeing APM instance");
        delete ctx;
        SetContext(env, thiz, nullptr);
    }
}

// ============================================================================
// High-Pass Filter
// ============================================================================

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_high_1pass_1filter_1enable(
    JNIEnv* env,
    jobject thiz,
    jboolean enable) {

    ApmContext* ctx = GetContext(env, thiz);
    if (!ctx || !ctx->apm) return -1;

    AudioProcessing::Config config = ctx->apm->GetConfig();
    config.high_pass_filter.enabled = enable;
    ctx->apm->ApplyConfig(config);

    LOGD("High-pass filter %s", enable ? "enabled" : "disabled");
    return 0;
}

// ============================================================================
// AEC (Echo Cancellation) - Legacy
// ============================================================================

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_aec_1enable(
    JNIEnv* env,
    jobject thiz,
    jboolean enable) {

    ApmContext* ctx = GetContext(env, thiz);
    if (!ctx || !ctx->apm) return -1;

    // For AEC3, this is controlled by config
    AudioProcessing::Config config = ctx->apm->GetConfig();
    config.echo_canceller.enabled = enable;
    config.echo_canceller.mobile_mode = false;  // Use full AEC3
    ctx->apm->ApplyConfig(config);

    LOGD("AEC3 %s", enable ? "enabled" : "disabled");
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_aec_1set_1suppression_1level(
    JNIEnv* env,
    jobject thiz,
    jint level) {

    // AEC3 suppression level is internal, not configurable via API
    // This is a no-op for compatibility
    LOGD("AEC suppression level set to %d (internal to AEC3)", level);
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_aec_1clock_1drift_1compensation_1enable(
    JNIEnv* env,
    jobject thiz,
    jboolean enable) {

    // AEC3 handles this internally
    LOGD("Clock drift compensation (handled internally by AEC3)");
    return 0;
}

// ============================================================================
// AECM (Echo Cancellation Mobile)
// ============================================================================

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_aecm_1enable(
    JNIEnv* env,
    jobject thiz,
    jboolean enable) {

    ApmContext* ctx = GetContext(env, thiz);
    if (!ctx || !ctx->apm) return -1;

    AudioProcessing::Config config = ctx->apm->GetConfig();
    config.echo_canceller.enabled = enable;
    config.echo_canceller.mobile_mode = true;  // Use mobile mode
    ctx->apm->ApplyConfig(config);

    LOGD("AECM (mobile) %s", enable ? "enabled" : "disabled");
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_aecm_1set_1suppression_1level(
    JNIEnv* env,
    jobject thiz,
    jint level) {

    // Mobile mode suppression handled internally
    LOGD("AECM suppression level: %d", level);
    return 0;
}

// ============================================================================
// Noise Suppression
// ============================================================================

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_ns_1enable(
    JNIEnv* env,
    jobject thiz,
    jboolean enable) {

    ApmContext* ctx = GetContext(env, thiz);
    if (!ctx || !ctx->apm) return -1;

    AudioProcessing::Config config = ctx->apm->GetConfig();
    config.noise_suppression.enabled = enable;
    ctx->apm->ApplyConfig(config);

    LOGD("Noise suppression %s", enable ? "enabled" : "disabled");
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_ns_1set_1level(
    JNIEnv* env,
    jobject thiz,
    jint level) {

    ApmContext* ctx = GetContext(env, thiz);
    if (!ctx || !ctx->apm) return -1;

    AudioProcessing::Config config = ctx->apm->GetConfig();

    // Map level [0,1,2,3] to NS level
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

    ctx->apm->ApplyConfig(config);
    LOGD("NS level set to %d", level);
    return 0;
}

// ============================================================================
// Automatic Gain Control
// ============================================================================

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_agc_1enable(
    JNIEnv* env,
    jobject thiz,
    jboolean enable) {

    ApmContext* ctx = GetContext(env, thiz);
    if (!ctx || !ctx->apm) return -1;

    AudioProcessing::Config config = ctx->apm->GetConfig();
    config.gain_controller1.enabled = enable;
    ctx->apm->ApplyConfig(config);

    LOGD("AGC %s", enable ? "enabled" : "disabled");
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_agc_1set_1target_1level_1dbfs(
    JNIEnv* env,
    jobject thiz,
    jint level) {

    ApmContext* ctx = GetContext(env, thiz);
    if (!ctx || !ctx->apm) return -1;

    AudioProcessing::Config config = ctx->apm->GetConfig();
    config.gain_controller1.target_level_dbfs = level;
    ctx->apm->ApplyConfig(config);

    LOGD("AGC target level: %d dBFS", level);
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_agc_1set_1compression_1gain_1db(
    JNIEnv* env,
    jobject thiz,
    jint gain) {

    ApmContext* ctx = GetContext(env, thiz);
    if (!ctx || !ctx->apm) return -1;

    AudioProcessing::Config config = ctx->apm->GetConfig();
    config.gain_controller1.compression_gain_db = gain;
    ctx->apm->ApplyConfig(config);

    LOGD("AGC compression gain: %d dB", gain);
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_agc_1enable_1limiter(
    JNIEnv* env,
    jobject thiz,
    jboolean enable) {

    ApmContext* ctx = GetContext(env, thiz);
    if (!ctx || !ctx->apm) return -1;

    AudioProcessing::Config config = ctx->apm->GetConfig();
    config.gain_controller1.enable_limiter = enable;
    ctx->apm->ApplyConfig(config);

    LOGD("AGC limiter %s", enable ? "enabled" : "disabled");
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_agc_1set_1analog_1level_1limits(
    JNIEnv* env,
    jobject thiz,
    jint minimum,
    jint maximum) {

    ApmContext* ctx = GetContext(env, thiz);
    if (!ctx || !ctx->apm) return -1;

    // M120 doesn't support analog_level_minimum/maximum in Config
    // These settings are not available in this version
    LOGD("AGC analog limits not supported in M120 (requested: %d - %d)", minimum, maximum);
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_agc_1set_1mode(
    JNIEnv* env,
    jobject thiz,
    jint mode) {

    ApmContext* ctx = GetContext(env, thiz);
    if (!ctx || !ctx->apm) return -1;

    AudioProcessing::Config config = ctx->apm->GetConfig();

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

    ctx->apm->ApplyConfig(config);
    LOGD("AGC mode set to %d", mode);
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_agc_1set_1stream_1analog_1level(
    JNIEnv* env,
    jobject thiz,
    jint level) {

    ApmContext* ctx = GetContext(env, thiz);
    if (!ctx || !ctx->apm) return -1;

    ctx->apm->set_stream_analog_level(level);
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_agc_1stream_1analog_1level(
    JNIEnv* env,
    jobject thiz) {

    ApmContext* ctx = GetContext(env, thiz);
    if (!ctx || !ctx->apm) return -1;

    return ctx->apm->recommended_stream_analog_level();
}

// ============================================================================
// Voice Activity Detection
// ============================================================================

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_vad_1enable(
    JNIEnv* env,
    jobject thiz,
    jboolean enable) {

    ApmContext* ctx = GetContext(env, thiz);
    if (!ctx || !ctx->apm) return -1;

    // M120 doesn't have voice_detection in Config
    // VAD functionality is not available through this API in M120
    LOGD("VAD not supported in M120 Config API (requested: %s)", enable ? "enabled" : "disabled");
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_vad_1set_1likelihood(
    JNIEnv* env,
    jobject thiz,
    jint likelihood) {

    // VAD likelihood is internal to WebRTC
    LOGD("VAD likelihood: %d", likelihood);
    return 0;
}

JNIEXPORT jboolean JNICALL
Java_com_webrtc_audioprocessing_Apm_vad_1stream_1has_1voice(
    JNIEnv* env,
    jobject thiz) {

    ApmContext* ctx = GetContext(env, thiz);
    if (!ctx || !ctx->apm) return JNI_FALSE;

    // Get voice detection result from last processed frame
    AudioProcessingStats stats = ctx->apm->GetStatistics();
    return stats.voice_detected.value_or(false) ? JNI_TRUE : JNI_FALSE;
}

// ============================================================================
// Stream Processing
// ============================================================================

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_ProcessStream(
    JNIEnv* env,
    jobject thiz,
    jshortArray nearEnd,
    jint offset) {

    ApmContext* ctx = GetContext(env, thiz);
    if (!ctx || !ctx->apm) return -1;

    // Get array
    jsize length = env->GetArrayLength(nearEnd);
    jshort* data = env->GetShortArrayElements(nearEnd, nullptr);

    if (!data) return -2;

    // Expected frame size: 10ms at 16kHz = 160 samples
    const int frame_size = 160;

    if (length - offset < frame_size) {
        env->ReleaseShortArrayElements(nearEnd, data, 0);
        return -3;
    }

    // Convert to normalized float [-1.0, 1.0]
    // WebRTC M120 expects normalized floats, NOT raw int16 values!
    float float_buffer[frame_size];
    for (int i = 0; i < frame_size; i++) {
        float_buffer[i] = static_cast<float>(data[offset + i]) / 32768.0f;
    }

    // Process capture stream (microphone)
    float* channel_ptr = float_buffer;
    int result = ctx->apm->ProcessStream(
        &channel_ptr,
        ctx->input_config,
        ctx->output_config,
        &channel_ptr);

    // Convert back to int16, with proper clamping
    for (int i = 0; i < frame_size; i++) {
        float sample = float_buffer[i] * 32768.0f;
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;
        data[offset + i] = static_cast<jshort>(sample);
    }

    env->ReleaseShortArrayElements(nearEnd, data, 0);
    return result;
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_ProcessReverseStream(
    JNIEnv* env,
    jobject thiz,
    jshortArray farEnd,
    jint offset) {

    ApmContext* ctx = GetContext(env, thiz);
    if (!ctx || !ctx->apm) return -1;

    // Get array
    jsize length = env->GetArrayLength(farEnd);
    jshort* data = env->GetShortArrayElements(farEnd, nullptr);

    if (!data) return -2;

    const int frame_size = 160;

    if (length - offset < frame_size) {
        env->ReleaseShortArrayElements(farEnd, data, JNI_ABORT);
        return -3;
    }

    // Convert to normalized float [-1.0, 1.0]
    // WebRTC M120 expects normalized floats, NOT raw int16 values!
    float float_buffer[frame_size];
    for (int i = 0; i < frame_size; i++) {
        float_buffer[i] = static_cast<float>(data[offset + i]) / 32768.0f;
    }

    // Process render stream (speaker reference for AEC)
    float* channel_ptr = float_buffer;
    int result = ctx->apm->ProcessReverseStream(
        &channel_ptr,
        ctx->input_config,
        ctx->output_config,
        &channel_ptr);

    env->ReleaseShortArrayElements(farEnd, data, JNI_ABORT);
    return result;
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_set_1stream_1delay_1ms(
    JNIEnv* env,
    jobject thiz,
    jint delay) {

    ApmContext* ctx = GetContext(env, thiz);
    if (!ctx || !ctx->apm) return -1;

    // Set delay hint for AEC3
    ctx->apm->set_stream_delay_ms(delay);
    LOGD("Stream delay hint set to %d ms", delay);
    return 0;
}

// ============================================================================
// Resampler (for compatibility)
// ============================================================================

JNIEXPORT jboolean JNICALL
Java_com_webrtc_audioprocessing_Apm_SamplingInit(
    JNIEnv* env,
    jobject thiz,
    jint inFreq,
    jint outFreq,
    jlong numChannels) {

    ApmContext* ctx = GetContext(env, thiz);
    if (!ctx) return JNI_FALSE;

    ctx->resampler.reset(new Resampler(inFreq, outFreq, numChannels));
    LOGD("Resampler initialized: %d Hz -> %d Hz", inFreq, outFreq);
    return JNI_TRUE;
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_SamplingReset(
    JNIEnv* env,
    jobject thiz,
    jint inFreq,
    jint outFreq,
    jlong numChannels) {

    ApmContext* ctx = GetContext(env, thiz);
    if (!ctx || !ctx->resampler) return -1;

    return ctx->resampler->Reset(inFreq, outFreq, numChannels);
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_SamplingResetIfNeeded(
    JNIEnv* env,
    jobject thiz,
    jint inFreq,
    jint outFreq,
    jlong numChannels) {

    ApmContext* ctx = GetContext(env, thiz);
    if (!ctx || !ctx->resampler) return -1;

    return ctx->resampler->ResetIfNeeded(inFreq, outFreq, numChannels);
}

JNIEXPORT jint JNICALL
Java_com_webrtc_audioprocessing_Apm_SamplingPush(
    JNIEnv* env,
    jobject thiz,
    jshortArray samplesIn,
    jlong lengthIn,
    jshortArray samplesOut,
    jlong maxLen,
    jlong outLen) {

    ApmContext* ctx = GetContext(env, thiz);
    if (!ctx || !ctx->resampler) return -1;

    jshort* in_data = env->GetShortArrayElements(samplesIn, nullptr);
    jshort* out_data = env->GetShortArrayElements(samplesOut, nullptr);

    if (!in_data || !out_data) {
        if (in_data) env->ReleaseShortArrayElements(samplesIn, in_data, JNI_ABORT);
        if (out_data) env->ReleaseShortArrayElements(samplesOut, out_data, JNI_ABORT);
        return -2;
    }

    size_t out_length = 0;
    int result = ctx->resampler->Push(
        in_data, lengthIn,
        out_data, maxLen,
        out_length);

    env->ReleaseShortArrayElements(samplesIn, in_data, JNI_ABORT);
    env->ReleaseShortArrayElements(samplesOut, out_data, 0);

    return result;
}

JNIEXPORT jboolean JNICALL
Java_com_webrtc_audioprocessing_Apm_SamplingDestroy(
    JNIEnv* env,
    jobject thiz) {

    ApmContext* ctx = GetContext(env, thiz);
    if (!ctx) return JNI_FALSE;

    ctx->resampler.reset();
    LOGD("Resampler destroyed");
    return JNI_TRUE;
}

} // extern "C"
