/*
YabaSanshiro RetroAchievements Integration C Wrapper Header
Copyright 2025 YabaSanshiro Team

This header provides C-compatible wrapper functions for RetroAchievements integration.
For use in C code that needs to interface with the C++ RetroAchievements implementation.
*/

#ifndef YABAUSE_RA_INTEGRATION_CWRAPPER_H
#define YABAUSE_RA_INTEGRATION_CWRAPPER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// JSON-based achievement list (safer than struct-based approach)
// Returns malloc'd JSON string that caller must free()



/**
 * Get the size needed for serializing RetroAchievements progress
 * @return size in bytes needed for serialization buffer, 0 if not available
 */
size_t YabauseRA_GetProgressSize(void);

/**
 * Serialize RetroAchievements progress for save states
 * @param buffer Buffer to write serialized data (must be at least YabauseRA_GetProgressSize() bytes)
 * @param buffer_size Size of the buffer
 * @return 1 on success, 0 on failure
 */
int YabauseRA_SerializeProgress(uint8_t* buffer, size_t buffer_size);

/**
 * Deserialize RetroAchievements progress from save states
 * @param buffer Buffer containing serialized data (NULL to reset state)
 * @param buffer_size Size of the buffer (0 if buffer is NULL)
 * @return 1 on success, 0 on failure
 */
int YabauseRA_DeserializeProgress(const uint8_t* buffer, size_t buffer_size);

/**
 * Initialize the RetroAchievements system
 * @return 1 on success, 0 on failure
 */
int YabauseRA_Initialize(void);

/**
 * Shutdown the RetroAchievements system
 */
void YabauseRA_Shutdown(void);

/**
 * Initialize user with credentials
 * @param username RetroAchievements username
 * @param token User token or password
 * @return 1 on success, 0 on failure
 */
int YabauseRA_InitializeUser(const char* username, const char* token);

/**
 * Logout the current user
 */
void YabauseRA_Logout(void);

/**
 * Check if user is logged in
 * @return 1 if logged in, 0 otherwise
 */
int YabauseRA_IsUserLoggedIn(void);

/**
 * Get the current username
 * @return username string or NULL if not logged in
 */
const char* YabauseRA_GetUsername(void);

/**
 * Enable or disable hardcore mode
 * @param enabled 1 to enable, 0 to disable
 */
void YabauseRA_SetHardcoreEnabled(int enabled);

/**
 * Check if hardcore mode is enabled
 * @return 1 if enabled, 0 if disabled
 */
int YabauseRA_GetHardcoreEnabled(void);

/**
 * Reset the RetroAchievements runtime and emulator
 * Called when hardcore mode triggers a reset event
 */
void YabauseRA_Reset(void);

/**
 * Check if RetroAchievements processing is required for the current game
 * @return 1 if processing is required (game has achievements/leaderboards/rich presence), 0 if not
 */
int YabauseRA_IsProcessingRequired(void);

/**
 * Load game for achievement tracking by hash
 * @param game_hash Game hash string
 * @return 1 on success, 0 on failure
 */
int YabauseRA_LoadGame(const char* game_hash);

/**
 * Load game for achievement tracking from file path
 * @param game_path Path to the game file
 * @return 1 on success, 0 on failure
 */
int YabauseRA_LoadGameFromFile(const char* game_path);

/**
 * Get current game hash
 * @return game hash string or NULL if no game loaded
 */
const char* YabauseRA_GetGameHash(void);

/**
 * Update rich presence string
 * @param rich_presence Rich presence text
 */
void YabauseRA_UpdateRichPresence(const char* rich_presence);

/**
 * Get rich presence display string
 * @return rich presence string or NULL
 */
const char* YabauseRA_GetRichPresenceDisplayString(void);

/**
 * Achievement triggered callback
 * @param achievement_id ID of the triggered achievement
 */
void YabauseRA_OnAchievementTriggered(int achievement_id);

/**
 * Leaderboard submitted callback
 * @param leaderboard_id ID of the leaderboard
 * @param score Submitted score
 */
void YabauseRA_OnLeaderboardSubmitted(int leaderboard_id, int score);

/**
 * Read memory for achievement evaluation
 * @param address Memory address
 * @param size Number of bytes to read
 * @return Memory value (size-dependent)
 */
uint32_t YabauseRA_ReadMemory(uint32_t address, uint32_t size);

/**
 * Process one frame of achievements
 */
void YabauseRA_DoFrame(void);

/**
 * Begin changing media for multi-disc games
 * @param new_media_path Path to the new disc/media file
 * @return 1 if media change initiated successfully, 0 on failure
 */
int YabauseRA_BeginChangeMedia(const char* new_media_path);

/**
 * Get the URL for the current game's image
 * @param buffer Buffer to store the URL (must be at least 256 bytes)
 * @param buffer_size Size of the buffer
 * @return 1 on success, 0 on failure
 */
