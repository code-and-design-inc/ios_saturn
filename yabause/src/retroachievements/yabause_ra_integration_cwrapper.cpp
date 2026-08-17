/*
YabaSanshiro RetroAchievements Integration C Wrapper Implementation
Copyright 2025 YabaSanshiro Team

This file provides C-compatible wrapper functions for RetroAchievements integration.
For use in C code that needs to interface with the C++ RetroAchievements implementation.
*/

#include "yabause_ra_integration_cwrapper.h"
#include "yabause_ra_integration.h"
#include <cstring>
#include <cstdio>

// Include rcheevos headers for complete type definitions
#include "rcheevos/include/rc_api_request.h"
#include "rcheevos/include/rc_client.h"

extern "C" {

// Global variables for callbacks
static YabauseRA_ServerCallback g_server_callback = nullptr;
static YabauseRA_EventCallback g_event_callback = nullptr;
static YabauseRA_LoginCallback g_login_callback = nullptr;
static YabauseRA_GamePlacardCallback g_game_placard_callback = nullptr;

// Structure to store pending HTTP request context
struct PendingHTTPRequest {
    rc_client_server_callback_t callback;
    void* callback_data;
};

// Server callback wrapper
static void server_callback_wrapper(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callback_data) {
    printf("[DEBUG] Server callback wrapper called - URL: %s\n", request->url ? request->url : "NULL");
    if (g_server_callback) {
        printf("[DEBUG] Calling Swift HTTP handler\n");
        // Create a context for this HTTP request
        PendingHTTPRequest* context = new PendingHTTPRequest();
        context->callback = callback;
        context->callback_data = callback_data;
        
        // Call the iOS HTTP handler with the context as userdata
        g_server_callback(request->url, request->post_data, context);
    } else {
        printf("[DEBUG] No server callback set, completing with error\n");
        // No server callback set, complete with error
        rc_api_server_response_t response = {0};
        response.http_status_code = 0; // Error status
        callback(&response, callback_data);
    }
}

// Event callback wrapper  
static void event_callback_wrapper(const rc_client_event_t* event) {
    if (g_event_callback) {
        // For Achievement Triggered event, send JSON with complete achievement data
        if (event->type == RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED && event->achievement) {
            char* json_str = YabauseRA_CreateAchievementTriggeredJSON(event->achievement);
            if (json_str) {
                printf("[DEBUG] Sending Achievement Triggered JSON to Swift: %s\n", json_str);
                g_event_callback(event->type, json_str);
                free(json_str);
            } else {
                printf("[DEBUG] Failed to create Achievement Triggered JSON, using fallback\n");
                g_event_callback(event->type, "Achievement event");
            }
        }
        // For Challenge Indicator events, send JSON with complete achievement data
        else if ((event->type == RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW ||
                  event->type == RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE) &&
                 event->achievement) {
            
            char* json_str = YabauseRA_CreateChallengeIndicatorJSON(event->achievement);
            if (json_str) {
                printf("[DEBUG] Sending Challenge Indicator JSON to Swift: %s\n", json_str);
                g_event_callback(event->type, json_str);
                free(json_str);
            } else {
                printf("[DEBUG] Failed to create Challenge Indicator JSON, using fallback\n");
                g_event_callback(event->type, "Achievement event");
            }
        }
        // For Leaderboard Tracker events, send JSON with complete tracker data
        else if ((event->type == RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW ||
                  event->type == RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE ||
                  event->type == RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE) &&
                 event->leaderboard_tracker) {
            
            char* json_str = YabauseRA_CreateLeaderboardTrackerJSON(event->leaderboard_tracker);
            if (json_str) {
                printf("[DEBUG] Sending Leaderboard Tracker JSON to Swift: %s\n", json_str);
                g_event_callback(event->type, json_str);
                free(json_str);
            } else {
                printf("[DEBUG] Failed to create Leaderboard Tracker JSON, using fallback\n");
                g_event_callback(event->type, "Achievement event");
            }
        }
        // For Progress Indicator events, send JSON with complete achievement data
        else if ((event->type == RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW ||
                  event->type == RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE) &&
                 event->achievement) {
            
            char* json_str = YabauseRA_CreateProgressIndicatorJSON(event->achievement);
            if (json_str) {
                printf("[DEBUG] Sending Progress Indicator JSON to Swift: %s\n", json_str);
                g_event_callback(event->type, json_str);
                free(json_str);
            } else {
                printf("[DEBUG] Failed to create Progress Indicator JSON, using fallback\n");
                g_event_callback(event->type, "Achievement event");
            }
        }
        // For Progress Indicator hide event (no achievement data)
        else if (event->type == RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE) {
            // HIDE event doesn't contain achievement data, send simple JSON
            const char* hide_json = "{\"action\":\"hide\"}";
            printf("[DEBUG] Sending Progress Indicator Hide JSON to Swift: %s\n", hide_json);
            g_event_callback(event->type, hide_json);
        }
        // For Leaderboard Scoreboard events, create JSON with scoreboard data
        else if (event->type == RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD && 
                 event->leaderboard_scoreboard && event->leaderboard) {
            
            char* json_str = YabauseRA_CreateLeaderboardScoreboardJSON(event->leaderboard_scoreboard, event->leaderboard);
            if (json_str) {
                printf("[DEBUG] Sending Leaderboard Scoreboard JSON to Swift: %s\n", json_str);
                g_event_callback(event->type, json_str);
                free(json_str);
            } else {
                printf("[DEBUG] Failed to create Leaderboard Scoreboard JSON, using fallback\n");
                g_event_callback(event->type, "Achievement event");
            }
        } else {
            // For other events, use generic message
            g_event_callback(event->type, "Achievement event");
        }
    }
}

// Login callback wrapper
static void login_callback_wrapper(bool success, const char* username, const char* display_name, uint32_t score, const char* error_message) {
    printf("[DEBUG] Login callback wrapper called - Success: %s, Username: %s\n", success ? "true" : "false", username ? username : "NULL");
    if (g_login_callback) {
        printf("[DEBUG] Calling Swift login callback\n");
        g_login_callback(success ? 1 : 0, username, display_name, score, error_message);
    } else {
        printf("[DEBUG] No login callback set\n");
    }
}

// Game placard callback wrapper
static void game_placard_callback_wrapper(const char* game_title, const char* image_url, uint32_t unlocked_achievements, uint32_t total_achievements, uint32_t unlocked_points, uint32_t total_points, bool has_unsupported) {
    printf("[DEBUG] Game placard callback wrapper called - Title: %s, Achievements: %u/%u\n", 
           game_title ? game_title : "NULL", unlocked_achievements, total_achievements);
    if (g_game_placard_callback) {
        printf("[DEBUG] Calling Swift game placard callback\n");
        g_game_placard_callback(game_title, image_url, unlocked_achievements, total_achievements, 
                                unlocked_points, total_points, has_unsupported ? 1 : 0);
    } else {
        printf("[DEBUG] No game placard callback set\n");
    }
}

size_t YabauseRA_GetProgressSize(void) {
    auto* integration = YabauseRA::Integration::getInstance();
    if (!integration || !integration->isInitialized()) {
        return 0;
    }
    return integration->getProgressSize();
}

int YabauseRA_SerializeProgress(uint8_t* buffer, size_t buffer_size) {
    auto* integration = YabauseRA::Integration::getInstance();
    if (!integration || !integration->isInitialized()) {
        return 0;
    }
    return integration->serializeProgress(buffer, buffer_size) ? 1 : 0;
}

int YabauseRA_DeserializeProgress(const uint8_t* buffer, size_t buffer_size) {
    auto* integration = YabauseRA::Integration::getInstance();
    if (!integration || !integration->isInitialized()) {
        return 0;
    }
    return integration->deserializeProgress(buffer, buffer_size) ? 1 : 0;
}

int YabauseRA_Initialize(void) {
    auto* integration = YabauseRA::Integration::getInstance();
    if (!integration) {
        return 0;
    }
    
    bool result = integration->initialize();
    if (result) {
        // Set callbacks that were registered before initialization
        if (g_server_callback) {
            integration->setServerCallback(server_callback_wrapper);
        }
        if (g_event_callback) {
            integration->setEventCallback(event_callback_wrapper);
        }
        if (g_login_callback) {
            integration->setLoginCallback(login_callback_wrapper);
        }
        if (g_game_placard_callback) {
            integration->setGamePlacardCallback(game_placard_callback_wrapper);
        }
    }
    
    return result ? 1 : 0;
}

void YabauseRA_Shutdown(void) {
    auto* integration = YabauseRA::Integration::getInstance();
    if (integration) {
        integration->shutdown();
    }
    YabauseRA::Integration::destroyInstance();
}

int YabauseRA_InitializeUser(const char* username, const char* token) {
    auto* integration = YabauseRA::Integration::getInstance();
    if (!integration || !integration->isInitialized()) {
        return 0;
    }
    return integration->loginUser(username, token) ? 1 : 0;
}

void YabauseRA_Logout(void) {
    auto* integration = YabauseRA::Integration::getInstance();
    if (integration && integration->isInitialized()) {
        // Logout functionality would be implemented in Integration class
        // For now, we can shutdown and reinitialize
        integration->shutdown();
        bool result = integration->initialize();
        
        if (result) {
            // Restore callbacks after reinitialization
            if (g_server_callback) {
                printf("[DEBUG] C Wrapper: Restoring server callback after logout\n");
                integration->setServerCallback(server_callback_wrapper);
            }
            if (g_event_callback) {
                printf("[DEBUG] C Wrapper: Restoring event callback after logout\n");
                integration->setEventCallback(event_callback_wrapper);
            }
            if (g_login_callback) {
                printf("[DEBUG] C Wrapper: Restoring login callback after logout\n");
                integration->setLoginCallback(login_callback_wrapper);
            }
            if (g_game_placard_callback) {
                printf("[DEBUG] C Wrapper: Restoring game placard callback after logout\n");
                integration->setGamePlacardCallback(game_placard_callback_wrapper);
            }
        }
    }
}

int YabauseRA_IsUserLoggedIn(void) {
    auto* integration = YabauseRA::Integration::getInstance();
    if (!integration || !integration->isInitialized()) {
        return 0;
    }
    const auto* user_info = integration->getUserInfo();
    return (user_info != nullptr) ? 1 : 0;
}

const char* YabauseRA_GetUsername(void) {
    auto* integration = YabauseRA::Integration::getInstance();
    if (!integration || !integration->isInitialized()) {
        return nullptr;
    }
    const auto* user_info = integration->getUserInfo();
    return user_info ? user_info->username : nullptr;
}

void YabauseRA_SetHardcoreEnabled(int enabled) {
    auto* integration = YabauseRA::Integration::getInstance();
    if (integration && integration->isInitialized()) {
        integration->setHardcoreEnabled(enabled != 0);
    }
}

int YabauseRA_GetHardcoreEnabled(void) {
    auto* integration = YabauseRA::Integration::getInstance();
    if (!integration || !integration->isInitialized()) {
        return 0;
    }
    return integration->isHardcoreEnabled() ? 1 : 0;
}

void YabauseRA_Reset(void) {
    auto* integration = YabauseRA::Integration::getInstance();
    if (integration && integration->isInitialized()) {
        // Reset emulator first
        extern int YabauseReset(void);
        YabauseReset();
        
        // Notify RetroAchievements runtime that reset is complete
        integration->reset();
    }
}

int YabauseRA_IsProcessingRequired(void) {
    auto* integration = YabauseRA::Integration::getInstance();
    if (!integration || !integration->isInitialized()) {
        return 0;
    }
    return integration->isProcessingRequired() ? 1 : 0;
}

int YabauseRA_LoadGame(const char* game_hash) {
    auto* integration = YabauseRA::Integration::getInstance();
    if (!integration || !integration->isInitialized()) {
        return 0;
    }
    // TODO: Add hash-based game loading to Integration class
    // For now, this is not implemented as rcheevos primarily works with file paths
    return 0;
}

int YabauseRA_LoadGameFromFile(const char* game_path) {
    auto* integration = YabauseRA::Integration::getInstance();
    if (!integration || !integration->isInitialized()) {
        return 0;
    }
    return integration->loadGameFromPath(game_path) ? 1 : 0;
}

const char* YabauseRA_GetGameHash(void) {
    // This would need to be implemented in the Integration class
    // For now, return placeholder
    return "placeholder_hash";
}

void YabauseRA_UpdateRichPresence(const char* rich_presence) {
    // Rich presence updates would be handled automatically by rc_client
    // during doFrame() calls based on game state
}

const char* YabauseRA_GetRichPresenceDisplayString(void) {
    // This would need to be implemented in the Integration class
    // For now, return placeholder
    return nullptr;
}

void YabauseRA_OnAchievementTriggered(int achievement_id) {
    // Achievement triggering is handled automatically by rc_client
    // during doFrame() calls when conditions are met
}

void YabauseRA_OnLeaderboardSubmitted(int leaderboard_id, int score) {
    // Leaderboard submission is handled automatically by rc_client
    // during doFrame() calls when conditions are met
}

uint32_t YabauseRA_ReadMemory(uint32_t address, uint32_t size) {
    auto* integration = YabauseRA::Integration::getInstance();
    if (!integration || !integration->isInitialized()) {
        return 0;
    }
    
    uint8_t buffer[4] = {0};
    uint32_t bytes_read = integration->readMemory(address, buffer, size);
    
    if (bytes_read == 0) {
        return 0;
    }
    
    // Convert bytes to value based on size
    switch (size) {
        case 1:
            return buffer[0];
        case 2:
            return (buffer[1] << 8) | buffer[0];
        case 4:
            return (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0];
        default:
            return 0;
    }
}

void YabauseRA_DoFrame(void) {
    auto* integration = YabauseRA::Integration::getInstance();
    if (integration && integration->isInitialized()) {
        integration->doFrame();
    }
}

int YabauseRA_BeginChangeMedia(const char* new_media_path) {
    auto* integration = YabauseRA::Integration::getInstance();
    if (integration && integration->isInitialized()) {
        return integration->beginChangeMedia(new_media_path) ? 1 : 0;
    }
    return 0;
}

int YabauseRA_GetGameImageURL(char* buffer, size_t buffer_size) {
    auto* integration = YabauseRA::Integration::getInstance();
    if (integration && integration->isInitialized()) {
        return integration->getGameImageURL(buffer, buffer_size) ? 1 : 0;
    }
    return 0;
}

void YabauseRA_SetServerCallback(YabauseRA_ServerCallback callback) {
    printf("[DEBUG] C Wrapper: Setting server callback: %p\n", (void*)callback);
    g_server_callback = callback;
    auto* integration = YabauseRA::Integration::getInstance();
    if (integration && integration->isInitialized()) {
        printf("[DEBUG] C Wrapper: Setting server callback on integration\n");
        integration->setServerCallback(server_callback_wrapper);
    } else {
        printf("[DEBUG] C Wrapper: Integration not initialized yet\n");
    }
}

void YabauseRA_SetEventCallback(YabauseRA_EventCallback callback) {
    g_event_callback = callback;
    auto* integration = YabauseRA::Integration::getInstance();
    if (integration && integration->isInitialized()) {
        integration->setEventCallback(event_callback_wrapper);
    }
}

void YabauseRA_SetLoginCallback(YabauseRA_LoginCallback callback) {
    printf("[DEBUG] C Wrapper: Setting login callback: %p\n", (void*)callback);
    g_login_callback = callback;
    auto* integration = YabauseRA::Integration::getInstance();
    if (integration && integration->isInitialized()) {
        printf("[DEBUG] C Wrapper: Setting login callback on integration\n");
        integration->setLoginCallback(login_callback_wrapper);
    } else {
        printf("[DEBUG] C Wrapper: Integration not initialized yet\n");
    }
}

void YabauseRA_SetGamePlacardCallback(YabauseRA_GamePlacardCallback callback) {
    printf("[DEBUG] C Wrapper: Setting game placard callback: %p\n", (void*)callback);
    g_game_placard_callback = callback;
    auto* integration = YabauseRA::Integration::getInstance();
    if (integration && integration->isInitialized()) {
        printf("[DEBUG] C Wrapper: Setting game placard callback on integration\n");
        integration->setGamePlacardCallback(game_placard_callback_wrapper);
    } else {
        printf("[DEBUG] C Wrapper: Integration not initialized yet\n");
    }
}

void YabauseRA_SetPaused(int paused) {
    auto* integration = YabauseRA::Integration::getInstance();
    if (integration && integration->isInitialized()) {
        integration->setPaused(paused != 0);
    }
}

void YabauseRA_CompleteServerRequest(void* userdata, int http_status_code, const char* response_body, size_t response_length) {
    if (!userdata) {
        return; // Invalid context
    }
    
    // Cast userdata back to our context structure
    PendingHTTPRequest* context = static_cast<PendingHTTPRequest*>(userdata);
    
    // Create rc_api_server_response_t structure
    rc_api_server_response_t response = {0};
    response.http_status_code = http_status_code;
    response.body = response_body;
    response.body_length = response_length;
    
    // Call the original rcheevos callback with the response
    if (context->callback) {
        context->callback(&response, context->callback_data);
    }
    
    // Clean up the context
    delete context;
}

const char* YabauseRA_GetAchievementBadgeURL(int achievementId, int state) {
    auto* integration = YabauseRA::Integration::getInstance();
    if (!integration || !integration->isInitialized()) {
        return nullptr;
    }
    
    // Get the current game's achievement list
    return integration->getAchievementBadgeURL(achievementId, state);
}

const char* YabauseRA_GetAchievementBadgeName(int achievementId) {
    auto* integration = YabauseRA::Integration::getInstance();
    if (!integration || !integration->isInitialized()) {
        return nullptr;
    }
    
    // Get the achievement badge name for local caching
    return integration->getAchievementBadgeName(achievementId);
}

// Removed dangerous struct-based function - use YabauseRA_CreateAchievementListJSON instead

char* YabauseRA_CreateAchievementListJSON(int category, int grouping) {
    auto* integration = YabauseRA::Integration::getInstance();
    if (!integration || !integration->isInitialized()) {
        return nullptr;
    }
    
    return integration->createAchievementListJSON(category, grouping);
}

char* YabauseRA_CreateAchievementTriggeredJSON(const void* achievement) {
    if (!achievement) {
        return nullptr;
    }
    
    const rc_client_achievement_t* ach = static_cast<const rc_client_achievement_t*>(achievement);
    
    // Get badge image URL for unlocked state
    char badge_url[256] = {0};
    if (rc_client_achievement_get_image_url(ach, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED, badge_url, sizeof(badge_url)) != RC_OK) {
        // Fallback to active state if unlocked URL not available
        rc_client_achievement_get_image_url(ach, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE, badge_url, sizeof(badge_url));
    }
    
    // Determine if unofficial
    bool is_unofficial = (ach->category == RC_CLIENT_ACHIEVEMENT_CATEGORY_UNOFFICIAL);
    
    // Create JSON string
    // Calculate required buffer size
    size_t title_len = ach->title ? strlen(ach->title) : 0;
    size_t desc_len = ach->description ? strlen(ach->description) : 0;
    size_t badge_url_len = strlen(badge_url);
    size_t badge_name_len = ach->badge_name ? strlen(ach->badge_name) : 0;
    
    // Calculate JSON size (with escape characters and structure)
    size_t json_size = 512 + (title_len * 2) + (desc_len * 2) + badge_url_len + badge_name_len;
    
    char* json = (char*)malloc(json_size);
    if (!json) {
        return nullptr;
    }
    
    // Helper function to escape JSON string
    auto escape_json_string = [](const char* src, char* dst, size_t dst_size) {
        if (!src || !dst || dst_size < 3) return;
        
        size_t src_len = strlen(src);
        size_t dst_pos = 0;
        
        for (size_t i = 0; i < src_len && dst_pos < dst_size - 2; i++) {
            char c = src[i];
            if (c == '"' || c == '\\') {
                if (dst_pos < dst_size - 3) {
                    dst[dst_pos++] = '\\';
                    dst[dst_pos++] = c;
                }
            } else if (c == '\n') {
                if (dst_pos < dst_size - 3) {
                    dst[dst_pos++] = '\\';
                    dst[dst_pos++] = 'n';
                }
            } else if (c == '\r') {
                if (dst_pos < dst_size - 3) {
                    dst[dst_pos++] = '\\';
                    dst[dst_pos++] = 'r';
                }
            } else if (c == '\t') {
                if (dst_pos < dst_size - 3) {
                    dst[dst_pos++] = '\\';
                    dst[dst_pos++] = 't';
                }
            } else {
                dst[dst_pos++] = c;
            }
        }
        dst[dst_pos] = '\0';
    };
    
    // Prepare escaped strings
    char* escaped_title = (char*)malloc(title_len * 2 + 1);
    char* escaped_desc = (char*)malloc(desc_len * 2 + 1);
    
    if (escaped_title && escaped_desc) {
        escaped_title[0] = '\0';
        escaped_desc[0] = '\0';
        
        if (ach->title) escape_json_string(ach->title, escaped_title, title_len * 2 + 1);
        if (ach->description) escape_json_string(ach->description, escaped_desc, desc_len * 2 + 1);
        
        // Build JSON with additional fields for triggered achievement
        snprintf(json, json_size,
            "{"
            "\"id\":%u,"
            "\"title\":\"%s\","
            "\"description\":\"%s\","
            "\"badge\":\"%s\","
            "\"badge_url\":\"%s\","
            "\"points\":%u,"
            "\"state\":%u,"
            "\"is_unofficial\":%s,"
            "\"unlocked_time\":%u,"
            "\"category\":%u"
            "}",
            ach->id,
            escaped_title,
            escaped_desc,
            ach->badge_name ? ach->badge_name : "",
            badge_url,
            ach->points,
            ach->state,
            is_unofficial ? "true" : "false",
            ach->unlocked,
            ach->category
        );
    } else {
        // Fallback without escaping if malloc failed
        snprintf(json, json_size,
            "{"
            "\"id\":%u,"
            "\"title\":\"%s\","
            "\"description\":\"%s\","
            "\"badge\":\"%s\","
            "\"badge_url\":\"%s\","
            "\"points\":%u,"
            "\"state\":%u,"
            "\"is_unofficial\":%s,"
            "\"unlocked_time\":%u,"
            "\"category\":%u"
            "}",
            ach->id,
            ach->title ? ach->title : "",
            ach->description ? ach->description : "",
            ach->badge_name ? ach->badge_name : "",
            badge_url,
            ach->points,
            ach->state,
            is_unofficial ? "true" : "false",
            ach->unlocked,
            ach->category
        );
    }
    
    // Clean up
    if (escaped_title) free(escaped_title);
    if (escaped_desc) free(escaped_desc);
    
    return json;
}

char* YabauseRA_CreateChallengeIndicatorJSON(const void* achievement) {
    if (!achievement) {
        return nullptr;
    }
    
    const rc_client_achievement_t* ach = static_cast<const rc_client_achievement_t*>(achievement);
    
    // Get badge image URL
    char badge_url[256] = {0};
    if (rc_client_achievement_get_image_url(ach, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED, badge_url, sizeof(badge_url)) != RC_OK) {
        // Fallback to locked state if unlocked URL not available
        //rc_client_achievement_get_image_url(ach, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED, badge_url, sizeof(badge_url));
    }
    
    // Create JSON string
    // Calculate required buffer size
    size_t title_len = ach->title ? strlen(ach->title) : 0;
    size_t desc_len = ach->description ? strlen(ach->description) : 0;
    size_t badge_url_len = strlen(badge_url);
    size_t badge_name_len = ach->badge_name ? strlen(ach->badge_name) : 0;
    
    // Calculate JSON size (with escape characters and structure)
    size_t json_size = 512 + (title_len * 2) + (desc_len * 2) + badge_url_len + badge_name_len;
    
    char* json = (char*)malloc(json_size);
    if (!json) {
        return nullptr;
    }
    
    // Helper function to escape JSON string
    auto escape_json_string = [](const char* src, char* dst, size_t dst_size) {
        if (!src || !dst || dst_size < 3) return;
        
        size_t src_len = strlen(src);
        size_t dst_pos = 0;
        
        for (size_t i = 0; i < src_len && dst_pos < dst_size - 2; i++) {
            char c = src[i];
            if (c == '"' || c == '\\') {
                if (dst_pos < dst_size - 3) {
                    dst[dst_pos++] = '\\';
                    dst[dst_pos++] = c;
                }
            } else if (c == '\n') {
                if (dst_pos < dst_size - 3) {
                    dst[dst_pos++] = '\\';
                    dst[dst_pos++] = 'n';
                }
            } else if (c == '\r') {
                if (dst_pos < dst_size - 3) {
                    dst[dst_pos++] = '\\';
                    dst[dst_pos++] = 'r';
                }
            } else if (c == '\t') {
                if (dst_pos < dst_size - 3) {
                    dst[dst_pos++] = '\\';
                    dst[dst_pos++] = 't';
                }
            } else {
                dst[dst_pos++] = c;
            }
        }
        dst[dst_pos] = '\0';
    };
    
    // Prepare escaped strings
    char* escaped_title = (char*)malloc(title_len * 2 + 1);
    char* escaped_desc = (char*)malloc(desc_len * 2 + 1);
    
    if (escaped_title && escaped_desc) {
        escaped_title[0] = '\0';
        escaped_desc[0] = '\0';
        
        if (ach->title) escape_json_string(ach->title, escaped_title, title_len * 2 + 1);
        if (ach->description) escape_json_string(ach->description, escaped_desc, desc_len * 2 + 1);
        
        // Build JSON
        snprintf(json, json_size,
            "{"
            "\"id\":%u,"
            "\"title\":\"%s\","
            "\"description\":\"%s\","
            "\"badge\":\"%s\","
            "\"badge_url\":\"%s\","
            "\"points\":%u,"
            "\"state\":%u"
            "}",
            ach->id,
            escaped_title,
            escaped_desc,
            ach->badge_name ? ach->badge_name : "",
            badge_url,
            ach->points,
            ach->state
        );
    } else {
        // Fallback without escaping if malloc failed
        snprintf(json, json_size,
            "{"
            "\"id\":%u,"
            "\"title\":\"%s\","
            "\"description\":\"%s\","
            "\"badge\":\"%s\","
            "\"badge_url\":\"%s\","
            "\"points\":%u,"
            "\"state\":%u"
            "}",
            ach->id,
            ach->title ? ach->title : "",
            ach->description ? ach->description : "",
            ach->badge_name ? ach->badge_name : "",
            badge_url,
            ach->points,
            ach->state
        );
    }
    
    // Clean up
    if (escaped_title) free(escaped_title);
    if (escaped_desc) free(escaped_desc);
    
    return json;
}

char* YabauseRA_CreateLeaderboardTrackerJSON(const void* leaderboard_tracker) {
    if (!leaderboard_tracker) {
        return nullptr;
    }
    
    const rc_client_leaderboard_tracker_t* tracker = static_cast<const rc_client_leaderboard_tracker_t*>(leaderboard_tracker);
    
    // Create JSON string
    // Calculate required buffer size
    size_t display_len = tracker->display ? strlen(tracker->display) : 0;
    
    // Calculate JSON size (with escape characters and structure)
    size_t json_size = 256 + (display_len * 2);
    
    char* json = (char*)malloc(json_size);
    if (!json) {
        return nullptr;
    }
    
    // Helper function to escape JSON string
    auto escape_json_string = [](const char* src, char* dst, size_t dst_size) {
        if (!src || !dst || dst_size < 3) return;
        
        size_t src_len = strlen(src);
        size_t dst_pos = 0;
        
        for (size_t i = 0; i < src_len && dst_pos < dst_size - 2; i++) {
            char c = src[i];
            if (c == '"' || c == '\\') {
                if (dst_pos < dst_size - 3) {
                    dst[dst_pos++] = '\\';
                    dst[dst_pos++] = c;
                }
            } else if (c == '\n') {
                if (dst_pos < dst_size - 3) {
                    dst[dst_pos++] = '\\';
                    dst[dst_pos++] = 'n';
                }
            } else if (c == '\r') {
                if (dst_pos < dst_size - 3) {
                    dst[dst_pos++] = '\\';
                    dst[dst_pos++] = 'r';
                }
            } else if (c == '\t') {
                if (dst_pos < dst_size - 3) {
                    dst[dst_pos++] = '\\';
                    dst[dst_pos++] = 't';
                }
            } else {
                dst[dst_pos++] = c;
            }
        }
        dst[dst_pos] = '\0';
    };
    
    // Prepare escaped strings
    char* escaped_display = (char*)malloc(display_len * 2 + 1);
    
    if (escaped_display) {
        escaped_display[0] = '\0';
        
        if (tracker->display) escape_json_string(tracker->display, escaped_display, display_len * 2 + 1);
        
        // Build JSON for leaderboard tracker
        snprintf(json, json_size,
            "{"
            "\"id\":%u,"
            "\"display\":\"%s\""
            "}",
            tracker->id,
            escaped_display
        );
    } else {
        // Fallback without escaping if malloc failed
        snprintf(json, json_size,
            "{"
            "\"id\":%u,"
            "\"display\":\"%s\""
            "}",
            tracker->id,
            tracker->display ? tracker->display : ""
        );
    }
    
    // Clean up
    if (escaped_display) free(escaped_display);
    
    return json;
}

char* YabauseRA_CreateLeaderboardScoreboardJSON(const void* leaderboard_scoreboard, const void* leaderboard) {
    if (!leaderboard_scoreboard || !leaderboard) {
        return nullptr;
    }
    
    const rc_client_leaderboard_scoreboard_t* scoreboard = static_cast<const rc_client_leaderboard_scoreboard_t*>(leaderboard_scoreboard);
    const rc_client_leaderboard_t* lb = static_cast<const rc_client_leaderboard_t*>(leaderboard);
    
    // Escape JSON strings to handle special characters
    auto escapeJsonString = [](const char* str) -> char* {
        if (!str) return strdup("");
        
        size_t len = strlen(str);
        size_t escaped_len = len * 2 + 1; // Worst case: every char needs escaping
        char* escaped = static_cast<char*>(malloc(escaped_len));
        if (!escaped) return strdup("");
        
        size_t j = 0;
        for (size_t i = 0; i < len && j < escaped_len - 1; i++) {
            switch (str[i]) {
                case '"': 
                    if (j < escaped_len - 2) { escaped[j++] = '\\'; escaped[j++] = '"'; }
                    break;
                case '\\': 
                    if (j < escaped_len - 2) { escaped[j++] = '\\'; escaped[j++] = '\\'; }
                    break;
                case '\n': 
                    if (j < escaped_len - 2) { escaped[j++] = '\\'; escaped[j++] = 'n'; }
                    break;
                case '\r': 
                    if (j < escaped_len - 2) { escaped[j++] = '\\'; escaped[j++] = 'r'; }
                    break;
                case '\t': 
                    if (j < escaped_len - 2) { escaped[j++] = '\\'; escaped[j++] = 't'; }
                    break;
                default: 
                    escaped[j++] = str[i];
                    break;
            }
        }
        escaped[j] = '\0';
        return escaped;
    };
    
    // Escape the necessary strings
    char* escaped_title = escapeJsonString(lb->title);
    char* escaped_description = escapeJsonString(lb->description);
    char* escaped_submitted_score = escapeJsonString(scoreboard->submitted_score);
    char* escaped_best_score = escapeJsonString(scoreboard->best_score);
    
    // Create the JSON string matching the Swift LeaderboardScoreboardInfo structure
    size_t json_size = 1024 + strlen(escaped_title) + strlen(escaped_description) + 
                       strlen(escaped_submitted_score) + strlen(escaped_best_score);
    char* json = static_cast<char*>(malloc(json_size));
    
    if (json) {
        snprintf(json, json_size,
            "{"
            "\"id\":%u,"
            "\"title\":\"%s\","
            "\"description\":\"%s\","
            "\"submitted_score\":\"%s\","
            "\"best_score\":\"%s\","
            "\"new_rank\":%u,"
            "\"num_entries\":%u"
            "}",
            scoreboard->leaderboard_id,
            escaped_title,
            escaped_description,
            escaped_submitted_score,
            escaped_best_score,
            scoreboard->new_rank,
            scoreboard->num_entries
        );
    }
    
    // Free escaped strings
    free(escaped_title);
    free(escaped_description);
    free(escaped_submitted_score);
    free(escaped_best_score);
    
    return json;
}

char* YabauseRA_CreateProgressIndicatorJSON(const void* achievement) {
    if (!achievement) {
        return nullptr;
    }
    
    const rc_client_achievement_t* ach = static_cast<const rc_client_achievement_t*>(achievement);
    
    // Get badge image URL for active/locked state (progress indicators show locked badge)
    char badge_url[256] = {0};
    if (rc_client_achievement_get_image_url(ach, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE, badge_url, sizeof(badge_url)) != RC_OK) {
        // Fallback to unlocked state if active URL not available
        rc_client_achievement_get_image_url(ach, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED, badge_url, sizeof(badge_url));
    }
    
    // Create JSON string
    // Calculate required buffer size
    size_t title_len = ach->title ? strlen(ach->title) : 0;
    size_t desc_len = ach->description ? strlen(ach->description) : 0;
    size_t progress_len = ach->measured_progress ? strlen(ach->measured_progress) : 0;
    size_t badge_url_len = strlen(badge_url);
    size_t badge_name_len = ach->badge_name ? strlen(ach->badge_name) : 0;
    
    // Calculate JSON size (with escape characters and structure)
    size_t json_size = 768 + (title_len * 2) + (desc_len * 2) + (progress_len * 2) + badge_url_len + badge_name_len;
    
    char* json = (char*)malloc(json_size);
    if (!json) {
        return nullptr;
    }
    
    // Helper function to escape JSON string
    auto escape_json_string = [](const char* src, char* dst, size_t dst_size) {
        if (!src || !dst || dst_size < 3) return;
        
        size_t src_len = strlen(src);
        size_t dst_pos = 0;
        
        for (size_t i = 0; i < src_len && dst_pos < dst_size - 2; i++) {
            char c = src[i];
            if (c == '"' || c == '\\') {
                if (dst_pos < dst_size - 3) {
                    dst[dst_pos++] = '\\';
                    dst[dst_pos++] = c;
                }
            } else if (c == '\n') {
                if (dst_pos < dst_size - 3) {
                    dst[dst_pos++] = '\\';
                    dst[dst_pos++] = 'n';
                }
            } else if (c == '\r') {
                if (dst_pos < dst_size - 3) {
                    dst[dst_pos++] = '\\';
                    dst[dst_pos++] = 'r';
                }
            } else if (c == '\t') {
                if (dst_pos < dst_size - 3) {
                    dst[dst_pos++] = '\\';
                    dst[dst_pos++] = 't';
                }
            } else {
                dst[dst_pos++] = c;
            }
        }
        dst[dst_pos] = '\0';
    };
    
    // Prepare escaped strings
    char* escaped_title = (char*)malloc(title_len * 2 + 1);
    char* escaped_desc = (char*)malloc(desc_len * 2 + 1);
    char* escaped_progress = (char*)malloc(progress_len * 2 + 1);
    
    if (escaped_title && escaped_desc && escaped_progress) {
        escaped_title[0] = '\0';
        escaped_desc[0] = '\0';
        escaped_progress[0] = '\0';
        
        if (ach->title) escape_json_string(ach->title, escaped_title, title_len * 2 + 1);
        if (ach->description) escape_json_string(ach->description, escaped_desc, desc_len * 2 + 1);
        if (ach->measured_progress) escape_json_string(ach->measured_progress, escaped_progress, progress_len * 2 + 1);
        
        // Build JSON for progress indicator
        snprintf(json, json_size,
            "{"
            "\"id\":%u,"
            "\"title\":\"%s\","
            "\"description\":\"%s\","
            "\"badge\":\"%s\","
            "\"badge_url\":\"%s\","
            "\"points\":%u,"
            "\"state\":%u,"
            "\"measured_progress\":\"%s\","
            "\"measured_percent\":%f,"
            "\"category\":%u"
            "}",
            ach->id,
            escaped_title,
            escaped_desc,
            ach->badge_name ? ach->badge_name : "",
            badge_url,
            ach->points,
            ach->state,
            escaped_progress,
            ach->measured_percent,
            ach->category
        );
    } else {
        // Fallback without escaping if malloc failed
        snprintf(json, json_size,
            "{"
            "\"id\":%u,"
            "\"title\":\"%s\","
            "\"description\":\"%s\","
            "\"badge\":\"%s\","
            "\"badge_url\":\"%s\","
            "\"points\":%u,"
            "\"state\":%u,"
            "\"measured_progress\":\"%s\","
            "\"measured_percent\":%f,"
            "\"category\":%u"
            "}",
            ach->id,
            ach->title ? ach->title : "",
            ach->description ? ach->description : "",
            ach->badge_name ? ach->badge_name : "",
            badge_url,
            ach->points,
            ach->state,
            ach->measured_progress ? ach->measured_progress : "",
            ach->measured_percent,
            ach->category
        );
    }
    
    // Clean up
    if (escaped_title) free(escaped_title);
    if (escaped_desc) free(escaped_desc);
    if (escaped_progress) free(escaped_progress);
    
    return json;
}

} // extern "C"