/*
YabaSanshiro Android Debug Logging
Copyright 2025 YabaSanshiro Team

This file provides unified debug logging macros for Android builds.
Logging is enabled only in Debug builds and disabled in Release builds.
*/

#ifndef ANDROID_DEBUG_LOG_H
#define ANDROID_DEBUG_LOG_H

#ifdef ANDROID
#include <android/log.h>

// Check if this is a debug build
// Debug builds define DEBUG or _DEBUG, Release builds define NDEBUG
#if defined(DEBUG) || defined(_DEBUG)
    // Debug build - enable logging
    #define YAB_LOG_ENABLED 1
#elif defined(NDEBUG)
    // Release build - disable logging
    #define YAB_LOG_ENABLED 0
#else
    // Default to debug mode if no build type is explicitly defined
    #define YAB_LOG_ENABLED 1
#endif

#if YAB_LOG_ENABLED
    // Debug build - actual logging functions
    #define YAB_LOGV(tag, ...) __android_log_print(ANDROID_LOG_VERBOSE, tag, __VA_ARGS__)
    #define YAB_LOGD(tag, ...) __android_log_print(ANDROID_LOG_DEBUG, tag, __VA_ARGS__)
    #define YAB_LOGI(tag, ...) __android_log_print(ANDROID_LOG_INFO, tag, __VA_ARGS__)
    #define YAB_LOGW(tag, ...) __android_log_print(ANDROID_LOG_WARN, tag, __VA_ARGS__)
    #define YAB_LOGE(tag, ...) __android_log_print(ANDROID_LOG_ERROR, tag, __VA_ARGS__)
    #define YAB_LOGF(tag, ...) __android_log_print(ANDROID_LOG_FATAL, tag, __VA_ARGS__)
#else
    // Release build - no-op macros
    #define YAB_LOGV(tag, ...)
    #define YAB_LOGD(tag, ...)
    #define YAB_LOGI(tag, ...)
    #define YAB_LOGW(tag, ...)
    #define YAB_LOGE(tag, ...)
    #define YAB_LOGF(tag, ...)
#endif

// Convenience macros with predefined tags for common modules
#define YAB_LOG_TAG_MAIN "yabause"
#define YAB_LOG_TAG_RA "YabauseRA"
#define YAB_LOG_TAG_RA_MEMORY "YabauseRAMemory"
#define YAB_LOG_TAG_RA_JNI "RetroAchievementsJNI"
#define YAB_LOG_TAG_VULKAN "vulkanYaba"
#define YAB_LOG_TAG_NETPLAY "YabaSanshiro"

// Module-specific convenience macros
#define MAIN_LOGD(...) YAB_LOGD(YAB_LOG_TAG_MAIN, __VA_ARGS__)
#define MAIN_LOGI(...) YAB_LOGI(YAB_LOG_TAG_MAIN, __VA_ARGS__)
#define MAIN_LOGW(...) YAB_LOGW(YAB_LOG_TAG_MAIN, __VA_ARGS__)
#define MAIN_LOGE(...) YAB_LOGE(YAB_LOG_TAG_MAIN, __VA_ARGS__)

#define RA_LOGD(...) YAB_LOGD(YAB_LOG_TAG_RA, __VA_ARGS__)
#define RA_LOGI(...) YAB_LOGI(YAB_LOG_TAG_RA, __VA_ARGS__)
#define RA_LOGW(...) YAB_LOGW(YAB_LOG_TAG_RA, __VA_ARGS__)
#define RA_LOGE(...) YAB_LOGE(YAB_LOG_TAG_RA, __VA_ARGS__)

#define RA_MEMORY_LOGD(...) YAB_LOGD(YAB_LOG_TAG_RA_MEMORY, __VA_ARGS__)
#define RA_MEMORY_LOGE(...) YAB_LOGE(YAB_LOG_TAG_RA_MEMORY, __VA_ARGS__)

#define RA_JNI_LOGD(...) YAB_LOGD(YAB_LOG_TAG_RA_JNI, __VA_ARGS__)
#define RA_JNI_LOGI(...) YAB_LOGI(YAB_LOG_TAG_RA_JNI, __VA_ARGS__)
#define RA_JNI_LOGW(...) YAB_LOGW(YAB_LOG_TAG_RA_JNI, __VA_ARGS__)
#define RA_JNI_LOGE(...) YAB_LOGE(YAB_LOG_TAG_RA_JNI, __VA_ARGS__)

#define VULKAN_LOGD(...) YAB_LOGD(YAB_LOG_TAG_VULKAN, __VA_ARGS__)
#define VULKAN_LOGI(...) YAB_LOGI(YAB_LOG_TAG_VULKAN, __VA_ARGS__)
#define VULKAN_LOGW(...) YAB_LOGW(YAB_LOG_TAG_VULKAN, __VA_ARGS__)
#define VULKAN_LOGE(...) YAB_LOGE(YAB_LOG_TAG_VULKAN, __VA_ARGS__)

