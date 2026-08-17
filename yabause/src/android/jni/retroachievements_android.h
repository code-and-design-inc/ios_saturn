/*
RetroAchievements Android JNI Interface Header
Copyright 2025 YabaSanshiro Team

This header defines the JNI interface for RetroAchievements integration on Android.
*/

#ifndef RETROACHIEVEMENTS_ANDROID_H
#define RETROACHIEVEMENTS_ANDROID_H

#include <jni.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize RetroAchievements system
 * @param env JNI environment
 * @param obj Java object instance
 * @param callback Java callback object for events
 * @return 0 on success, negative on error
 */
JNIEXPORT jint JNICALL 
Java_org_uoyabause_android_YabauseRunnable_initRetroAchievements(
    JNIEnv* env, jclass clazz, jobject callback);

/**
 * Shutdown RetroAchievements system
 * @param env JNI environment
 * @param obj Java object instance
 */
JNIEXPORT void JNICALL 
Java_org_uoyabause_android_YabauseRunnable_shutdownRetroAchievements(
    JNIEnv* env, jobject obj);

/**
 * Login to RetroAchievements
 * @param env JNI environment
 * @param obj Java object instance
 * @param username RetroAchievements username
 * @param api_token User's API token
 */
JNIEXPORT jboolean JNICALL 
Java_org_uoyabause_android_YabauseRunnable_loginRetroAchievements(
    JNIEnv* env, jclass clazz, jstring username, jstring password);

/**
 * Load game for achievement tracking
 * @param env JNI environment
 * @param obj Java object instance
 * @param game_data Game ROM data
 * @param filename Game filename
 */
JNIEXPORT jboolean JNICALL 
Java_org_uoyabause_android_YabauseRunnable_loadGameRetroAchievements(
    JNIEnv* env, jclass clazz, jstring gamePath);

/**
 * Process one frame of achievement checking
 * @param env JNI environment
 * @param obj Java object instance
 */
JNIEXPORT void JNICALL 
Java_org_uoyabause_android_YabauseRunnable_doFrameRetroAchievements(
    JNIEnv* env, jclass clazz);

/**
 * Set hardcore mode
 * @param env JNI environment
 * @param obj Java object instance
 * @param enabled True to enable hardcore mode
 */
JNIEXPORT void JNICALL 
Java_org_uoyabause_android_YabauseRunnable_setHardcoreEnabledRetroAchievements(
    JNIEnv* env, jclass clazz, jboolean enabled);

/**
 * Check if hardcore mode is enabled
 * @param env JNI environment
 * @param obj Java object instance
 * @return True if hardcore mode is enabled
 */
JNIEXPORT jboolean JNICALL 
Java_org_uoyabause_android_YabauseRunnable_isHardcoreEnabledRetroAchievements(
    JNIEnv* env, jclass clazz);

/**
 * Handle HTTP request response
 * @param env JNI environment
 * @param obj Java object instance
 * @param callback_data Callback data pointer
 * @param http_code HTTP response code
 * @param response Response body
 */
JNIEXPORT void JNICALL 
Java_org_uoyabause_android_YabauseRunnable_handleHttpResponse(
    JNIEnv* env, jclass clazz, jlong callback_data, jint http_code, jstring response);

/**
 * RetroAchievementsManager JNI Functions
 */

/**
 * Initialize RetroAchievements from manager
 * @param env JNI environment
 * @param thiz Java object instance
 * @return True on success
 */
JNIEXPORT jboolean JNICALL 
Java_org_uoyabause_android_RetroAchievementsManager_initializeNative(
    JNIEnv* env, jobject thiz);

/**
 * Login user from manager
 * @param env JNI environment
 * @param thiz Java object instance
 * @param username RetroAchievements username
 * @param password User password/token
 * @return True on success
 */
JNIEXPORT jboolean JNICALL 
Java_org_uoyabause_android_RetroAchievementsManager_loginUserNative(
    JNIEnv* env, jobject thiz, jstring username, jstring password);

/**
 * Load game from manager
 * @param env JNI environment
 * @param thiz Java object instance
 * @param gamePath Path to game file
 * @return True on success
 */
JNIEXPORT jboolean JNICALL 
Java_org_uoyabause_android_RetroAchievementsManager_loadGameNative(
    JNIEnv* env, jobject thiz, jstring gamePath);

/**
 * Process frame from manager
 * @param env JNI environment
 * @param thiz Java object instance
 */
JNIEXPORT void JNICALL 
Java_org_uoyabause_android_RetroAchievementsManager_doFrameNative(
    JNIEnv* env, jobject thiz);

/**
 * Set hardcore mode from manager
 * @param env JNI environment
 * @param thiz Java object instance
 * @param enabled True to enable hardcore mode
 */
JNIEXPORT void JNICALL 
Java_org_uoyabause_android_RetroAchievementsManager_setHardcoreEnabledNative(
    JNIEnv* env, jobject thiz, jboolean enabled);

/**
 * Check hardcore mode from manager
 * @param env JNI environment
 * @param thiz Java object instance
 * @return True if hardcore mode is enabled
 */
JNIEXPORT jboolean JNICALL 
Java_org_uoyabause_android_RetroAchievementsManager_isHardcoreEnabledNative(
    JNIEnv* env, jobject thiz);

/**
 * Check if the current game requires RetroAchievements processing
 * @param env JNI environment
 * @param thiz Java object instance
 * @return True if processing is required (game has achievements/leaderboards/rich presence)
 */
JNIEXPORT jboolean JNICALL 
Java_org_uoyabause_android_RetroAchievementsManager_isProcessingRequiredNative(
    JNIEnv* env, jobject thiz);

/**
 * Handle HTTP response from manager
 * @param env JNI environment
 * @param thiz Java object instance
 * @param callback_data Callback data pointer
 * @param http_code HTTP response code
 * @param response Response body
 */
JNIEXPORT void JNICALL 
Java_org_uoyabause_android_RetroAchievementsManager_handleHttpResponseNative(
    JNIEnv* env, jobject thiz, jlong callback_data, jint http_code, jstring response);

#ifdef __cplusplus
}
#endif

#endif // RETROACHIEVEMENTS_ANDROID_H