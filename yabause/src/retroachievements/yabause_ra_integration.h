/*
YabaSanshiro RetroAchievements Integration Header
Copyright 2025 YabaSanshiro Team

This header defines the main RetroAchievements integration interface for YabaSanshiro.
*/

#ifndef YABAUSE_RA_INTEGRATION_H
#define YABAUSE_RA_INTEGRATION_H

#include <stdint.h>
#include <stddef.h>
#include <string>

// Forward declarations
struct rc_client_t;
struct rc_client_event_t;
struct rc_client_user_t;
struct rc_api_request_t;
struct rc_client_all_user_progress_t;
typedef void (*rc_client_server_callback_t)(const struct rc_api_server_response_t* server_response, void* callback_data);

struct YabauseRA_AchievementList;

namespace YabauseRA {

/**
 * Main RetroAchievements integration class
 * Manages the lifetime and operations of the rc_client
 */
class Integration {
public:
    // Callback types
    using EventCallback = void(*)(const rc_client_event_t* event);
    using ServerCallback = void(*)(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callback_data);
    using LoginCallback = void(*)(bool success, const char* username, const char* display_name, uint32_t score, const char* error_message);
    using GamePlacardCallback = void(*)(const char* game_title, const char* image_url, uint32_t unlocked_achievements, uint32_t total_achievements, uint32_t unlocked_points, uint32_t total_points, bool has_unsupported);

    /**
     * Get the singleton instance
     */
    static Integration* getInstance();

    /**
     * Destroy the singleton instance
     */
    static void destroyInstance();

    /**
     * Initialize the RetroAchievements client
     * @return true on success, false on failure
     */
    bool initialize();

    /**
     * Shutdown the RetroAchievements client
     */
    void shutdown();

    /**
     * Login with username and password
     * @param username RetroAchievements username
     * @param password User's password
     * @return true if login initiated successfully
     */
    bool loginUser(const char* username, const char* password);

    /**
     * Load a game from file path for achievement tracking
     * @param game_path Full path to the game file
     * @return true if loading initiated successfully
     */
    bool loadGameFromPath(const char* game_path);

    /**
     * Load a game for achievement tracking using pre-loaded data
     * @param game_data Pointer to game ROM data
     * @param data_size Size of game data in bytes
     * @param filename Game filename for identification
     */
    void loadGame(const void* game_data, size_t data_size, const char* filename);

    /**
     * Process one frame of achievement checking
     * Should be called once per emulation frame
     */
    void doFrame();

    /**
     * Enable or disable hardcore mode
     * @param enabled true to enable hardcore mode
     */
    void setHardcoreEnabled(bool enabled);

    /**
     * Check if hardcore mode is enabled
     * @return true if hardcore mode is enabled
     */
    bool isHardcoreEnabled() const;
    
    /**
     * Begin changing media (for multi-disc games)
     * @param new_media_path Path to the new disc/media file
     * @return true if media change initiated successfully
     */
    bool beginChangeMedia(const char* new_media_path);
    
    /**
     * Get the URL for the current game's image
     * @param buffer Buffer to store the URL
     * @param buffer_size Size of the buffer
     * @return true on success, false on failure
     */
    bool getGameImageURL(char* buffer, size_t buffer_size) const;
    
    /**
     * Check if the current game requires RetroAchievements processing
     * (has achievements, leaderboards, or rich presence)
     * @return true if processing is required
     */
    bool isProcessingRequired() const;

    /**
     * Handle media change (disc swapping) for multi-disc games
     * @param new_media_path Path to the new disc/media
     * @return true if media change initiated successfully
     */
    bool changeMedia(const char* new_media_path);

    /**
     * Get the size needed for serializing RetroAchievements progress
     * @return size in bytes needed for serialization buffer
     */
    size_t getProgressSize() const;

    /**
     * Serialize RetroAchievements progress for save states
     * @param buffer Buffer to write serialized data (must be at least getProgressSize() bytes)
     * @param buffer_size Size of the buffer
     * @return true on success, false on failure
     */
    bool serializeProgress(uint8_t* buffer, size_t buffer_size) const;

    /**
     * Deserialize RetroAchievements progress from save states
     * @param buffer Buffer containing serialized data (nullptr to reset state)
     * @param buffer_size Size of the buffer (0 if buffer is nullptr)
     * @return true on success, false on failure
     */
    bool deserializeProgress(const uint8_t* buffer, size_t buffer_size);

