/*
RetroAchievements Android JNI Interface Implementation
Copyright 2025 YabaSanshiro Team

This file implements the JNI interface for RetroAchievements integration on Android.
*/
#include "retroachievements_android.h"
#include "../../retroachievements/yabause_ra_integration.h"
#include "rc_client.h"
#include "rc_hash.h"
#include "rc_consoles.h"
#include "../../android_debug_log.h"
#include <memory>
#include <string>

// CHD support
extern "C" {
#include "chd.h"
}

// Use direct Android logging to ensure visibility
#include <android/log.h>
#define LOG_TAG "RA_CHD"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// Global JNI references
static JavaVM* g_java_vm = nullptr;
static jobject g_callback_obj = nullptr;
static jclass g_callback_class = nullptr;

// Java method IDs
static jmethodID g_on_achievement_unlocked = nullptr;
static jmethodID g_on_leaderboard_submit = nullptr;
static jmethodID g_on_rich_presence_update = nullptr;
static jmethodID g_on_game_load_complete = nullptr;
static jmethodID g_on_login_complete = nullptr;
static jmethodID g_on_http_request = nullptr;
static jmethodID g_on_leaderboard_tracker_show = nullptr;
static jmethodID g_on_leaderboard_tracker_hide = nullptr;
static jmethodID g_on_leaderboard_tracker_update = nullptr;
static jmethodID g_on_challenge_indicator_show = nullptr;
static jmethodID g_on_challenge_indicator_hide = nullptr;
static jmethodID g_on_progress_indicator_show = nullptr;
static jmethodID g_on_progress_indicator_hide = nullptr;
static jmethodID g_on_progress_indicator_update = nullptr;
static jmethodID g_on_game_placard = nullptr;
static jmethodID g_on_game_mastery = nullptr;
static jmethodID g_on_server_error = nullptr;

// Tracker data structure
struct tracker_data {
    uint32_t id;
    char display[256];
    bool visible;
};

// Challenge indicator data structure
struct challenge_indicator_data {
    uint32_t achievement_id;
    char badge_name[64];
    char image_url[256];
    bool visible;
};

// Progress indicator data structure (only one at a time)
struct progress_indicator_data {
    uint32_t achievement_id;
    char title[256];
    char badge_name[64];
    char image_url[256];
    char progress[256];
    float percent;
    bool visible;
};

// Active trackers and indicators storage
#include <map>
static std::map<uint32_t, tracker_data> g_active_trackers;
static std::map<uint32_t, challenge_indicator_data> g_active_challenge_indicators;
static progress_indicator_data g_active_progress_indicator = {0}; // Only one progress indicator at a time