#define NETPLAY_LOGI(...) YAB_LOGI(YAB_LOG_TAG_NETPLAY, __VA_ARGS__)

#else
// Non-Android platforms - use printf or disable
#include <stdio.h>

#if defined(DEBUG) || defined(_DEBUG)
    // Debug build - enable logging
    #define YAB_LOGV(tag, fmt, ...) //printf("[VERBOSE][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define YAB_LOGD(tag, fmt, ...) //printf("[DEBUG][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define YAB_LOGI(tag, fmt, ...) //printf("[INFO][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define YAB_LOGW(tag, fmt, ...) //printf("[WARN][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define YAB_LOGE(tag, fmt, ...) //printf("[ERROR][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define YAB_LOGF(tag, fmt, ...) //printf("[FATAL][%s] " fmt "\n", tag, ##__VA_ARGS__)
#elif defined(NDEBUG)
    // Release build - disable logging
    #define YAB_LOGV(tag, ...)
    #define YAB_LOGD(tag, ...)
    #define YAB_LOGI(tag, ...)
    #define YAB_LOGW(tag, ...)
    #define YAB_LOGE(tag, ...)
    #define YAB_LOGF(tag, ...)
#else
    // Default to debug mode if no build type is explicitly defined
    #define YAB_LOGV(tag, fmt, ...) printf("[VERBOSE][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define YAB_LOGD(tag, fmt, ...) printf("[DEBUG][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define YAB_LOGI(tag, fmt, ...) printf("[INFO][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define YAB_LOGW(tag, fmt, ...) printf("[WARN][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define YAB_LOGE(tag, fmt, ...) printf("[ERROR][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define YAB_LOGF(tag, fmt, ...) printf("[FATAL][%s] " fmt "\n", tag, ##__VA_ARGS__)
#endif

// Module-specific convenience macros for non-Android
#define MAIN_LOGD(fmt, ...) YAB_LOGD("yabause", fmt, ##__VA_ARGS__)
#define MAIN_LOGI(fmt, ...) YAB_LOGI("yabause", fmt, ##__VA_ARGS__)
#define MAIN_LOGW(fmt, ...) YAB_LOGW("yabause", fmt, ##__VA_ARGS__)
#define MAIN_LOGE(fmt, ...) YAB_LOGE("yabause", fmt, ##__VA_ARGS__)

#define RA_LOGD(fmt, ...) YAB_LOGD("YabauseRA", fmt, ##__VA_ARGS__)
#define RA_LOGI(fmt, ...) YAB_LOGI("YabauseRA", fmt, ##__VA_ARGS__)
#define RA_LOGW(fmt, ...) YAB_LOGW("YabauseRA", fmt, ##__VA_ARGS__)
#define RA_LOGE(fmt, ...) YAB_LOGE("YabauseRA", fmt, ##__VA_ARGS__)

#define RA_MEMORY_LOGD(fmt, ...) YAB_LOGD("YabauseRAMemory", fmt, ##__VA_ARGS__)
#define RA_MEMORY_LOGE(fmt, ...) YAB_LOGE("YabauseRAMemory", fmt, ##__VA_ARGS__)

#define RA_JNI_LOGD(fmt, ...) YAB_LOGD("RetroAchievementsJNI", fmt, ##__VA_ARGS__)
#define RA_JNI_LOGI(fmt, ...) YAB_LOGI("RetroAchievementsJNI", fmt, ##__VA_ARGS__)
#define RA_JNI_LOGW(fmt, ...) YAB_LOGW("RetroAchievementsJNI", fmt, ##__VA_ARGS__)
#define RA_JNI_LOGE(fmt, ...) YAB_LOGE("RetroAchievementsJNI", fmt, ##__VA_ARGS__)

#define VULKAN_LOGD(fmt, ...) YAB_LOGD("vulkanYaba", fmt, ##__VA_ARGS__)
#define VULKAN_LOGI(fmt, ...) YAB_LOGI("vulkanYaba", fmt, ##__VA_ARGS__)
#define VULKAN_LOGW(fmt, ...) YAB_LOGW("vulkanYaba", fmt, ##__VA_ARGS__)
#define VULKAN_LOGE(fmt, ...) YAB_LOGE("vulkanYaba", fmt, ##__VA_ARGS__)

#define NETPLAY_LOGI(fmt, ...) YAB_LOGI("YabaSanshiro", fmt, ##__VA_ARGS__)

#endif // ANDROID

#endif // ANDROID_DEBUG_LOG_H