    /**
     * Get current user info
     * @return user info if logged in, nullptr otherwise
     */
    const rc_client_user_t* getUserInfo() const;

    /**
     * Get the rc_client instance
     * @return rc_client_t pointer or nullptr if not initialized
     */
    rc_client_t* getClient() const;

    /**
     * Set the event callback for achievement events
     * @param callback Function to call when events occur
     */
    void setEventCallback(EventCallback callback);

    /**
     * Set the server callback for HTTP requests
     * @param callback Function to call for server requests
     */
    void setServerCallback(ServerCallback callback);

    /**
     * Set the login callback for login result events
     * @param callback Function to call when login completes
     */
    void setLoginCallback(LoginCallback callback);

    /**
     * Set the game placard callback for game load events
     * @param callback Function to call when game is loaded and achievements become active
     */
    void setGamePlacardCallback(GamePlacardCallback callback);

    /**
     * Check if the client is initialized
     * @return true if initialized
     */
    bool isInitialized() const { return m_is_initialized; }

    /**
     * Read memory for achievement conditions (public access for external use)
     * @param address Memory address to read
     * @param buffer Buffer to store read data
     * @param num_bytes Number of bytes to read
     * @return Number of bytes actually read
     */
    uint32_t readMemory(uint32_t address, uint8_t* buffer, uint32_t num_bytes);

    /**
     * Handle server HTTP requests (public for network layer access)
     * @param request Request details
     * @param callback Callback function for response
     * @param callback_data Callback data for response
     */
    void handleServerRequest(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callback_data);

    /**
     * notify reset to RA
     */    
    void reset() const;

    /**
     * Set paused state for RetroAchievements processing
     * @param paused true to pause processing, false to resume
     */
    void setPaused(bool paused);

    /**
     * Get achievement badge URL
     * @param achievementId The achievement ID
     * @param state The achievement state
     * @return URL string for the badge image, or nullptr if not available
     */
    const char* getAchievementBadgeURL(int achievementId, int state);

    /**
     * Get achievement badge name for local caching
     * @param achievementId The achievement ID
     * @return Badge name string, or nullptr if not available
     */
    const char* getAchievementBadgeName(int achievementId);

    /**
     * Create achievement list using rc_client_create_achievement_list (JSON-based)
     * @param category Achievement category
     * @param grouping Achievement grouping
     * @return JSON string containing achievement data (caller must free()), or nullptr if failed
     */
    char* createAchievementListJSON(int category, int grouping);
    
    /**
     * Escape a string for JSON format
     * @param str Input string to escape
     * @return Escaped string suitable for JSON
     */
    std::string escapeJsonString(const char* str);

    // Make constructor/destructor public for make_unique
    Integration();
    ~Integration();

private:

    // Disable copy/move
    Integration(const Integration&) = delete;
    Integration& operator=(const Integration&) = delete;

    /**
     * Handle RetroAchievements events
     * @param event Event details
     */
    void handleEvent(const rc_client_event_t* event);

    /**
     * Called when login completes
     * @param result Result code
     * @param error_message Error message if failed
     */
    void onLoginComplete(int result, const char* error_message);

    /**
     * Called when game load completes
     * @param result Result code
     * @param error_message Error message if failed
     */
    void onGameLoadComplete(int result, const char* error_message);

    /**
     * Called when user progress sync completes
     * @param result Result code
     * @param error_message Error message if failed
     * @param list User progress list from server
     */
    void onUserProgressSyncComplete(int result, const char* error_message, rc_client_all_user_progress_t* list);

    /**
     * Show game placard following rcheevos guidelines
     * This should be called when achievements become active for a game
     */
    void showGamePlacard();


private:
    rc_client_t* m_client;
    bool m_is_initialized;
    bool m_is_hardcore_enabled;
    bool m_is_paused;
    EventCallback m_event_callback;
    ServerCallback m_server_callback;
    LoginCallback m_login_callback;
    GamePlacardCallback m_game_placard_callback;
};

// Memory implementation function (defined in yabause_ra_memory.cpp)
uint32_t yabause_ra_read_memory_impl(uint32_t address, uint8_t* buffer, uint32_t num_bytes);

} // namespace YabauseRA

#endif // YABAUSE_RA_INTEGRATION_H