int YabauseRA_GetGameImageURL(char* buffer, size_t buffer_size);

/**
 * Set server callback for HTTP requests
 * @param callback Function pointer for server requests
 */
typedef void (*YabauseRA_ServerCallback)(const char* url, const char* post_data, void* userdata);
void YabauseRA_SetServerCallback(YabauseRA_ServerCallback callback);

/**
 * Complete a pending HTTP request with response data
 * @param userdata The userdata pointer passed to the server callback
 * @param http_status_code HTTP status code (200 for success)
 * @param response_body Response body data (can be NULL)
 * @param response_length Length of response body (0 if response_body is NULL)
 */
void YabauseRA_CompleteServerRequest(void* userdata, int http_status_code, const char* response_body, size_t response_length);

/**
 * Set event callback for achievement events
 * @param callback Function pointer for achievement events
 */
typedef void (*YabauseRA_EventCallback)(int event_type, const char* message);
void YabauseRA_SetEventCallback(YabauseRA_EventCallback callback);

/**
 * Set login callback for login result events
 * @param callback Function pointer for login completion events
 */
typedef void (*YabauseRA_LoginCallback)(int success, const char* username, const char* display_name, uint32_t score, const char* error_message);
void YabauseRA_SetLoginCallback(YabauseRA_LoginCallback callback);

/**
 * Set game placard callback for game load events
 * @param callback Function pointer for game placard display events
 */
typedef void (*YabauseRA_GamePlacardCallback)(const char* game_title, const char* image_url, uint32_t unlocked_achievements, uint32_t total_achievements, uint32_t unlocked_points, uint32_t total_points, int has_unsupported);
void YabauseRA_SetGamePlacardCallback(YabauseRA_GamePlacardCallback callback);

/**
 * Set paused state for RetroAchievements processing
 * @param paused 1 to pause, 0 to resume
 */
void YabauseRA_SetPaused(int paused);

/**
 * Get achievement badge URL
 * @param achievementId The achievement ID
 * @param state The achievement state (locked/unlocked)
 * @return URL string for the badge image, or NULL if not available
 */
const char* YabauseRA_GetAchievementBadgeURL(int achievementId, int state);

/**
 * Get achievement badge name (for local caching)
 * @param achievementId The achievement ID
 * @return Badge name string, or NULL if not available
 */
const char* YabauseRA_GetAchievementBadgeName(int achievementId);

/**
 * Create achievement list using rc_client_create_achievement_list (JSON-based)
 * @param category Achievement category (RC_CLIENT_ACHIEVEMENT_CATEGORY_*)
 * @param grouping Achievement grouping (RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_*)
 * @return JSON string containing achievement list data (caller must free()), or NULL if failed
 */
char* YabauseRA_CreateAchievementListJSON(int category, int grouping);

/**
 * Create Achievement Triggered JSON for a specific achievement (includes badge URL)
 * @param achievement Pointer to rc_client_achievement_t structure
 * @return JSON string containing triggered achievement data (caller must free()), or NULL if failed
 */
char* YabauseRA_CreateAchievementTriggeredJSON(const void* achievement);

/**
 * Create Challenge Indicator JSON for a specific achievement (includes badge URL)
 * @param achievement Pointer to rc_client_achievement_t structure
 * @return JSON string containing challenge indicator data (caller must free()), or NULL if failed
 */
char* YabauseRA_CreateChallengeIndicatorJSON(const void* achievement);

/**
 * Create Leaderboard Tracker JSON for a specific leaderboard tracker
 * @param leaderboard_tracker Pointer to rc_client_leaderboard_tracker_t structure
 * @return JSON string containing leaderboard tracker data (caller must free()), or NULL if failed
 */
char* YabauseRA_CreateLeaderboardTrackerJSON(const void* leaderboard_tracker);

/**
 * Create Progress Indicator JSON for a specific achievement with progress information
 * @param achievement Pointer to rc_client_achievement_t structure
 * @return JSON string containing progress indicator data (caller must free()), or NULL if failed
 */
/**
 * Create Leaderboard Scoreboard JSON for a leaderboard scoreboard event
 * @param leaderboard_scoreboard Pointer to rc_client_leaderboard_scoreboard_t structure
 * @param leaderboard Pointer to rc_client_leaderboard_t structure
 * @return JSON string containing leaderboard scoreboard data (caller must free()), or NULL if failed
 */
char* YabauseRA_CreateLeaderboardScoreboardJSON(const void* leaderboard_scoreboard, const void* leaderboard);

char* YabauseRA_CreateProgressIndicatorJSON(const void* achievement);

#ifdef __cplusplus
}
#endif

#endif // YABAUSE_RA_INTEGRATION_CWRAPPER_H