// Utility functions
namespace {

JNIEnv* getJNIEnv() {
    JNIEnv* env = nullptr;
    if (g_java_vm) {
        jint result = g_java_vm->GetEnv((void**)&env, JNI_VERSION_1_6);
        if (result == JNI_EDETACHED) {
            // Thread not attached, attach it
            result = g_java_vm->AttachCurrentThread(&env, nullptr);
            if (result != JNI_OK) {
                LOGE("Failed to attach thread to JVM: %d", result);
                return nullptr;
            }
        } else if (result != JNI_OK) {
            LOGE("Failed to get JNI environment: %d", result);
            return nullptr;
        }
    }
    return env;
}

bool wasThreadAttached() {
    JNIEnv* env = nullptr;
    if (g_java_vm) {
        jint result = g_java_vm->GetEnv((void**)&env, JNI_VERSION_1_6);
        return result != JNI_EDETACHED;
    }
    return false;
}

void detachCurrentThreadIfNeeded(bool was_attached) {
    if (!was_attached && g_java_vm) {
        g_java_vm->DetachCurrentThread();
    }
}

std::string jstringToString(JNIEnv* env, jstring jstr) {
    if (!jstr) return "";
    
    const char* chars = env->GetStringUTFChars(jstr, nullptr);
    std::string result(chars);
    env->ReleaseStringUTFChars(jstr, chars);
    return result;
}

// Tracker management functions
tracker_data* find_tracker(uint32_t id) {
    auto it = g_active_trackers.find(id);
    return (it != g_active_trackers.end()) ? &it->second : nullptr;
}

void create_tracker(uint32_t id, const char* display) {
    tracker_data data;
    data.id = id;
    strncpy(data.display, display, sizeof(data.display) - 1);
    data.display[sizeof(data.display) - 1] = '\0';
    data.visible = true;
    
    g_active_trackers[id] = data;
    LOGD("Created tracker %u with display: %s", id, display);
}

void destroy_tracker(uint32_t id) {
    g_active_trackers.erase(id);
    LOGD("Destroyed tracker %u", id);
}

// Challenge indicator management functions
void create_challenge_indicator(uint32_t achievement_id, const char* badge_name, const char* image_url) {
    challenge_indicator_data data;
    data.achievement_id = achievement_id;
    strncpy(data.badge_name, badge_name ? badge_name : "", sizeof(data.badge_name) - 1);
    data.badge_name[sizeof(data.badge_name) - 1] = '\0';
    strncpy(data.image_url, image_url ? image_url : "", sizeof(data.image_url) - 1);
    data.image_url[sizeof(data.image_url) - 1] = '\0';
    data.visible = true;
    
    g_active_challenge_indicators[achievement_id] = data;
    LOGD("Created challenge indicator for achievement %u with badge: %s", achievement_id, badge_name);
}

void destroy_challenge_indicator(uint32_t achievement_id) {
    g_active_challenge_indicators.erase(achievement_id);
    LOGD("Destroyed challenge indicator for achievement %u", achievement_id);
}

// Progress indicator management functions (single indicator)
void update_progress_indicator(const rc_client_achievement_t* achievement, const char* image_url) {
    g_active_progress_indicator.achievement_id = achievement->id;
    strncpy(g_active_progress_indicator.title, achievement->title ? achievement->title : "", sizeof(g_active_progress_indicator.title) - 1);
    g_active_progress_indicator.title[sizeof(g_active_progress_indicator.title) - 1] = '\0';
    strncpy(g_active_progress_indicator.badge_name, achievement->badge_name, sizeof(g_active_progress_indicator.badge_name) - 1);
    g_active_progress_indicator.badge_name[sizeof(g_active_progress_indicator.badge_name) - 1] = '\0';
    strncpy(g_active_progress_indicator.image_url, image_url ? image_url : "", sizeof(g_active_progress_indicator.image_url) - 1);
    g_active_progress_indicator.image_url[sizeof(g_active_progress_indicator.image_url) - 1] = '\0';
    strncpy(g_active_progress_indicator.progress, achievement->measured_progress, sizeof(g_active_progress_indicator.progress) - 1);
    g_active_progress_indicator.progress[sizeof(g_active_progress_indicator.progress) - 1] = '\0';
    g_active_progress_indicator.percent = achievement->measured_percent;
    
    // Log with proper percentage display  
    float displayPercent = achievement->measured_percent <= 1.0f ? 
        achievement->measured_percent * 100.0f : 
        (achievement->measured_percent > 100.0f ? 100.0f : achievement->measured_percent);
    LOGD("Updated progress indicator for achievement %u: %s (%.1f%%)", achievement->id, achievement->measured_progress, displayPercent);
}

void show_progress_indicator() {
    g_active_progress_indicator.visible = true;
    LOGD("Showing progress indicator for achievement %u", g_active_progress_indicator.achievement_id);
}

void hide_progress_indicator() {
    g_active_progress_indicator.visible = false;
    g_active_progress_indicator.achievement_id = 0;
    LOGD("Hiding progress indicator");
}

// Leaderboard tracker event handlers
void leaderboard_tracker_update(const rc_client_leaderboard_tracker_t* tracker) {
    tracker_data* data = find_tracker(tracker->id);
    if (data) {
        strncpy(data->display, tracker->display, sizeof(data->display) - 1);
        data->display[sizeof(data->display) - 1] = '\0';
        LOGD("Updated tracker %u with display: %s", tracker->id, tracker->display);
        
        // Call Java update callback
        JNIEnv* env = getJNIEnv();
        if (env && g_callback_obj && g_on_leaderboard_tracker_update) {
            jstring displayStr = env->NewStringUTF(tracker->display);
            env->CallVoidMethod(g_callback_obj, g_on_leaderboard_tracker_update,
                               tracker->id, displayStr);
            env->DeleteLocalRef(displayStr);
        }
    }
}

void leaderboard_tracker_show(const rc_client_leaderboard_tracker_t* tracker) {
    create_tracker(tracker->id, tracker->display);
    
    // Call Java show callback
    JNIEnv* env = getJNIEnv();
    if (env && g_callback_obj && g_on_leaderboard_tracker_show) {
        jstring displayStr = env->NewStringUTF(tracker->display);
        env->CallVoidMethod(g_callback_obj, g_on_leaderboard_tracker_show,
                           tracker->id, displayStr);
        env->DeleteLocalRef(displayStr);
    }
}

void leaderboard_tracker_hide(const rc_client_leaderboard_tracker_t* tracker) {
    destroy_tracker(tracker->id);
    
    // Call Java hide callback
    JNIEnv* env = getJNIEnv();
    if (env && g_callback_obj && g_on_leaderboard_tracker_hide) {
        env->CallVoidMethod(g_callback_obj, g_on_leaderboard_tracker_hide, tracker->id);
    }
}

// Challenge indicator event handlers
void challenge_indicator_show(const rc_client_achievement_t* achievement) {
    char image_url[256] = {0};
    
    // Get the achievement badge image URL
    if (rc_client_achievement_get_image_url(achievement, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED, image_url, sizeof(image_url)) != RC_OK) {
        image_url[0] = '\0'; // Clear on failure
    }
    
    create_challenge_indicator(achievement->id, achievement->badge_name, image_url);
    
    // Call Java show callback
    JNIEnv* env = getJNIEnv();
    if (env && g_callback_obj && g_on_challenge_indicator_show) {
        jstring titleStr = env->NewStringUTF(achievement->title ? achievement->title : "");
        jstring imageUrlStr = env->NewStringUTF(image_url);
        
        env->CallVoidMethod(g_callback_obj, g_on_challenge_indicator_show,
                           achievement->id, titleStr, imageUrlStr);
        
        env->DeleteLocalRef(titleStr);
        env->DeleteLocalRef(imageUrlStr);
    }
}

void challenge_indicator_hide(const rc_client_achievement_t* achievement) {
    destroy_challenge_indicator(achievement->id);
    
    // Call Java hide callback
    JNIEnv* env = getJNIEnv();
    if (env && g_callback_obj && g_on_challenge_indicator_hide) {
        env->CallVoidMethod(g_callback_obj, g_on_challenge_indicator_hide, achievement->id);
    }
}

// Progress indicator event handlers following RC client pattern
void progress_indicator_update_event(const rc_client_achievement_t* achievement) {
    char image_url[256] = {0};
    
    // Get the locked achievement badge image URL
    if (rc_client_achievement_get_image_url(achievement, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE, image_url, sizeof(image_url)) != RC_OK) {
        image_url[0] = '\0'; // Clear on failure
    }
    
    // Update the progress indicator data
    update_progress_indicator(achievement, image_url);
    
    // Call Java update callback
    JNIEnv* env = getJNIEnv();
    if (env && g_callback_obj && g_on_progress_indicator_update) {
        jstring titleStr = env->NewStringUTF(achievement->title ? achievement->title : "");
        jstring progressStr = env->NewStringUTF(achievement->measured_progress);
        jstring imageUrlStr = env->NewStringUTF(image_url);
        // Debug log to check the range of measured_percent
        LOGD("Raw measured_percent: %.6f for achievement %d", achievement->measured_percent, achievement->id);
        
        // Handle different possible ranges of measured_percent
        jint progressPercent;
        if (achievement->measured_percent <= 1.0f) {
            // Range 0.0 - 1.0, convert to 0-100
            progressPercent = (jint)(achievement->measured_percent * 100.0f);
        } else {
            // Already in 0-100+ range, use as-is but cap at 100
            progressPercent = (jint)achievement->measured_percent;
            if (progressPercent > 100) {
                progressPercent = 100;
            }
        }
        
        env->CallVoidMethod(g_callback_obj, g_on_progress_indicator_update,
                           achievement->id, titleStr, progressStr, imageUrlStr, progressPercent);
        
        env->DeleteLocalRef(titleStr);
        env->DeleteLocalRef(progressStr);
        env->DeleteLocalRef(imageUrlStr);
    }
}

void progress_indicator_show_event(const rc_client_achievement_t* achievement) {
    // SHOW event: update the indicator then show it
    progress_indicator_update_event(achievement);
    show_progress_indicator();
    
    // Call Java show callback
    JNIEnv* env = getJNIEnv();
    if (env && g_callback_obj && g_on_progress_indicator_show) {
        env->CallVoidMethod(g_callback_obj, g_on_progress_indicator_show);
    }
}

void progress_indicator_hide_event() {
    hide_progress_indicator();
    
    // Call Java hide callback
    JNIEnv* env = getJNIEnv();
    if (env && g_callback_obj && g_on_progress_indicator_hide) {
        env->CallVoidMethod(g_callback_obj, g_on_progress_indicator_hide);
    }
}

void handleRAEvent(const rc_client_event_t* event) {
    JNIEnv* env = getJNIEnv();
    if (!env || !g_callback_obj) return;

    switch (event->type) {
        case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
            if (g_on_achievement_unlocked) {
                jstring title = env->NewStringUTF(event->achievement->title);
                jstring description = env->NewStringUTF(event->achievement->description);
                
                // Get achievement image URL for unlocked state
                char imageUrl[256] = {0};
                jstring imageUrlStr = nullptr;
                if (rc_client_achievement_get_image_url(event->achievement, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED, imageUrl, sizeof(imageUrl)) == RC_OK) {
                    imageUrlStr = env->NewStringUTF(imageUrl);
                }
                
                jboolean isUnofficial = (event->achievement->category == RC_CLIENT_ACHIEVEMENT_CATEGORY_UNOFFICIAL) ? JNI_TRUE : JNI_FALSE;
                
                // Call enhanced method signature: (ILjava/lang/String;Ljava/lang/String;ILjava/lang/String;Z)V
                env->CallVoidMethod(g_callback_obj, g_on_achievement_unlocked,
                                   event->achievement->id, title, description, 
                                   event->achievement->points, imageUrlStr, isUnofficial);
                
                env->DeleteLocalRef(title);
                env->DeleteLocalRef(description);
                if (imageUrlStr) env->DeleteLocalRef(imageUrlStr);
            } else {
                LOGD("Achievement unlocked but no callback available: %s", event->achievement->title);
            }
            break;

        case RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED:
            // This event is fired when submission is initiated - just show notification without score data
            if (g_on_leaderboard_submit) {
                // Get leaderboard information
                jstring title = env->NewStringUTF(event->leaderboard->title ? event->leaderboard->title : "Leaderboard");
                jstring description = env->NewStringUTF("Score submission in progress...");
                jstring submittedScore = env->NewStringUTF(event->leaderboard->tracker_value ? event->leaderboard->tracker_value : ""); // Empty score for submission notification
                
                LOGD("JNI: Leaderboard submission initiated for: %s (ID: %u) %s",
                     event->leaderboard->title, event->leaderboard->id, event->leaderboard->tracker_value);
                
                // Call method with empty score for submission notification
                env->CallVoidMethod(g_callback_obj, g_on_leaderboard_submit,
                                   event->leaderboard->id, title, description, submittedScore);
                
                env->DeleteLocalRef(title);
                env->DeleteLocalRef(description);
                env->DeleteLocalRef(submittedScore);
            } else {
                LOGD("Leaderboard score submitted but no callback available: %s",
                     event->leaderboard->title ? event->leaderboard->title : "Unknown");
            }
            break;
            
        case RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD:
            // This event is fired when server response is received - has actual score data
            if (g_on_leaderboard_submit && event->leaderboard_scoreboard) {
                // Get leaderboard information with actual score data
                jstring title = env->NewStringUTF(event->leaderboard->title ? event->leaderboard->title : "Leaderboard");
                jstring description = env->NewStringUTF(event->leaderboard->description ? event->leaderboard->description : "Score submitted");
                jstring submittedScore = env->NewStringUTF(event->leaderboard_scoreboard->submitted_score);
                
                LOGD("JNI: Leaderboard scoreboard received - submitted_score: '%s', new_rank: %u, total_entries: %u", 
                     event->leaderboard_scoreboard->submitted_score,
                     event->leaderboard_scoreboard->new_rank,
                     event->leaderboard_scoreboard->num_entries);
                
                // Call method with actual score data for Firebase submission
                env->CallVoidMethod(g_callback_obj, g_on_leaderboard_submit,
                                   event->leaderboard->id, title, description, submittedScore);
                
                env->DeleteLocalRef(title);
                env->DeleteLocalRef(description);
                env->DeleteLocalRef(submittedScore);
            } else {
                LOGD("Leaderboard scoreboard received but no callback available or no scoreboard data");
            }
            break;

        // Rich presence events are handled differently in newer API

        case RC_CLIENT_EVENT_GAME_COMPLETED: {
            // Handle game completion event (mastery)
            LOGD("Game completed/mastered!");
            
            JNIEnv* env = getJNIEnv();
            if (env && g_callback_obj && g_on_game_mastery) {
                // Get the RA integration instance and client
                YabauseRA::Integration* ra = YabauseRA::Integration::getInstance();
                if (!ra) {
                    LOGE("RetroAchievements integration not available");
                    break;
                }
                
                rc_client_t* client = ra->getClient();
                if (!client) {
                    LOGE("RetroAchievements client not available");
                    break;
                }
                
                const rc_client_game_t* game = rc_client_get_game_info(client);
                if (game) {
                    // Get game badge image URL
                    char image_url[256] = {0};
                    rc_client_game_get_image_url(game, image_url, sizeof(image_url));
                    
                    // Count achievements and points from the completed subset
                    int achievement_count = 0;
                    int total_points = 0;
                    
                    // The event->subset contains the completed subset info
                    if (event->subset) {
                        // Use rcheevos API to iterate through achievements in the subset
                        rc_client_achievement_list_t* list = rc_client_create_achievement_list(client, 
                            RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
                        if (list) {
                            // Count all achievements and points in the game
                            for (uint32_t i = 0; i < list->num_buckets; i++) {
                                for (uint32_t j = 0; j < list->buckets[i].num_achievements; j++) {
                                    const rc_client_achievement_t* achievement = list->buckets[i].achievements[j];
                                    if (achievement && achievement->state == RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED) {
                                        achievement_count++;
                                        total_points += achievement->points;
                                    }
                                }
                            }
                            rc_client_destroy_achievement_list(list);
                        }
                    }
                    
                    // Check if hardcore mode is enabled
                    bool is_hardcore = rc_client_get_hardcore_enabled(client);
                    
                    // Get user info (optional - may be null)
                    const rc_client_user_t* user = rc_client_get_user_info(client);
                    const char* username = user ? user->display_name : nullptr;
                    
                    // Create Java strings
                    jstring gameTitle = env->NewStringUTF(game->title ? game->title : "Unknown Game");
                    jstring imageUrl = env->NewStringUTF(image_url[0] ? image_url : nullptr);
                    jstring usernameStr = username ? env->NewStringUTF(username) : nullptr;
                    jstring playtimeStr = nullptr; // Playtime not available from rcheevos directly
                    
                    // Call the Java callback
                    env->CallVoidMethod(g_callback_obj, g_on_game_mastery,
                                       gameTitle, imageUrl, achievement_count, total_points, 
                                       is_hardcore, usernameStr, playtimeStr);
                    
                    // Clean up local references
                    env->DeleteLocalRef(gameTitle);
                    if (imageUrl) env->DeleteLocalRef(imageUrl);
                    if (usernameStr) env->DeleteLocalRef(usernameStr);
                    if (playtimeStr) env->DeleteLocalRef(playtimeStr);
                }
            }
            break;
        }

        case RC_CLIENT_EVENT_SERVER_ERROR: {
            // Handle server errors that won't be retried
            const char* api = event->server_error->api ? event->server_error->api : "Unknown API";
            const char* error_message = event->server_error->error_message ? event->server_error->error_message : "Unknown error";
            
            LOGE("Server error: %s - %s", api, error_message);
            
            // Show the error to the user since these won't be retried
            JNIEnv* env = getJNIEnv();
            if (env && g_callback_obj && g_on_server_error) {
                jstring apiStr = env->NewStringUTF(api);
                jstring errorStr = env->NewStringUTF(error_message);
                
                env->CallVoidMethod(g_callback_obj, g_on_server_error, apiStr, errorStr);
                
                env->DeleteLocalRef(apiStr);
                env->DeleteLocalRef(errorStr);
            }
            break;
        }

        case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW:
            challenge_indicator_show(event->achievement);
            break;

        case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE:
            challenge_indicator_hide(event->achievement);
            break;

        case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW:
            progress_indicator_show_event(event->achievement);
            break;

        case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE:
            progress_indicator_hide_event();
            break;

        case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE:
            progress_indicator_update_event(event->achievement);
            break;

        case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW:
            leaderboard_tracker_show(event->leaderboard_tracker);
            break;

        case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE:
            leaderboard_tracker_hide(event->leaderboard_tracker);
            break;

        case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE:
            leaderboard_tracker_update(event->leaderboard_tracker);
            break;

        default:
            LOGD("Unhandled RA event type: %d", event->type);
            break;
    }
}

void handleRAServerRequest(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callback_data) {
    LOGD("handleRAServerRequest called from thread");
    
    // Check if thread was already attached before we try to get JNI env
    bool was_attached = wasThreadAttached();
    JNIEnv* env = getJNIEnv();
    
    if (!env || !g_callback_obj || !g_on_http_request) {
        LOGE("Failed to get JNI environment or callback objects not set");
        // No callback available, complete with error
        rc_api_server_response_t response = {0};
        response.http_status_code = 0; // Error status
        callback(&response, callback_data);
        
        // Detach thread if we attached it
        detachCurrentThreadIfNeeded(was_attached);
        return;
    }

    jstring url = env->NewStringUTF(request->url);
    jstring post_data = request->post_data ? env->NewStringUTF(request->post_data) : nullptr;

    // Store callback info for later use
    struct CallbackInfo {
        rc_client_server_callback_t callback;
        void* callback_data;
    };
    CallbackInfo* info = new CallbackInfo{callback, callback_data};
    
    LOGD("Calling Java HTTP request method: %s %s", request->url, request->post_data);
    env->CallVoidMethod(g_callback_obj, g_on_http_request, url, post_data, (jlong)info);
    
    // Check for JNI exceptions
    if (env->ExceptionCheck()) {
        LOGE("Exception occurred in CallVoidMethod");
        env->ExceptionDescribe();
        env->ExceptionClear();
        
        // Complete with error and cleanup
        rc_api_server_response_t response = {0};
        response.http_status_code = 0;
        callback(&response, callback_data);
        delete info;
    }
    
    // Clean up local references
    env->DeleteLocalRef(url);
    if (post_data) env->DeleteLocalRef(post_data);
    
    // Detach thread if we attached it
    detachCurrentThreadIfNeeded(was_attached);
}

void handleRALoginComplete(bool success, const char* username, const char* display_name, uint32_t score, const char* error_message) {
    LOGD("handleRALoginComplete: success=%s, user=%s, display=%s, score=%u", 
         success ? "true" : "false", 
         username ? username : "null", 
         display_name ? display_name : "null", 
         score);
    
    // Check if thread was already attached before we try to get JNI env
    bool was_attached = wasThreadAttached();
    JNIEnv* env = getJNIEnv();
    
    if (!env || !g_callback_obj) {
        LOGE("Failed to get JNI environment for login callback");
        // Detach thread if we attached it
        detachCurrentThreadIfNeeded(was_attached);
        return;
    }
    
    // Call the login complete callback if available
    if (g_on_login_complete) {
        jstring usernameStr = username ? env->NewStringUTF(username) : nullptr;
        
        // Call the Java callback method
        env->CallVoidMethod(g_callback_obj, g_on_login_complete, 
                           success ? JNI_TRUE : JNI_FALSE, usernameStr);
        
        // Check for JNI exceptions
        if (env->ExceptionCheck()) {
            LOGE("Exception occurred in login complete callback");
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        
        if (usernameStr) env->DeleteLocalRef(usernameStr);
        
        if (success) {
            LOGD("Login successful for user: %s (display: %s) with %u points", 
                 username ? username : "unknown", 
                 display_name ? display_name : "unknown", 
                 score);
        } else {
            LOGE("Login failed: %s", error_message ? error_message : "Unknown error");
        }
    } else {
        LOGW("Login complete callback not set - login result not forwarded to Java");
        if (success) {
            LOGD("Login successful for user: %s (display: %s) with %u points", 
                 username ? username : "unknown", 
                 display_name ? display_name : "unknown", 
                 score);
        } else {
            LOGE("Login failed: %s", error_message ? error_message : "Unknown error");
        }
    }
    
    // Detach thread if we attached it
    detachCurrentThreadIfNeeded(was_attached);
}

void handleRAGamePlacard(const char* game_title, const char* image_url, uint32_t unlocked_achievements, uint32_t total_achievements, uint32_t unlocked_points, uint32_t total_points, bool has_unsupported) {
    LOGD("handleRAGamePlacard: %s - %u/%u achievements (%u/%u points) unsupported=%s", 
         game_title ? game_title : "Unknown Game",
         unlocked_achievements, total_achievements,
         unlocked_points, total_points,
         has_unsupported ? "true" : "false");
    
    // Check if thread was already attached before we try to get JNI env
    bool was_attached = wasThreadAttached();
    JNIEnv* env = getJNIEnv();
    
    if (!env || !g_callback_obj) {
        LOGE("Failed to get JNI environment for game placard callback");
        // Detach thread if we attached it
        detachCurrentThreadIfNeeded(was_attached);
        return;
    }
    
    // For now, we'll use onGameLoadComplete to trigger the placard display
    // Format the message following rcheevos guidelines
    char message[256];
    if (total_achievements == 0) {
        snprintf(message, sizeof(message), "This game has no achievements.");
    } else if (has_unsupported) {
        uint32_t unsupported_count = total_achievements - (unlocked_achievements + (total_achievements - unlocked_achievements));
        snprintf(message, sizeof(message), "You have %u of %u achievements unlocked (%u unsupported).", 
                 unlocked_achievements, total_achievements, unsupported_count);
    } else {
        snprintf(message, sizeof(message), "You have %u of %u achievements unlocked.", 
                 unlocked_achievements, total_achievements);
    }
    
    // Call the new game placard callback if available
    if (g_on_game_placard) {
        jstring gameTitleStr = game_title ? env->NewStringUTF(game_title) : env->NewStringUTF("Game");
        jstring imageUrlStr = image_url ? env->NewStringUTF(image_url) : nullptr;
        
        // Call the Java callback method with game info and achievement counts
        env->CallVoidMethod(g_callback_obj, g_on_game_placard, 
                           gameTitleStr, imageUrlStr, 
                           (jint)unlocked_achievements, (jint)total_achievements);
        
        // Check for JNI exceptions
        if (env->ExceptionCheck()) {
            LOGE("Exception occurred in game placard callback");
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        
        env->DeleteLocalRef(gameTitleStr);
        if (imageUrlStr) env->DeleteLocalRef(imageUrlStr);
        
        LOGD("Game placard ready via new callback: %s - %u/%u achievements (%u/%u points)", 
             game_title ? game_title : "Unknown Game",
             unlocked_achievements, total_achievements,
             unlocked_points, total_points);
    } else {
        // Fallback to old game load complete callback
        if (g_on_game_load_complete) {
            jstring messageStr = env->NewStringUTF(message);
            
            env->CallVoidMethod(g_callback_obj, g_on_game_load_complete, 
                               JNI_TRUE, messageStr);
            
            if (env->ExceptionCheck()) {
                LOGE("Exception occurred in game load complete callback");
                env->ExceptionDescribe();
                env->ExceptionClear();
            }
            
            env->DeleteLocalRef(messageStr);
            
            LOGD("Game placard ready via fallback: %s - %u/%u achievements (%u/%u points)", 
                 game_title ? game_title : "Unknown Game",
                 unlocked_achievements, total_achievements,
                 unlocked_points, total_points);
        } else {
            LOGW("Game load complete callback not set - placard not displayed");
        }
    }
    
    // Detach thread if we attached it
    detachCurrentThreadIfNeeded(was_attached);
}

} // anonymous namespace

// JNI method implementations

extern "C" {

JNIEXPORT jint JNICALL 
Java_org_uoyabause_android_achievements_RetroAchievementsManager_initializeCallbacksNative(
    JNIEnv* env, jobject thiz, jobject callback) {

    LOGD("Initializing RetroAchievements JNI callbacks...");

    // Store Java VM reference
    env->GetJavaVM(&g_java_vm);

    // Store callback object
    if (g_callback_obj) {
        env->DeleteGlobalRef(g_callback_obj);
    }
    g_callback_obj = env->NewGlobalRef(callback);

    // Get callback class and method IDs
    g_callback_class = env->GetObjectClass(callback);
    
    g_on_achievement_unlocked = env->GetMethodID(g_callback_class, 
        "onAchievementUnlocked", "(ILjava/lang/String;Ljava/lang/String;ILjava/lang/String;Z)V");
    g_on_leaderboard_submit = env->GetMethodID(g_callback_class, 
        "onLeaderboardSubmit", "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    g_on_rich_presence_update = env->GetMethodID(g_callback_class, 
        "onRichPresenceUpdate", "(Ljava/lang/String;)V");
    g_on_game_load_complete = env->GetMethodID(g_callback_class, 
        "onGameLoadComplete", "(ZLjava/lang/String;)V");
    g_on_login_complete = env->GetMethodID(g_callback_class, 
        "onLoginComplete", "(ZLjava/lang/String;)V");    
    g_on_http_request = env->GetMethodID(g_callback_class, 
        "onHttpRequest", "(Ljava/lang/String;Ljava/lang/String;J)V");
    g_on_leaderboard_tracker_show = env->GetMethodID(g_callback_class, 
        "onLeaderboardTrackerShow", "(ILjava/lang/String;)V");
    g_on_leaderboard_tracker_hide = env->GetMethodID(g_callback_class, 
        "onLeaderboardTrackerHide", "(I)V");
    g_on_leaderboard_tracker_update = env->GetMethodID(g_callback_class, 
        "onLeaderboardTrackerUpdate", "(ILjava/lang/String;)V");
    g_on_challenge_indicator_show = env->GetMethodID(g_callback_class, 
        "onChallengeIndicatorShow", "(ILjava/lang/String;Ljava/lang/String;)V");
    g_on_challenge_indicator_hide = env->GetMethodID(g_callback_class, 
        "onChallengeIndicatorHide", "(I)V");
    g_on_progress_indicator_show = env->GetMethodID(g_callback_class, 
        "onProgressIndicatorShow", "()V");
    g_on_progress_indicator_hide = env->GetMethodID(g_callback_class, 
        "onProgressIndicatorHide", "()V");
    g_on_progress_indicator_update = env->GetMethodID(g_callback_class, 
        "onProgressIndicatorUpdate", "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;I)V");
    g_on_game_placard = env->GetMethodID(g_callback_class, 
        "onGamePlacard", "(Ljava/lang/String;Ljava/lang/String;II)V");
    g_on_game_mastery = env->GetMethodID(g_callback_class, 
        "onGameMastery", "(Ljava/lang/String;Ljava/lang/String;IIZLjava/lang/String;Ljava/lang/String;)V");
    g_on_server_error = env->GetMethodID(g_callback_class, 
        "onServerError", "(Ljava/lang/String;Ljava/lang/String;)V");

    if (!g_on_achievement_unlocked || !g_on_leaderboard_submit || 
        !g_on_rich_presence_update || !g_on_http_request) {
        LOGE("Failed to get Java method IDs");
        return -1;
    }

    LOGD("RetroAchievements JNI callbacks initialization complete");
    return 0;
}

JNIEXPORT jint JNICALL 
Java_org_uoyabause_android_YabauseRunnable_initRetroAchievements(
    JNIEnv* env, jclass clazz, jobject callback) {

    LOGD("Initializing RetroAchievements JNI...");

    // Store Java VM reference
    env->GetJavaVM(&g_java_vm);

    // Store callback object
    if (g_callback_obj) {
        env->DeleteGlobalRef(g_callback_obj);
    }
    g_callback_obj = env->NewGlobalRef(callback);

    // Get callback class and method IDs
    g_callback_class = env->GetObjectClass(callback);
    
    g_on_achievement_unlocked = env->GetMethodID(g_callback_class, 
        "onAchievementUnlocked", "(ILjava/lang/String;Ljava/lang/String;ILjava/lang/String;Z)V");
    g_on_leaderboard_submit = env->GetMethodID(g_callback_class, 
        "onLeaderboardSubmit", "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    g_on_rich_presence_update = env->GetMethodID(g_callback_class, 
        "onRichPresenceUpdate", "(Ljava/lang/String;)V");
    g_on_game_load_complete = env->GetMethodID(g_callback_class, 
        "onGameLoadComplete", "(ZLjava/lang/String;)V");
    g_on_login_complete = env->GetMethodID(g_callback_class, 
        "onLoginComplete", "(ZLjava/lang/String;)V");    
    g_on_http_request = env->GetMethodID(g_callback_class, 
        "onHttpRequest", "(Ljava/lang/String;Ljava/lang/String;J)V");
    g_on_leaderboard_tracker_show = env->GetMethodID(g_callback_class, 
        "onLeaderboardTrackerShow", "(ILjava/lang/String;)V");
    g_on_leaderboard_tracker_hide = env->GetMethodID(g_callback_class, 
        "onLeaderboardTrackerHide", "(I)V");
    g_on_leaderboard_tracker_update = env->GetMethodID(g_callback_class, 
        "onLeaderboardTrackerUpdate", "(ILjava/lang/String;)V");
    g_on_challenge_indicator_show = env->GetMethodID(g_callback_class, 
        "onChallengeIndicatorShow", "(ILjava/lang/String;Ljava/lang/String;)V");
    g_on_challenge_indicator_hide = env->GetMethodID(g_callback_class, 
        "onChallengeIndicatorHide", "(I)V");
    g_on_progress_indicator_show = env->GetMethodID(g_callback_class, 
        "onProgressIndicatorShow", "()V");
    g_on_progress_indicator_hide = env->GetMethodID(g_callback_class, 
        "onProgressIndicatorHide", "()V");
    g_on_progress_indicator_update = env->GetMethodID(g_callback_class, 
        "onProgressIndicatorUpdate", "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;I)V");
    g_on_game_placard = env->GetMethodID(g_callback_class, 
        "onGamePlacard", "(Ljava/lang/String;Ljava/lang/String;II)V");
    g_on_game_mastery = env->GetMethodID(g_callback_class, 
        "onGameMastery", "(Ljava/lang/String;Ljava/lang/String;IIZLjava/lang/String;Ljava/lang/String;)V");
    g_on_server_error = env->GetMethodID(g_callback_class, 
        "onServerError", "(Ljava/lang/String;Ljava/lang/String;)V");

    if (!g_on_achievement_unlocked || !g_on_leaderboard_submit || 
        !g_on_rich_presence_update || !g_on_http_request) {
        LOGE("Failed to get Java method IDs");
        return -1;
    }

    // Initialize the RetroAchievements integration
    YabauseRA::Integration* ra = YabauseRA::Integration::getInstance();
    if (!ra->initialize()) {
        LOGE("Failed to initialize RetroAchievements");
        return -2;
    }

    // Set callbacks
    ra->setEventCallback(handleRAEvent);
    ra->setServerCallback(handleRAServerRequest);
    ra->setLoginCallback(handleRALoginComplete);
    ra->setGamePlacardCallback(handleRAGamePlacard);

    LOGD("RetroAchievements JNI initialization complete");
    return 0;
}

JNIEXPORT void JNICALL 
Java_org_uoyabause_android_YabauseRunnable_shutdownRetroAchievements(
    JNIEnv* env, jobject obj) {

    LOGD("Shutting down RetroAchievements JNI...");

    YabauseRA::Integration* ra = YabauseRA::Integration::getInstance();
    ra->shutdown();

    // Clean up active trackers and indicators
    g_active_trackers.clear();
    g_active_challenge_indicators.clear();
    g_active_progress_indicator = {0};

    // Clean up global references
    if (g_callback_obj) {
        env->DeleteGlobalRef(g_callback_obj);
        g_callback_obj = nullptr;
    }

    g_callback_class = nullptr;
    g_java_vm = nullptr;

    LOGD("RetroAchievements JNI shutdown complete");
}

JNIEXPORT jboolean JNICALL 
Java_org_uoyabause_android_YabauseRunnable_loginRetroAchievements(
    JNIEnv* env, jclass clazz, jstring username, jstring password) {

    std::string user_str = jstringToString(env, username);
    std::string password_str = jstringToString(env, password);

    LOGD("Logging in user: %s", user_str.c_str());

    YabauseRA::Integration* ra = YabauseRA::Integration::getInstance();
    bool result = ra->loginUser(user_str.c_str(), password_str.c_str());
    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL 
Java_org_uoyabause_android_YabauseRunnable_loadGameRetroAchievements(
    JNIEnv* env, jclass clazz, jstring gamePath) {

    if (!gamePath) {
        LOGE("Game path is null");
        return JNI_FALSE;
    }

    std::string path_str = jstringToString(env, gamePath);
    LOGD("Loading RetroAchievements game: %s", path_str.c_str());

    YabauseRA::Integration* ra = YabauseRA::Integration::getInstance();
    bool success = ra->loadGameFromPath(path_str.c_str());
    return success ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL 
Java_org_uoyabause_android_YabauseRunnable_doFrameRetroAchievements(
    JNIEnv* env, jclass clazz) {

    YabauseRA::Integration* ra = YabauseRA::Integration::getInstance();
    ra->doFrame();
}

JNIEXPORT void JNICALL 
Java_org_uoyabause_android_YabauseRunnable_setHardcoreEnabledRetroAchievements(
    JNIEnv* env, jclass clazz, jboolean enabled) {

    LOGD("Setting hardcore mode: %s", enabled ? "enabled" : "disabled");

    YabauseRA::Integration* ra = YabauseRA::Integration::getInstance();
    ra->setHardcoreEnabled(enabled == JNI_TRUE);
}

JNIEXPORT jboolean JNICALL 
Java_org_uoyabause_android_YabauseRunnable_isHardcoreEnabledRetroAchievements(
    JNIEnv* env, jclass clazz) {

    YabauseRA::Integration* ra = YabauseRA::Integration::getInstance();
    return ra->isHardcoreEnabled() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL 
Java_org_uoyabause_android_YabauseRunnable_handleHttpResponse(
    JNIEnv* env, jclass clazz, jlong callback_data, jint http_code, jstring response) {

    std::string response_str = jstringToString(env, response);
    
    LOGD("HTTP response: code=%d, size=%zu", http_code, response_str.size());

    struct CallbackInfo {
        rc_client_server_callback_t callback;
        void* callback_data;
    };
    CallbackInfo* info = reinterpret_cast<CallbackInfo*>(callback_data);
    
    if (info && info->callback) {
        rc_api_server_response_t server_response = {0};
        server_response.http_status_code = http_code;
        server_response.body = response_str.c_str();
        server_response.body_length = response_str.size();
        
        info->callback(&server_response, info->callback_data);
        delete info; // Clean up allocated callback info
    }
}

// RetroAchievementsManager JNI functions
JNIEXPORT jboolean JNICALL 
Java_org_uoyabause_android_achievements_RetroAchievementsManager_initializeNative(
    JNIEnv* env, jobject thiz) {
    
    LOGD("Initializing RetroAchievements core system from manager");
    
    // Initialize the RetroAchievements integration
    YabauseRA::Integration* ra = YabauseRA::Integration::getInstance();
    if (!ra->initialize()) {
        LOGE("Failed to initialize RetroAchievements core");
        return JNI_FALSE;
    }

    // Set callbacks (only if callbacks are initialized)
    if (g_callback_obj && g_on_achievement_unlocked) {
        ra->setEventCallback(handleRAEvent);
        ra->setServerCallback(handleRAServerRequest);
        ra->setLoginCallback(handleRALoginComplete);
        ra->setGamePlacardCallback(handleRAGamePlacard);
        LOGD("RetroAchievements callbacks set successfully");
    } else {
        LOGW("RetroAchievements callbacks not initialized - some functionality may not work");
    }

    LOGD("RetroAchievements core system initialization complete");
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL 
Java_org_uoyabause_android_achievements_RetroAchievementsManager_loginUserNative(
    JNIEnv* env, jobject thiz, jstring username, jstring password) {
    
    if (!username || !password) {
        LOGE("Invalid username or password provided");
        return JNI_FALSE;
    }

    std::string username_str = jstringToString(env, username);
    std::string password_str = jstringToString(env, password);
    
    LOGD("Logging in user: %s", username_str.c_str());

    YabauseRA::Integration* ra = YabauseRA::Integration::getInstance();
    bool result = ra->loginUser(username_str.c_str(), password_str.c_str());
    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL 
Java_org_uoyabause_android_achievements_RetroAchievementsManager_loadGameNative(
    JNIEnv* env, jobject thiz, jstring gamePath) {
    
    if (!gamePath) {
        LOGE("Invalid game path provided");
        return JNI_FALSE;
    }

    std::string path_str = jstringToString(env, gamePath);
    LOGD("Loading RetroAchievements game from manager: %s", path_str.c_str());

    YabauseRA::Integration* ra = YabauseRA::Integration::getInstance();
    bool success = ra->loadGameFromPath(path_str.c_str());
    return success ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL 
Java_org_uoyabause_android_achievements_RetroAchievementsManager_doFrameNative(
    JNIEnv* env, jobject thiz) {
    
    YabauseRA::Integration* ra = YabauseRA::Integration::getInstance();
    ra->doFrame();
}

JNIEXPORT void JNICALL 
Java_org_uoyabause_android_achievements_RetroAchievementsManager_setHardcoreEnabledNative(
    JNIEnv* env, jobject thiz, jboolean enabled) {
    
    YabauseRA::Integration* ra = YabauseRA::Integration::getInstance();
    ra->setHardcoreEnabled(enabled == JNI_TRUE);
}

JNIEXPORT jboolean JNICALL 
Java_org_uoyabause_android_achievements_RetroAchievementsManager_isHardcoreEnabledNative(
    JNIEnv* env, jobject thiz) {
    
    YabauseRA::Integration* ra = YabauseRA::Integration::getInstance();
    return ra->isHardcoreEnabled() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL 
Java_org_uoyabause_android_achievements_RetroAchievementsManager_isProcessingRequiredNative(
    JNIEnv* env, jobject thiz) {
    
    YabauseRA::Integration* ra = YabauseRA::Integration::getInstance();
    return ra->isProcessingRequired() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL 
Java_org_uoyabause_android_achievements_RetroAchievementsManager_handleHttpResponseNative(
    JNIEnv* env, jobject thiz, jlong callback_data, jint http_code, jstring response) {
    
    std::string response_str = jstringToString(env, response);
    
    LOGD("HTTP response from manager: code=%d, %s, size=%zu", http_code, response_str.c_str(), response_str.size());

    struct CallbackInfo {
        rc_client_server_callback_t callback;
        void* callback_data;
    };
    CallbackInfo* info = reinterpret_cast<CallbackInfo*>(callback_data);
    
    if (info && info->callback) {
        rc_api_server_response_t server_response = {0};
        server_response.http_status_code = http_code;
        server_response.body = response_str.c_str();
        server_response.body_length = response_str.size();
        
        info->callback(&server_response, info->callback_data);
        delete info; // Clean up allocated callback info
    }
}

JNIEXPORT jboolean JNICALL 
Java_org_uoyabause_android_achievements_RetroAchievementsManager_isUserLoggedInNative(
    JNIEnv* env, jobject thiz) {
    
    YabauseRA::Integration* ra = YabauseRA::Integration::getInstance();
    if (!ra) {
        LOGE("RetroAchievements integration not available");
        return JNI_FALSE;
    }
    
    // Check if user is logged in by trying to get user info
    const rc_client_user_t* user = ra->getUserInfo();
    bool isLoggedIn = (user != nullptr);
    
    LOGD("Login status check: %s", isLoggedIn ? "logged in" : "not logged in");
    if (isLoggedIn && user) {
        LOGD("Logged in user: %s", user->username);
    }
    
    return isLoggedIn ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobjectArray JNICALL
Java_org_uoyabause_android_achievements_RetroAchievementsManager_getAchievementListNative(
    JNIEnv* env, jclass clazz) {
    
    LOGD("Getting achievement list from native code");
    
    // Get the RetroAchievements integration instance
    YabauseRA::Integration* ra = YabauseRA::Integration::getInstance();
    if (!ra) {
        LOGE("RetroAchievements integration not initialized");
        return env->NewObjectArray(0, env->FindClass("org/uoyabause/android/achievements/AchievementListFragment$AchievementItem"), nullptr);
    }
    
    // Get the client
    rc_client_t* client = ra->getClient();
    if (!client) {
        LOGE("RetroAchievements client not available");
        return env->NewObjectArray(0, env->FindClass("org/uoyabause/android/achievements/AchievementListFragment$AchievementItem"), nullptr);
    }
    
    // Create achievement list with categories grouped by lock state
    rc_client_achievement_list_t* list = rc_client_create_achievement_list(client,
        RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL,
        RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
    
    if (!list) {
        LOGD("No achievement list available");
        return env->NewObjectArray(0, env->FindClass("org/uoyabause/android/achievements/AchievementListFragment$AchievementItem"), nullptr);
    }
    
    // Count total achievements
    int totalAchievements = 0;
    for (int i = 0; i < list->num_buckets; i++) {
        totalAchievements += list->buckets[i].num_achievements;
    }
    
    LOGD("Found %d achievements in %d categories", totalAchievements, list->num_buckets);
    
    // Find the AchievementItem class
    jclass achievementItemClass = env->FindClass("org/uoyabause/android/achievements/AchievementListFragment$AchievementItem");
    if (!achievementItemClass) {
        LOGE("Could not find AchievementItem class");
        rc_client_destroy_achievement_list(list);
        return env->NewObjectArray(0, env->FindClass("java/lang/Object"), nullptr);
    }
    
    // Find the AchievementState enum
    jclass stateClass = env->FindClass("org/uoyabause/android/achievements/AchievementListFragment$AchievementState");
    jfieldID unlockedField = env->GetStaticFieldID(stateClass, "UNLOCKED", "Lorg/uoyabause/android/achievements/AchievementListFragment$AchievementState;");
    jfieldID lockedField = env->GetStaticFieldID(stateClass, "LOCKED", "Lorg/uoyabause/android/achievements/AchievementListFragment$AchievementState;");
    jfieldID unsupportedField = env->GetStaticFieldID(stateClass, "UNSUPPORTED", "Lorg/uoyabause/android/achievements/AchievementListFragment$AchievementState;");
    
    jobject unlockedState = env->GetStaticObjectField(stateClass, unlockedField);
    jobject lockedState = env->GetStaticObjectField(stateClass, lockedField);
    jobject unsupportedState = env->GetStaticObjectField(stateClass, unsupportedField);
    
    // Get the constructor for AchievementItem
    jmethodID constructor = env->GetMethodID(achievementItemClass, "<init>", 
        "(ILjava/lang/String;Ljava/lang/String;ILjava/lang/String;Lorg/uoyabause/android/achievements/AchievementListFragment$AchievementState;Ljava/lang/String;Ljava/lang/String;I)V");
    
    if (!constructor) {
        LOGE("Could not find AchievementItem constructor");
        rc_client_destroy_achievement_list(list);
        return env->NewObjectArray(0, env->FindClass("java/lang/Object"), nullptr);
    }
    
    // Create the result array
    jobjectArray result = env->NewObjectArray(totalAchievements, achievementItemClass, nullptr);
    int arrayIndex = 0;
    
    // Process each category
    for (int i = 0; i < list->num_buckets; i++) {
        // Get appropriate category label based on bucket type
        const char* categoryLabel;
        switch (list->buckets[i].bucket_type) {
            case RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED:
                categoryLabel = "Locked Achievements";
                break;
            case RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED:
                categoryLabel = "Unlocked Achievements";
                break;
            case RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED:
                categoryLabel = "Recently Unlocked";
                break;
            case RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE:
                categoryLabel = "Active Challenges";
                break;
            case RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE:
                categoryLabel = "Almost There";
                break;
            case RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL:
                categoryLabel = "Unofficial Achievements";
                break;
            case RC_CLIENT_ACHIEVEMENT_BUCKET_UNSYNCED:
                categoryLabel = "Unsynced Achievements";
                break;
            case RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED:
                categoryLabel = "Unsupported Achievements";
                break;
            default:
                categoryLabel = list->buckets[i].label ? list->buckets[i].label : "Achievements";
                break;
        }
        
        LOGD("Processing category: %s (bucket_type: %d, count: %d)", 
             categoryLabel, list->buckets[i].bucket_type, list->buckets[i].num_achievements);
        
        // Add debug information about achievements in this bucket
        int unlocked_count = 0;
        int locked_count = 0;
        for (int debug_j = 0; debug_j < list->buckets[i].num_achievements; debug_j++) {
            const rc_client_achievement_t* debug_achievement = list->buckets[i].achievements[debug_j];
            if (debug_achievement->unlocked != 0) {
                unlocked_count++;
            } else {
                locked_count++;
            }
        }
        LOGD("  -> Unlocked: %d, Locked: %d", unlocked_count, locked_count);
        
        // Process achievements in this category
        for (int j = 0; j < list->buckets[i].num_achievements; j++) {
            const rc_client_achievement_t* achievement = list->buckets[i].achievements[j];
            
            // Get achievement image URL
            char imageUrl[256] = {0};
            jstring imageUrlStr = nullptr;
            if (rc_client_achievement_get_image_url(achievement, achievement->state, imageUrl, sizeof(imageUrl)) == RC_OK) {
                imageUrlStr = env->NewStringUTF(imageUrl);
            }
            
            // Determine achievement state based on actual unlocked field
            jobject state;
            if (list->buckets[i].bucket_type == RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED) {
                // Unsupported achievements are always unsupported regardless of unlock status
                state = unsupportedState;
            } else if (achievement->unlocked != 0) {
                // RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE = 0, anything else means unlocked
                state = unlockedState;
                LOGD("Achievement %d (%s) is UNLOCKED: unlocked=%d, state=%d, unlock_time=%ld, bucket=%d", 
                     achievement->id, achievement->title, achievement->unlocked, 
                     achievement->state, (long)achievement->unlock_time, list->buckets[i].bucket_type);
            } else {
                // Not unlocked
                state = lockedState;
                LOGD("Achievement %d (%s) is LOCKED: unlocked=%d, state=%d, unlock_time=%ld, bucket=%d", 
                     achievement->id, achievement->title, achievement->unlocked, 
                     achievement->state, (long)achievement->unlock_time, list->buckets[i].bucket_type);
            }
            
            // Create progress string based on actual unlock status and bucket type
            const char* progress;
            if (list->buckets[i].bucket_type == RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED) {
                progress = "Unsupported";
            } else if (achievement->unlocked != 0) {
                // Achievement is unlocked - determine specific unlock type
                switch (list->buckets[i].bucket_type) {
                    case RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED:
                        progress = "Recently Unlocked";
                        break;
                    case RC_CLIENT_ACHIEVEMENT_BUCKET_UNSYNCED:
                        progress = "Unlocked (Not Synced)";
                        break;
                    default:
                        progress = "Unlocked";
                        break;
                }
            } else {
                // Achievement is locked - show progress or status based on bucket
                switch (list->buckets[i].bucket_type) {
                    case RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE:
                        if (achievement->measured_percent > 0) {
                            progress = achievement->measured_progress;
                        } else {
                            progress = "Active Challenge";
                        }
                        break;
                    case RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE:
                        if (achievement->measured_percent > 0) {
                            progress = achievement->measured_progress;
                        } else {
                            progress = "Almost There";
                        }
                        break;
                    case RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL:
                        progress = "Unofficial";
                        break;
                    case RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED:
                    default:
                        if (achievement->measured_percent > 0) {
                            progress = achievement->measured_progress;
                        } else {
                            progress = "Locked";
                        }
                        break;
                }
            }
            
            // Create Java strings
            jstring titleStr = env->NewStringUTF(achievement->title);
            jstring descStr = env->NewStringUTF(achievement->description);
            jstring categoryStr = env->NewStringUTF(categoryLabel);
            jstring progressStr = env->NewStringUTF(progress);
            
            // Handle different ranges of measured_percent
            int measuredPercentInt;
            if (achievement->measured_percent <= 1.0f) {
                // Range 0.0 - 1.0, convert to 0-100
                measuredPercentInt = (int)(achievement->measured_percent * 100.0f);
            } else {
                // Already in 0-100+ range, cap at 100
                measuredPercentInt = (int)achievement->measured_percent;
                if (measuredPercentInt > 100) {
                    measuredPercentInt = 100;
                }
            }
            
            // Create the AchievementItem object
            jobject achievementItem = env->NewObject(achievementItemClass, constructor,
                achievement->id,                    // id
                titleStr,                          // title
                descStr,                           // description
                achievement->points,               // points
                imageUrlStr,                       // badgeUrl
                state,                             // state
                categoryStr,                       // category
                progressStr,                       // progress
                measuredPercentInt                 // measuredPercent (converted to 0-100 range)
            );
            
            // Add to result array
            env->SetObjectArrayElement(result, arrayIndex++, achievementItem);
            
            // Clean up local references
            env->DeleteLocalRef(titleStr);
            env->DeleteLocalRef(descStr);
            env->DeleteLocalRef(categoryStr);
            env->DeleteLocalRef(progressStr);
            if (imageUrlStr) env->DeleteLocalRef(imageUrlStr);
            env->DeleteLocalRef(achievementItem);
        }
    }
    
    // Clean up
    rc_client_destroy_achievement_list(list);
    
    LOGD("Created achievement array with %d items", arrayIndex);
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_uoyabause_android_achievements_RetroAchievementsManager_changeMediaNative(
    JNIEnv* env, jclass clazz, jstring newMediaPath) {

    // Get the RetroAchievements integration instance
    YabauseRA::Integration* ra = YabauseRA::Integration::getInstance();
    if (!ra) {
        LOGE("RetroAchievements integration not initialized");
        return JNI_FALSE;
    }

    // Convert Java string to C string
    const char* mediaPath = env->GetStringUTFChars(newMediaPath, nullptr);
    if (!mediaPath) {
        LOGE("Failed to get media path string");
        return JNI_FALSE;
    }

    LOGD("Changing RetroAchievements media to: %s", mediaPath);

    // Call the integration's changeMedia method
    bool success = ra->changeMedia(mediaPath);

    // Release the string
    env->ReleaseStringUTFChars(newMediaPath, mediaPath);

    return success ? JNI_TRUE : JNI_FALSE;
}

// CHD CD reader implementation for rcheevos
namespace {

// CHD handle structure
struct chd_track_handle {
    chd_file* chd;
    uint8_t* hunk_buffer;
    uint32_t hunk_size;
    uint32_t current_hunk;
    uint32_t track_number;
    uint32_t sector_size;
};

// Open a CHD track
void* chd_open_track(const char* path, uint32_t track) {
    LOGD("chd_open_track: path=%s, track=%u", path, track);

    // Handle paths with fake extensions (e.g., /proc/self/fd/XX.chd)
    // Strip the extension if the path is a /proc/self/fd path
    std::string actualPath(path);
    if (actualPath.find("/proc/self/fd/") == 0) {
        size_t dotPos = actualPath.find_last_of('.');
        if (dotPos != std::string::npos) {
            actualPath = actualPath.substr(0, dotPos);
            LOGD("Stripped extension for fd path: %s", actualPath.c_str());
        }
    }

    // Open CHD file
    chd_file* chd = nullptr;
    chd_error err = chd_open(actualPath.c_str(), CHD_OPEN_READ, nullptr, &chd);
    if (err != CHDERR_NONE) {
        LOGE("chd_open failed: %d", err);
        return nullptr;
    }

    // Get header
    const chd_header* header = chd_get_header(chd);
    if (!header) {
        LOGE("chd_get_header failed");
        chd_close(chd);
        return nullptr;
    }

    LOGD("CHD header: version=%u, hunkbytes=%u, totalhunks=%u, logicalbytes=%llu, unitbytes=%u",
         header->version, header->hunkbytes, header->totalhunks, header->logicalbytes, header->unitbytes);

    // Allocate handle
    chd_track_handle* handle = new chd_track_handle();
    handle->chd = chd;
    handle->hunk_size = header->hunkbytes;
    handle->hunk_buffer = new uint8_t[handle->hunk_size];
    handle->current_hunk = (uint32_t)-1;
    handle->track_number = track;
    // CHD unitbytes should tell us the sector/frame size
    handle->sector_size = (header->unitbytes > 0) ? header->unitbytes : 2352;

    LOGD("CHD opened successfully: hunk_size=%u, sector_size=%u", handle->hunk_size, handle->sector_size);
    return handle;
}

// Close a CHD track
void chd_close_track(void* track_handle) {
    if (!track_handle) return;

    chd_track_handle* handle = (chd_track_handle*)track_handle;
    if (handle->chd) {
        chd_close(handle->chd);
    }
    if (handle->hunk_buffer) {
        delete[] handle->hunk_buffer;
    }
    delete handle;

    LOGD("CHD closed");
}

// Read a sector from CHD
size_t chd_read_sector(void* track_handle, uint32_t sector, void* buffer, size_t requested_bytes) {
    LOGD("chd_read_sector called: sector=%u, requested_bytes=%zu", sector, requested_bytes);

    if (!track_handle || !buffer) {
        LOGE("chd_read_sector: invalid parameters");
        return 0;
    }

    chd_track_handle* handle = (chd_track_handle*)track_handle;

    // Calculate which hunk contains this sector
    uint32_t sector_offset = sector * handle->sector_size;
    uint32_t hunk_id = sector_offset / handle->hunk_size;
    uint32_t offset_in_hunk = sector_offset % handle->hunk_size;

    // Read hunk if not already cached
    if (handle->current_hunk != hunk_id) {
        chd_error err = chd_read(handle->chd, hunk_id, handle->hunk_buffer);
        if (err != CHDERR_NONE) {
            LOGE("chd_read failed for hunk %u: %d", hunk_id, err);
            return 0;
        }
        handle->current_hunk = hunk_id;
    }

    // CD-ROM sectors have a 16-byte header (sync pattern + address)
    // User data starts at offset 16 within each sector
    // Skip the CD-ROM header to get actual user data
    const uint32_t CD_HEADER_SIZE = 16;
    uint8_t* sector_data = handle->hunk_buffer + offset_in_hunk;

    // Copy requested bytes from user data area (skip CD header)
    size_t bytes_to_copy = (requested_bytes < handle->sector_size - CD_HEADER_SIZE) ?
                           requested_bytes : (handle->sector_size - CD_HEADER_SIZE);
    memcpy(buffer, sector_data + CD_HEADER_SIZE, bytes_to_copy);

    // Log first 16 bytes for debugging
    uint8_t* data = (uint8_t*)buffer;
    LOGD("chd_read_sector: read %zu bytes, first 16: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
         bytes_to_copy,
         data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
         data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);

    return bytes_to_copy;
}

// Get first track sector (always 0 for CHD)
uint32_t chd_first_track_sector(void* track_handle) {
    return 0;
}

// Initialize CHD callbacks for rcheevos
void init_chd_callbacks() {
    static rc_hash_cdreader_t chd_reader = {
        chd_open_track,
        chd_read_sector,
        chd_close_track,
        chd_first_track_sector
    };

    rc_hash_init_custom_cdreader(&chd_reader);
    LOGD("CHD callbacks initialized");
}

} // anonymous namespace

JNIEXPORT jstring JNICALL
Java_org_uoyabause_android_achievements_RetroAchievementsManager_generateGameHashNative(
    JNIEnv* env, jobject thiz, jstring gamePath) {

    if (!gamePath) {
        LOGE("gamePath is null");
        return nullptr;
    }

    // Convert Java string to C string
    const char* path = env->GetStringUTFChars(gamePath, nullptr);
    if (!path) {
        LOGE("Failed to get game path string");
        return nullptr;
    }

    LOGD("Generating hash for game: %s", path);

    // Initialize CHD callbacks (idempotent)
    init_chd_callbacks();

    // Generate hash using rcheevos library
    char hash[33];
    int result = rc_hash_generate_from_file(hash, RC_CONSOLE_SATURN, path);

    // Release the string
    env->ReleaseStringUTFChars(gamePath, path);

    if (result) {
        // Hash generated successfully
        LOGD("Generated hash: %s", hash);
        return env->NewStringUTF(hash);
    } else {
        LOGE("Failed to generate hash for: %s", path);
        return nullptr;
    }
}

// Test function for mastery notification - can be called from C++ code
extern "C" void testMasteryNotification() {
    LOGD("testMasteryNotification called");
    
    JNIEnv* env = getJNIEnv();
    if (!env || !g_callback_obj || !g_on_game_mastery) {
        LOGE("Failed to get JNI environment or mastery callback not available");
        return;
    }
    
    // Create test mastery data
    const char* test_game_title = "Test Saturn Game";
    const char* test_image_url = "https://retroachievements.org/Images/game_badges/12345.png";
    int test_achievement_count = 42;
    int test_total_points = 1337;
    bool test_is_hardcore = true;
    const char* test_username = "TestUser";
    const char* test_playtime = "5h 30m";
    
    // Create Java strings
    jstring gameTitle = env->NewStringUTF(test_game_title);
    jstring imageUrl = env->NewStringUTF(test_image_url);
    jstring usernameStr = env->NewStringUTF(test_username);
    jstring playtimeStr = env->NewStringUTF(test_playtime);
    
    LOGD("Calling Java mastery callback with test data");
    
    // Call the Java callback method
    env->CallVoidMethod(g_callback_obj, g_on_game_mastery,
                       gameTitle, imageUrl, test_achievement_count, test_total_points, 
                       test_is_hardcore, usernameStr, playtimeStr);
    
    // Check for JNI exceptions
    if (env->ExceptionCheck()) {
        LOGE("Exception occurred in test mastery callback");
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
    
    // Clean up local references
    env->DeleteLocalRef(gameTitle);
    env->DeleteLocalRef(imageUrl);
    env->DeleteLocalRef(usernameStr);
    env->DeleteLocalRef(playtimeStr);
    
    LOGD("Test mastery notification completed");
}

/**
 * Get user statistics from RetroAchievements
 * Returns an int array with [score, score_softcore, num_unread_messages]
 */
JNIEXPORT jintArray JNICALL
Java_org_uoyabause_android_achievements_RetroAchievementsManager_getUserStatsNative(
    JNIEnv* env, jclass clazz) {
    
    LOGD("Getting user stats from native layer");
    
    // Get the RetroAchievements integration instance
    YabauseRA::Integration* ra = YabauseRA::Integration::getInstance();
    if (!ra) {
        LOGE("RetroAchievements integration not initialized");
        return nullptr;
    }
    
    // Get user info
    const rc_client_user_t* user = ra->getUserInfo();
    if (!user) {
        LOGD("No user logged in");
        return nullptr;
    }
    
    LOGD("User stats: username=%s, score=%d, score_softcore=%d, num_unread_messages=%d", 
         user->username, user->score, user->score_softcore, user->num_unread_messages);
    
    // Create Java int array with user stats  
    jintArray result = env->NewIntArray(3);
    if (result == nullptr) {
        LOGE("Failed to create int array");
        return nullptr;
    }
    
    jint stats[3];
    stats[0] = user->score;               // Hardcore score
    stats[1] = user->score_softcore;      // Softcore score  
    stats[2] = user->num_unread_messages; // Unread messages
    
    env->SetIntArrayRegion(result, 0, 3, stats);
    
    return result;
}

/**
 * Get user avatar URL from RetroAchievements
 * Returns a string with the avatar URL
 */
JNIEXPORT jstring JNICALL
Java_org_uoyabause_android_achievements_RetroAchievementsManager_getUserAvatarUrlNative(
    JNIEnv* env, jclass clazz) {
    
    LOGD("Getting user avatar URL from native layer");
    
    // Get the RetroAchievements integration instance
    YabauseRA::Integration* ra = YabauseRA::Integration::getInstance();
    if (!ra) {
        LOGE("RetroAchievements integration not initialized");
        return nullptr;
    }
    
    // Get user info
    const rc_client_user_t* user = ra->getUserInfo();
    if (!user) {
        LOGD("No user logged in");
        return nullptr;
    }
    
    // Check if avatar_url is available
    if (!user->avatar_url || strlen(user->avatar_url) == 0) {
        LOGD("No avatar URL available for user");
        return nullptr;
    }
    
    LOGD("User avatar URL: %s", user->avatar_url);

    // Create Java string with avatar URL
    return env->NewStringUTF(user->avatar_url);
}

/**
 * Get user API token (Web API key) from RetroAchievements
 * Returns a string with the API token for HTTP API calls
 */
JNIEXPORT jstring JNICALL
Java_org_uoyabause_android_achievements_RetroAchievementsManager_getUserApiTokenNative(
    JNIEnv* env, jclass clazz) {

    LOGD("Getting user API token from native layer");

    // Get the RetroAchievements integration instance
    YabauseRA::Integration* ra = YabauseRA::Integration::getInstance();
    if (!ra) {
        LOGE("RetroAchievements integration not initialized");
        return nullptr;
    }

    // Get user info
    const rc_client_user_t* user = ra->getUserInfo();
    if (!user) {
        LOGD("No user logged in");
        return nullptr;
    }

    // Check if token is available
    if (!user->token || strlen(user->token) == 0) {
        LOGD("No API token available for user");
        return nullptr;
    }

    LOGD("User API token retrieved (length: %zu chars)", strlen(user->token));

    // Create Java string with API token
    return env->NewStringUTF(user->token);
}

/**
 * Fetch all user progress for Sega Saturn games
 * Returns a JSON string with all game progress data
 */
JNIEXPORT jstring JNICALL
Java_org_uoyabause_android_achievements_RetroAchievementsManager_fetchAllUserProgressNative(
    JNIEnv* env, jclass clazz) {

    LOGD("Fetching all user progress from native layer");

    YabauseRA::Integration* ra = YabauseRA::Integration::getInstance();
    if (!ra) {
        LOGE("RetroAchievements integration not initialized");
        return nullptr;
    }

    // Fetch all user progress for Sega Saturn (console ID 39)
    // This is an async operation that will call back when complete
    // For now, return null to indicate async operation started
    // We'll need to implement a callback mechanism to return the data

    LOGD("All user progress fetch not yet implemented - requires async callback");
    return nullptr;
}

} // extern "C"