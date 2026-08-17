/*
YabaSanshiro RetroAchievements Integration
Copyright 2025 YabaSanshiro Team

This file implements the core RetroAchievements integration for YabaSanshiro.
*/

#include "yabause_ra_integration_cwrapper.h"
#include "yabause_ra_integration.h"
#include "rc_client.h"
#include "rc_hash.h"
#include "rc_consoles.h"
#include "../yabause.h"
#include "../core.h"
#include <cstring>
#include <memory>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "../android_debug_log.h"

#ifdef ANDROID
#include <unistd.h> // for dup()
extern "C" {
FILE* idiocy_fopen_fd(const char* fname, const char * mode);
extern const char * GetFileDescriptorPath( const char * fileName );
}
#define fopen_utf8 idiocy_fopen_fd
#elif defined(_WINDOWS)
#include <stdio.h>
#include <wchar.h>
#include <windows.h>
#define strcasecmp _stricmp
#else
#include <stdio.h>
#endif

// Use the unified logging macros
#define LOGD(...) RA_LOGD(__VA_ARGS__)
#define LOGE(...) RA_LOGE(__VA_ARGS__)



namespace YabauseRA {

// Forward declarations for CD format support
extern "C" {
    int checkCHD(const char *filename);
    
    // CHD library direct access
    typedef struct chd_file chd_file;
    typedef int chd_error;
    
    // CHD header structure (partial definition)
    typedef struct chd_header {
        uint32_t length;
        uint32_t version;
        uint32_t flags;
        uint32_t compression;
        uint32_t hunkbytes;
        uint32_t totalhunks;
        uint64_t logicalbytes;
        uint64_t metaoffset;
        // ... other fields not needed
    } chd_header;
    
    #define CHDERR_NONE 0
    #define CHD_OPEN_READ 1
    #define CD_FRAME_SIZE 2448
    
    chd_error chd_open(const char *filename, int mode, chd_file *parent, chd_file **chd);
    void chd_close(chd_file *chd);
    const chd_header *chd_get_header(chd_file *chd);
    chd_error chd_read(chd_file *chd, uint32_t hunknum, void *buffer);
}

// CD track handle structure to support different formats
struct CDTrackHandle {
    enum CDImageType {
        CD_TYPE_ISO,
        CD_TYPE_CHD,
        CD_TYPE_CUE,
        CD_TYPE_MDS,
        CD_TYPE_CCD
    } type;
    
    union {
        FILE* file_handle;          // For ISO files
        struct {
            chd_file* chd;
            const chd_header* header;
            uint8_t* hunk_buffer;
            uint32_t current_hunk_id;
            uint32_t hunk_bytes;
        } chd_data;                 // For CHD files
        void* cue_handle;           // For CUE files
        void* mds_handle;           // For MDS files
        void* ccd_handle;           // For CCD files
    };
    
    uint32_t track_number;
    char path[512];
};

// Detect CD image format from file extension and content
static CDTrackHandle::CDImageType detect_cd_format(const char* path) {
    if (!path) return CDTrackHandle::CD_TYPE_ISO;
    
    // Check file extension first
    const char* ext = strrchr(path, '.');
    if (ext) {
        if (strcasecmp(ext, ".chd") == 0) {
            return CDTrackHandle::CD_TYPE_CHD;
        } else if (strcasecmp(ext, ".cue") == 0) {
            return CDTrackHandle::CD_TYPE_CUE;
        } else if (strcasecmp(ext, ".mds") == 0) {
            return CDTrackHandle::CD_TYPE_MDS;
        } else if (strcasecmp(ext, ".ccd") == 0) {
            return CDTrackHandle::CD_TYPE_CCD;
        }
    }
    
    // Check CHD format by header for files without extension
    if (checkCHD(path) == 0) {
        return CDTrackHandle::CD_TYPE_CHD;
    }
    
    // Default to ISO
    return CDTrackHandle::CD_TYPE_ISO;
}

static void* cd_open_track(const char* path, uint32_t track) {
    LOGD("Opening CD track %u for path: %s", track, path);
    
    // For Saturn games, we typically need track 1 (data track)
    if (track != 1) {
        LOGE("Only track 1 is supported for Saturn games");
        return nullptr;
    }
    
    CDTrackHandle* handle = new CDTrackHandle();
    handle->track_number = track;
    strncpy(handle->path, path, sizeof(handle->path) - 1);
    handle->path[sizeof(handle->path) - 1] = '\0';
    
    // Detect format and open accordingly
    handle->type = detect_cd_format(path);
    
    switch (handle->type) {
        case CDTrackHandle::CD_TYPE_CHD:
        {
            LOGD("Opening CHD format");
            // Direct CHD library usage
            chd_error error = chd_open(path, CHD_OPEN_READ, nullptr, &handle->chd_data.chd);
            if (error != CHDERR_NONE) {
                LOGE("Failed to open CHD file: %s (error: %d)", path, error);
                delete handle;
                return nullptr;
            }
            
            handle->chd_data.header = chd_get_header(handle->chd_data.chd);
            handle->chd_data.hunk_bytes = handle->chd_data.header->hunkbytes;
            handle->chd_data.hunk_buffer = static_cast<uint8_t*>(malloc(handle->chd_data.hunk_bytes));
            handle->chd_data.current_hunk_id = 0xFFFFFFFF; // Invalid hunk ID to force first read
            
            if (!handle->chd_data.hunk_buffer) {
                LOGE("Failed to allocate CHD hunk buffer");
                chd_close(handle->chd_data.chd);
                delete handle;
                return nullptr;
            }
            
            LOGD("CHD opened successfully: hunk_bytes=%u", handle->chd_data.hunk_bytes);
            break;
        }
            
        case CDTrackHandle::CD_TYPE_CUE:
        {
            LOGD("Opening CUE format");
            // Read CUE file to find the referenced ISO/BIN file
            FILE* cue_file = fopen_utf8(path, "r");
            if (!cue_file) {
                LOGE("Failed to open CUE file: %s", path);
                delete handle;
                return nullptr;
            }
            
            char line[512];
            char referenced_filename[512] = {0};
            char referenced_file[1024] = {0};
            bool found_file = false;
            
            while (fgets(line, sizeof(line), cue_file)) {
                // Look for FILE "filename" pattern
                char* file_start = strstr(line, "FILE \"");
                if (file_start) {
                    file_start += 6; // Skip "FILE \""
                    char* file_end = strchr(file_start, '"');
                    if (file_end) {
                        size_t len = file_end - file_start;
                        if (len < sizeof(referenced_filename)) {
                            strncpy(referenced_filename, file_start, len);
                            referenced_filename[len] = '\0';
                            found_file = true;
                            break;
                        }
                    }
                }
            }

            fclose(cue_file);
            
            if (!found_file) {
                LOGE("No FILE entry found in CUE file: %s", path);
                delete handle;
                return nullptr;
            }
            
#ifdef ANDROID            
            const char * fdname = GetFileDescriptorPath((const char *)(referenced_file));
#else            
            // Extract directory from CUE file path and combine with referenced filename
            strncpy(referenced_file, path, sizeof(referenced_file) - 1);
            referenced_file[sizeof(referenced_file) - 1] = '\0';
            
            // Find the last directory separator
            char* last_slash = strrchr(referenced_file, '/');
            char* last_backslash = strrchr(referenced_file, '\\');
            char* dir_end = (last_slash > last_backslash) ? last_slash : last_backslash;
            
            if (dir_end) {
                // Replace filename with referenced filename from CUE
                dir_end[1] = '\0'; // Keep directory separator
                strncat(referenced_file, referenced_filename, sizeof(referenced_file) - strlen(referenced_file) - 1);
            } else {
                // No directory in path, use referenced filename as-is
                strncpy(referenced_file, referenced_filename, sizeof(referenced_file) - 1);
                referenced_file[sizeof(referenced_file) - 1] = '\0';
            }
            
            LOGD("Resolved CUE referenced file: %s -> %s", referenced_filename, referenced_file);
            const char * fdname = (const char *)(referenced_file);
#endif       
            if( fdname == NULL ){
                LOGE("Failed to open referenced file from CUE: %s", referenced_file);
                delete handle;
                return nullptr;                
            }
            
            // Open the referenced file as ISO
            handle->file_handle = fopen(fdname, "rb");
            if (!handle->file_handle) {
                LOGE("Failed to open referenced file from CUE: %s", referenced_file);
                delete handle;
                return nullptr;
            }
            
            LOGD("CUE format opened successfully, using referenced file: %s", referenced_file);
            break;
        }
            
        case CDTrackHandle::CD_TYPE_MDS:
        {
            LOGD("Opening MDS format");
            // For MDS files, replace .mds extension with .mdf to find the data file
            char mdf_path[512];
            strncpy(mdf_path, path, sizeof(mdf_path) - 1);
            mdf_path[sizeof(mdf_path) - 1] = '\0';
            
            // Find the .mds extension and replace with .mdf
            char* ext = strrchr(mdf_path, '.');
            if (ext && (strcasecmp(ext, ".mds") == 0)) {
                strcpy(ext, ".mdf");
            } else {
                LOGE("Invalid MDS file extension: %s", path);
                delete handle;
                return nullptr;
            }
            
            // Open the MDF file as ISO
            handle->file_handle = fopen_utf8(mdf_path, "rb");
            if (!handle->file_handle) {
                LOGE("Failed to open MDF file: %s", mdf_path);
                delete handle;
                return nullptr;
            }
            
            LOGD("MDS format opened successfully, using MDF file: %s", mdf_path);
            break;
        }
            
        case CDTrackHandle::CD_TYPE_CCD:
        {
            LOGD("Opening CCD format");
            // For CCD files, replace .ccd extension with .img to find the data file
            char img_path[512];
            strncpy(img_path, path, sizeof(img_path) - 1);
            img_path[sizeof(img_path) - 1] = '\0';
            
            // Find the .ccd extension and replace with .img
            char* ext = strrchr(img_path, '.');
            if (ext && (strcasecmp(ext, ".ccd") == 0)) {
                strcpy(ext, ".img");
            } else {
                LOGE("Invalid CCD file extension: %s", path);
                delete handle;
                return nullptr;
            }
            
            // Open the IMG file as ISO
            handle->file_handle = fopen_utf8(img_path, "rb");
            if (!handle->file_handle) {
                LOGE("Failed to open IMG file: %s", img_path);
                delete handle;
                return nullptr;
            }
            
            LOGD("CCD format opened successfully, using IMG file: %s", img_path);
            break;
        }
            
        case CDTrackHandle::CD_TYPE_ISO:
        default:
            LOGD("Opening ISO format");
            handle->file_handle = fopen_utf8(path, "rb");
            if (!handle->file_handle) {
                LOGE("Failed to open ISO file: %s", path);
                delete handle;
                return nullptr;
            }
            break;
    }
    
    return handle;
}

static size_t cd_read_sector(void* track_handle, uint32_t sector, void* buffer, size_t buffer_size) {
    if (!track_handle || !buffer) {
        LOGE("Invalid parameters for CD sector read");
        return 0;
    }
    
    CDTrackHandle* handle = static_cast<CDTrackHandle*>(track_handle);
    
    // Saturn CD sectors are typically 2048 bytes (Mode 1)
    const size_t SECTOR_SIZE = 2048;
    const size_t read_size = (buffer_size < SECTOR_SIZE) ? buffer_size : SECTOR_SIZE;
    
    switch (handle->type) {
        case CDTrackHandle::CD_TYPE_CHD: {
            // Direct CHD reading implementation
            // CHD stores full CD frames (2448 bytes), but we want just the data portion (2048 bytes)
            uint32_t byte_offset = sector * CD_FRAME_SIZE; // Use full frame size for CHD
            uint32_t hunk_id = byte_offset / handle->chd_data.hunk_bytes;
            uint32_t hunk_offset = byte_offset % handle->chd_data.hunk_bytes;
            
            // Read hunk if it's not already cached
            if (handle->chd_data.current_hunk_id != hunk_id) {
                chd_error error = chd_read(handle->chd_data.chd, hunk_id, handle->chd_data.hunk_buffer);
                if (error != CHDERR_NONE) {
                    LOGE("Failed to read CHD hunk %u (error: %d)", hunk_id, error);
                    return 0;
                }
                handle->chd_data.current_hunk_id = hunk_id;
            }
            
            // For Mode 1 CD data sectors, the actual data starts at offset 16 within the 2448-byte frame
            // Frame structure: 12-byte sync + 4-byte header + 2048-byte data + 4-byte EDC + 276-byte P/Q
            uint8_t* frame_start = handle->chd_data.hunk_buffer + hunk_offset;
            uint8_t* data_start = frame_start + 16; // Skip sync + header
            
            // Copy the data portion (2048 bytes)
            size_t copy_size = read_size;
            if (hunk_offset + 16 + copy_size > handle->chd_data.hunk_bytes) {
                // Handle boundary case
                size_t available = handle->chd_data.hunk_bytes - (hunk_offset + 16);
                copy_size = (available < copy_size) ? available : copy_size;
            }
            
            memcpy(buffer, data_start, copy_size);
            
            LOGD("Read CHD sector %u (hunk %u, offset %u+16): %zu bytes", sector, hunk_id, hunk_offset, copy_size);
            return copy_size;
        }
            
        case CDTrackHandle::CD_TYPE_ISO:
        case CDTrackHandle::CD_TYPE_CUE:
        case CDTrackHandle::CD_TYPE_MDS:
        case CDTrackHandle::CD_TYPE_CCD:
        {
            // All these formats use file_handle for reading their data files
            if (fseek(handle->file_handle, 16+sector * SECTOR_SIZE, SEEK_SET) != 0) {
                LOGE("Failed to seek to sector %u", sector);
                return 0;
            }
            
            size_t bytes_read = fread(buffer, 1, read_size, handle->file_handle);
            if (bytes_read != read_size) {
                LOGE("Failed to read complete sector. Expected: %zu, Read: %zu", read_size, bytes_read);
                return 0;
            }
            
            LOGD("Read sector %u from format %d: %zu bytes", sector, handle->type, bytes_read);
            return bytes_read;
        }
            
        default:
            LOGE("Unknown CD format: %d", handle->type);
            return 0;
    }
}

static void cd_close_track(void* track_handle) {
    if (!track_handle) return;
    
    CDTrackHandle* handle = static_cast<CDTrackHandle*>(track_handle);
    
    switch (handle->type) {
        case CDTrackHandle::CD_TYPE_ISO:
        case CDTrackHandle::CD_TYPE_CUE:
        case CDTrackHandle::CD_TYPE_MDS:
        case CDTrackHandle::CD_TYPE_CCD:
            // All these formats use file_handle for their data files
            if (handle->file_handle) {
                fclose(handle->file_handle);
                LOGD("Closed track for format: %d", handle->type);
            }
            break;
            
        case CDTrackHandle::CD_TYPE_CHD:
            // Direct CHD cleanup
            if (handle->chd_data.hunk_buffer) {
                free(handle->chd_data.hunk_buffer);
            }
            if (handle->chd_data.chd) {
                chd_close(handle->chd_data.chd);
            }
            LOGD("Closed CHD track");
            break;
    }
    
    delete handle;
}

static uint32_t cd_first_track_sector(void* track_handle) {
    // For Saturn CD games, the first track typically starts at sector 0
    return 0;
}

// CD reader function table
static rc_hash_cdreader_t g_cdreader_functions = {
    cd_open_track,
    cd_read_sector,
    cd_close_track,
    cd_first_track_sector
};

// Global instance
static std::unique_ptr<Integration> g_instance = nullptr;

Integration::Integration() 
    : m_client(nullptr)
    , m_is_initialized(false)
    , m_is_hardcore_enabled(true)
    , m_is_paused(false)
    , m_event_callback(nullptr)
    , m_server_callback(nullptr)
    , m_login_callback(nullptr)
    , m_game_placard_callback(nullptr)
{
    LOGD("RetroAchievements Integration created");
}

Integration::~Integration() {
    shutdown();
}

Integration* Integration::getInstance() {
    if (!g_instance) {
        g_instance = std::make_unique<Integration>();
    }
    return g_instance.get();
}

void Integration::destroyInstance() {
    g_instance.reset();
}

bool Integration::initialize() {
    if (m_is_initialized) {
        LOGD("Already initialized");
        return true;
    }

    LOGD("Initializing RetroAchievements...");

    // Initialize CD reader for Saturn games
    rc_hash_init_custom_cdreader(&g_cdreader_functions);

    // Create rc_client with our memory and server callbacks
    m_client = rc_client_create(
        [](uint32_t address, uint8_t* buffer, uint32_t num_bytes, rc_client_t* client) -> uint32_t {
            return Integration::getInstance()->readMemory(address, buffer, num_bytes);
        },
        [](const rc_api_request_t* request, rc_client_server_callback_t callback, void* callback_data, rc_client_t* client) {
            Integration::getInstance()->handleServerRequest(request, callback, callback_data);
        }
    );

    if (!m_client) {
        LOGE("Failed to create rc_client");
        return false;
    }

    // Set up event handler
    rc_client_set_event_handler(m_client, 
        [](const rc_client_event_t* event, rc_client_t* client) {
            Integration::getInstance()->handleEvent(event);
        }
    );

    // Start in hardcore mode by default per RetroAchievements guidelines
    // Hardcore should be on by default to avoid disappointing users who expect hardcore unlocks
    rc_client_set_hardcore_enabled(m_client, 1);
    m_is_hardcore_enabled = true;

    // Enable logging for debugging (reduce verbosity for ping issues)
    rc_client_enable_logging(m_client, RC_CLIENT_LOG_LEVEL_INFO, 
        [](const char* message, const rc_client_t* client) {
            LOGD("RC: %s", message);
        }
    );

    m_is_initialized = true;
    LOGD("RetroAchievements initialization complete");
    return true;
}

void Integration::shutdown() {
    if (!m_is_initialized) {
        return;
    }

    LOGD("Shutting down RetroAchievements...");

    if (m_client) {
        rc_client_destroy(m_client);
        m_client = nullptr;
    }

    m_is_initialized = false;
    m_event_callback = nullptr;
    m_server_callback = nullptr;
    m_login_callback = nullptr;
    m_game_placard_callback = nullptr;
    
    LOGD("RetroAchievements shutdown complete");
}

bool Integration::loginUser(const char* username, const char* password) {
    if (!m_client) {
        LOGE("Client not initialized");
        return false;
    }

    LOGD("Logging in user: %s", username);

    rc_client_begin_login_with_password(m_client, username, password, 
        [](int result, const char* error_message, rc_client_t* client, void* userdata) {
            Integration* instance = static_cast<Integration*>(userdata);
            instance->onLoginComplete(result, error_message);
        }, 
        this
    );

    return true;
}

bool Integration::loadGameFromPath(const char* game_path) {
    if (!m_client) {
        LOGE("Client not initialized");
        return false;
    }

    if (!game_path || strlen(game_path) == 0) {
        LOGE("Invalid game path");
        return false;
    }

    // Check if user is logged in before attempting game load
    const rc_client_user_t* user = rc_client_get_user_info(m_client);
    if (!user) {
        LOGE("User not logged in - cannot load game for RetroAchievements");
        return false;
    }
    
    LOGD("Loading game from path: %s (User: %s logged in)", game_path, user->username);
    
#ifdef RC_CLIENT_SUPPORTS_HASH
    // Load game using the actual data
    rc_client_begin_identify_and_load_game(m_client, RC_CONSOLE_SATURN, 
        game_path, NULL, 0,
        [](int result, const char* error_message, rc_client_t* client, void* userdata) {
            Integration* instance = static_cast<Integration*>(userdata);
            instance->onGameLoadComplete(result, error_message);
        }, 
        this
    );
#else
    LOGE("Game loading not supported: RC_CLIENT_SUPPORTS_HASH not defined");
    return false;
#endif

    return true;
}

void Integration::loadGame(const void* game_data, size_t data_size, const char* filename) {
    if (!m_client) {
        LOGE("Client not initialized");
        return;
    }

    if (!game_data || data_size == 0 || !filename) {
        LOGE("Invalid game data or filename");
        return;
    }

    LOGD("Loading game: %s (size: %zu)", filename, data_size);

#ifdef RC_CLIENT_SUPPORTS_HASH
    rc_client_begin_identify_and_load_game(m_client, RC_CONSOLE_SATURN, 
        filename, static_cast<const uint8_t*>(game_data), data_size,
        [](int result, const char* error_message, rc_client_t* client, void* userdata) {
            Integration* instance = static_cast<Integration*>(userdata);
            instance->onGameLoadComplete(result, error_message);
        }, 
        this
    );
#else
    LOGE("Game loading not supported: RC_CLIENT_SUPPORTS_HASH not defined");
#endif
}

void Integration::doFrame() {
    if (m_client && m_is_initialized && !m_is_paused) {
        rc_client_do_frame(m_client);
    }
}

void Integration::setPaused(bool paused) {
    m_is_paused = paused;
    
    // If we're unpausing, call rc_client_idle to handle any pending tasks
    if (!paused && m_client && m_is_initialized) {
        rc_client_idle(m_client);
    }
}

void Integration::setHardcoreEnabled(bool enabled) {
    if (!m_client) {
        return;
    }

    LOGD("Setting hardcore mode: %s", enabled ? "enabled" : "disabled");
    rc_client_set_hardcore_enabled(m_client, enabled ? 1 : 0);
    m_is_hardcore_enabled = enabled;
}

bool Integration::isHardcoreEnabled() const {
    return m_is_hardcore_enabled;
}

bool Integration::beginChangeMedia(const char* new_media_path) {
    if (!m_client || !m_is_initialized || !new_media_path) {
        LOGE("Cannot change media: client not initialized or invalid path");
        return false;
    }
    
    LOGD("Beginning media change to: %s", new_media_path);
    
#ifdef RC_CLIENT_SUPPORTS_HASH
    // Use rc_client_begin_identify_and_change_media for file path support
    rc_client_begin_identify_and_change_media(m_client, new_media_path, NULL, 0,
        [](int result, const char* error_message, rc_client_t* client, void* userdata) {
            Integration* self = Integration::getInstance();
            if (!self) return;
            
            // Handle media change result
            if (result == RC_OK) {
                LOGD("Media change successful");
                // Media validated and loaded successfully
            } else if (result == RC_HARDCORE_DISABLED) {
                LOGD("Media change disabled hardcore mode: unrecognized media");
                // Hardcore mode was disabled due to unrecognized media
                if (self->m_event_callback) {
                    rc_client_event_t event = {};
                    event.type = RC_CLIENT_EVENT_RESET;
                    self->m_event_callback(&event);
                }
            } else {
                LOGE("Media change failed: %s", error_message ? error_message : "Unknown error");
                // Media change failed
            }
        }, nullptr);
#else
    // Fallback: compute hash and use rc_client_begin_change_media
    // Note: This requires implementing hash computation for the file
    LOGE("Media change not supported: RC_CLIENT_SUPPORTS_HASH not defined");
    return false;
#endif
    
    return true;
}

bool Integration::getGameImageURL(char* buffer, size_t buffer_size) const {
    if (!m_client || !buffer || buffer_size == 0) {
        return false;
    }
    
    const rc_client_game_t* game = rc_client_get_game_info(m_client);
    if (!game) {
        return false;
    }
    
    int result = rc_client_game_get_image_url(game, buffer, buffer_size);
    return result == RC_OK;
}

bool Integration::isProcessingRequired() const {
    if (!m_client) {
        return false;
    }
    return rc_client_is_processing_required(m_client) != 0;
}

uint32_t Integration::readMemory(uint32_t address, uint8_t* buffer, uint32_t num_bytes) {
    // Forward to the memory implementation
    return YabauseRA::yabause_ra_read_memory_impl(address, buffer, num_bytes);
}

void Integration::handleServerRequest(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callback_data) {
    LOGD("Server request: %s", request->url ? request->url : "NULL");
    if (m_server_callback) {
        LOGD("Calling server callback");
        m_server_callback(request, callback, callback_data);
    } else {
        LOGE("No server callback set, completing with error");
        // No server callback set, complete with error
        rc_api_server_response_t response = {0};
        response.http_status_code = 0; // Error status
        callback(&response, callback_data);
    }
}

void Integration::handleEvent(const rc_client_event_t* event) {
    if (!event) return;

    switch (event->type) {
        case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
            LOGD("Achievement unlocked: %s", event->achievement->title);
            if (m_event_callback) {
                m_event_callback(event);
            }
            break;

        case RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED:
            LOGD("Leaderboard submitted");
            if (m_event_callback) {
                m_event_callback(event);
            }
            break;

        case RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD:
            LOGD("Leaderboard scoreboard received");
            if (m_event_callback) {
                m_event_callback(event);
            }
            break;

        case RC_CLIENT_EVENT_GAME_COMPLETED:
            LOGD("Game completed!");
            if (m_event_callback) {
                m_event_callback(event);
            }
            break;

        case RC_CLIENT_EVENT_SERVER_ERROR:
            LOGE("Server error: %s - %s", 
                 event->server_error->api ? event->server_error->api : "Unknown API",
                 event->server_error->error_message ? event->server_error->error_message : "Unknown error");
            // These errors should be shown to the user as they won't be retried
            if (m_event_callback) {
                m_event_callback(event);
            }
            break;

        case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW:
            LOGD("Achievement challenge indicator show: %s", 
                 event->achievement ? event->achievement->title : "Unknown achievement");
            if (m_event_callback) {
                m_event_callback(event);
            }
            break;

        case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE:
            LOGD("Achievement challenge indicator hide: %s", 
                 event->achievement ? event->achievement->title : "Unknown achievement");
            if (m_event_callback) {
                m_event_callback(event);
            }
            break;

        case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW:
            LOGD("Achievement progress indicator show: %s", 
                 event->achievement ? event->achievement->title : "Unknown achievement");
            if (m_event_callback) {
                m_event_callback(event);
            }
            break;

        case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE:
            LOGD("Achievement progress indicator hide");
            if (m_event_callback) {
                m_event_callback(event);
            }
            break;

        case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE:
            LOGD("Achievement progress indicator update: %s", 
                 event->achievement ? event->achievement->title : "Unknown achievement");
            if (m_event_callback) {
                m_event_callback(event);
            }
            break;

        case RC_CLIENT_EVENT_RESET:
            LOGD("RetroAchievements requesting emulator reset (hardcore mode enabled)");
            // Reset the emulator as requested by the runtime
            YabauseReset();
            // Notify the client that reset is complete
            rc_client_reset(m_client);
            LOGD("Emulator reset complete");
            break;

        default:
            // Handle other event types as needed
            LOGD("Unhandled event type: %d", event->type);
            break;
    }
}

void Integration::onLoginComplete(int result, const char* error_message) {
    if (result == RC_OK) {
        LOGD("Login successful");
        
        // Get user information following rcheevos guidelines
        const rc_client_user_t* user = rc_client_get_user_info(m_client);
        if (user) {
            LOGD("Login verified: User %s (display: %s) is now logged in with %u points", 
                 user->username, user->display_name, user->score);
            
            if (m_login_callback) {
                // Call login callback with user info (display_name, username, score)
                m_login_callback(true, user->username, user->display_name, user->score, nullptr);
            }
        } else {
            LOGE("Login callback called but user info not available yet");
            if (m_login_callback) {
                // Fallback if user info not available
                m_login_callback(true, "Unknown", "Unknown", 0, nullptr);
            }
        }
    } else {
        LOGE("Login failed: %s", error_message ? error_message : "Unknown error");
        
        // Call login callback with error
        if (m_login_callback) {
            m_login_callback(false, nullptr, nullptr, 0, error_message ? error_message : "Unknown error");
        }
    }
}

void Integration::onGameLoadComplete(int result, const char* error_message) {
    if (result == RC_OK) {
        LOGD("Game loaded successfully");
        const rc_client_game_t* game = rc_client_get_game_info(m_client);
        if (game) {
            LOGD("Game: %s (ID: %d)", game->title, game->id);
            
            // Get detailed achievement state information for debugging
            rc_client_achievement_list_t* achievements = rc_client_create_achievement_list(m_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
            if (achievements) {
                LOGD("Achievement buckets: %d", achievements->num_buckets);
                for (uint32_t i = 0; i < achievements->num_buckets; i++) {
                    const auto& bucket = achievements->buckets[i];
                    LOGD("Bucket %d: type=%d, label='%s', achievements=%d", 
                         i, bucket.bucket_type, bucket.label ? bucket.label : "NULL", bucket.num_achievements);
                    
                    // Log details for first few achievements in each bucket
                    for (uint32_t j = 0; j < bucket.num_achievements && j < 3; j++) {
                        const auto* achievement = bucket.achievements[j];
                        LOGD("  Achievement %d: '%s' - unlocked=%d, state=%d, unlock_time=%ld", 
                             achievement->id, achievement->title, achievement->unlocked, 
                             achievement->state, (long)achievement->unlock_time);
                    }
                }
                rc_client_destroy_achievement_list(achievements);
            }
            
            // Explicitly sync user progress for this game to ensure server state is current
            // We'll show the game placard after sync completes to ensure accurate achievement counts
            LOGD("Requesting user progress sync for console Saturn (console_id: %d)", RC_CONSOLE_SATURN);
            rc_client_begin_fetch_all_user_progress(m_client, RC_CONSOLE_SATURN,
                [](int result, const char* error_message, rc_client_all_user_progress_t* list, rc_client_t* client, void* userdata) {
                    Integration* instance = static_cast<Integration*>(userdata);
                    instance->onUserProgressSyncComplete(result, error_message, list);
                }, 
                this
            );
            
            // Get leaderboard count (for debugging)
            rc_client_leaderboard_list_t* leaderboards = rc_client_create_leaderboard_list(m_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_NONE);
            if (leaderboards) {
                LOGD("Leaderboards: %d", leaderboards->num_buckets > 0 ? leaderboards->buckets[0].num_leaderboards : 0);
                rc_client_destroy_leaderboard_list(leaderboards);
            }
        }
    } else {
        LOGE("Game load failed: %s", error_message ? error_message : "Unknown error");
    }
}

void Integration::onUserProgressSyncComplete(int result, const char* error_message, rc_client_all_user_progress_t* list) {
    if (result == RC_OK) {
        LOGD("User progress sync completed successfully");
        
        if (list && list->entries) {
            LOGD("Received progress data for %d games", list->num_entries);
            
            // Find current game in the progress list
            const rc_client_game_t* game = rc_client_get_game_info(m_client);
            if (game) {
                for (uint32_t i = 0; i < list->num_entries; i++) {
                    const auto& entry = list->entries[i];
                    if (entry.game_id == game->id) {
                        LOGD("Found progress for current game %d: %u/%u achievements unlocked (%u hardcore)", 
                             entry.game_id, entry.num_unlocked_achievements, entry.num_achievements, 
                             entry.num_unlocked_achievements_hardcore);
                        break;
                    }
                }
            }
            
            // Cleanup the progress list
            rc_client_destroy_all_user_progress(list);
        }
        
        // After sync, log achievement state again to see if it changed
        LOGD("Achievement state after user progress sync:");
        rc_client_achievement_list_t* achievements = rc_client_create_achievement_list(m_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
        if (achievements) {
            for (uint32_t i = 0; i < achievements->num_buckets; i++) {
                const auto& bucket = achievements->buckets[i];
                LOGD("Post-sync Bucket %d: type=%d, label='%s', achievements=%d", 
                     i, bucket.bucket_type, bucket.label ? bucket.label : "NULL", bucket.num_achievements);
                
                // Log details for first few achievements in each bucket
                for (uint32_t j = 0; j < bucket.num_achievements && j < 3; j++) {
                    const auto* achievement = bucket.achievements[j];
                    LOGD("  Post-sync Achievement %d: '%s' - unlocked=%d, state=%d, unlock_time=%ld", 
                         achievement->id, achievement->title, achievement->unlocked, 
                         achievement->state, (long)achievement->unlock_time);
                }
            }
            rc_client_destroy_achievement_list(achievements);
        }
        
        // Now show the game placard with potentially updated achievement counts
        LOGD("Showing game placard after user progress sync");
        showGamePlacard();
    } else {
        LOGE("User progress sync failed: %s", error_message ? error_message : "Unknown error");
    }
}

void Integration::showGamePlacard() {
    if (!m_client || !m_game_placard_callback) {
        return;
    }
    
    const rc_client_game_t* game = rc_client_get_game_info(m_client);
    if (!game) {
        LOGD("No game info available for placard");
        return;
    }
    
    // Get user game summary following rcheevos guidelines
    rc_client_user_game_summary_t summary;
    rc_client_get_user_game_summary(m_client, &summary);
    
    LOGD("Game placard: %s - %u/%u achievements unlocked (%u points)", 
         game->title, summary.num_unlocked_achievements, summary.num_core_achievements, summary.points_unlocked);
    
    LOGD("User game summary details: core=%u, unofficial=%u, unlocked=%u, unsupported=%u, points_core=%u, points_unlocked=%u",
         summary.num_core_achievements, summary.num_unofficial_achievements, summary.num_unlocked_achievements, 
         summary.num_unsupported_achievements, summary.points_core, summary.points_unlocked);
         
    // Debug hardcore mode and unlock bit checking
    int current_hardcore = rc_client_get_hardcore_enabled(m_client);
    LOGD("Current hardcore mode: %s", current_hardcore ? "enabled" : "disabled");
    
    // Check individual achievements to see their unlock states
    rc_client_achievement_list_t* debug_achievements = rc_client_create_achievement_list(m_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
    if (debug_achievements) {
        for (uint32_t i = 0; i < debug_achievements->num_buckets; i++) {
            const auto& bucket = debug_achievements->buckets[i];
            for (uint32_t j = 0; j < bucket.num_achievements; j++) {
                const auto* achievement = bucket.achievements[j];
                if (achievement->unlocked != 0) {
                    LOGD("Unlocked Achievement %d: '%s' - unlocked=0x%02x (hardcore_bit=%s, softcore_bit=%s), points=%u", 
                         achievement->id, achievement->title, achievement->unlocked,
                         (achievement->unlocked & 0x02) ? "set" : "unset",  // RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE
                         (achievement->unlocked & 0x01) ? "set" : "unset",  // RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE
                         achievement->points);
                }
            }
        }
        rc_client_destroy_achievement_list(debug_achievements);
    }
    
    // Get game image URL
    char image_url[256] = {0};
    bool has_image = (rc_client_game_get_image_url(game, image_url, sizeof(image_url)) == RC_OK);
    
    // Call the game placard callback with all the information
    m_game_placard_callback(
        game->title,
        has_image ? image_url : nullptr,
        summary.num_unlocked_achievements,
        summary.num_core_achievements,
        summary.points_unlocked,
        summary.points_core,
        summary.num_unsupported_achievements > 0
    );
}

void Integration::setEventCallback(EventCallback callback) {
    m_event_callback = callback;
}

void Integration::setServerCallback(ServerCallback callback) {
    m_server_callback = callback;
}

void Integration::setLoginCallback(LoginCallback callback) {
    m_login_callback = callback;
}

void Integration::setGamePlacardCallback(GamePlacardCallback callback) {
    m_game_placard_callback = callback;
}

const rc_client_user_t* Integration::getUserInfo() const {
    if (!m_client) {
        return nullptr;
    }
    return rc_client_get_user_info(m_client);
}

rc_client_t* Integration::getClient() const {
    return m_client;
}

void Integration::reset() const {
    if (!m_client) {
        return;
    }    
    rc_client_reset(m_client);
}

static void media_changed_callback(int result, const char* error_message, rc_client_t* client, void* userdata) {
    // On success, do nothing
    if (result == RC_OK) {
        LOGD("Media change successful");
        return;
    }

    if (result == RC_HARDCORE_DISABLED) {
        LOGD("Hardcore disabled. Unrecognized media inserted.");
        // Show notification to user that hardcore was disabled due to unrecognized media
        // This is important feedback as it affects achievement eligibility
    } else {
        if (!error_message) {
            error_message = rc_error_str(result);
        }
        LOGE("RetroAchievements media change failed: %s", error_message);
        // Show error to user - media validation failed
    }
}

bool Integration::changeMedia(const char* new_media_path) {
    if (!m_client) {
        LOGE("RetroAchievements client not initialized");
        return false;
    }

    if (!new_media_path) {
        LOGE("Invalid media path");
        return false;
    }

    LOGD("Changing media to: %s", new_media_path);
    
    // Begin media change operation using identify and change media (takes file path)
    #ifdef RC_CLIENT_SUPPORTS_HASH
    rc_client_begin_identify_and_change_media(m_client, new_media_path, nullptr, 0, media_changed_callback, nullptr);
    #else
    LOGE("Media change not supported - RC_CLIENT_SUPPORTS_HASH not defined");
    return false;
    #endif
    
    return true;
}

size_t Integration::getProgressSize() const {
    if (!m_client) {
        LOGD("RetroAchievements client not initialized");
        return 0;
    }

    size_t size = rc_client_progress_size(m_client);
    LOGD("RetroAchievements progress size: %zu bytes", size);
    return size;
}

bool Integration::serializeProgress(uint8_t* buffer, size_t buffer_size) const {
    if (!m_client) {
        LOGE("RetroAchievements client not initialized");
        return false;
    }

    if (!buffer) {
        LOGE("Invalid buffer for serialization");
        return false;
    }

    int result = rc_client_serialize_progress_sized(m_client, buffer, buffer_size);
    if (result == RC_OK) {
        LOGD("RetroAchievements progress serialized successfully (%zu bytes)", buffer_size);
        return true;
    } else {
        LOGE("Failed to serialize RetroAchievements progress: %d", result);
        return false;
    }
}

bool Integration::deserializeProgress(const uint8_t* buffer, size_t buffer_size) {
    if (!m_client) {
        LOGE("RetroAchievements client not initialized");
        return false;
    }

    int result;
    if (buffer == nullptr) {
        // Reset state by calling with NULL (as per rcheevos documentation)
        LOGD("Resetting RetroAchievements progress (no save state data)");
        result = rc_client_deserialize_progress_sized(m_client, nullptr, 0);
    } else {
        LOGD("Deserializing RetroAchievements progress (%zu bytes)", buffer_size);
        result = rc_client_deserialize_progress_sized(m_client, buffer, buffer_size);
    }

    if (result == RC_OK) {
        LOGD("RetroAchievements progress deserialized successfully");
        return true;
    } else {
        LOGE("Failed to deserialize RetroAchievements progress: %d", result);
        return false;
    }
}

} // namespace YabauseRA

// C wrapper functions for save state functionality
extern "C" {

const char* YabauseRA::Integration::getAchievementBadgeURL(int achievementId, int state) {
    if (!m_client) {
        return nullptr;
    }
    
    // Get the achievement info
    const rc_client_achievement_t* achievement = rc_client_get_achievement_info(m_client, achievementId);
    if (!achievement) {
        return nullptr;
    }
    
    // Use a static buffer to store the URL - this is safe since rcheevos docs show this pattern
    static char url_buffer[512];
    
    // Get the image URL based on state
    // state == 2 means RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED (achievement is unlocked)
    // state == 1 means RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE (achievement is locked/active)
    int rc_state = (state == 2) ? RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED : RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE;
    
    printf("YabauseRA: getAchievementBadgeURL - achievementId=%d, input_state=%d, rc_state=%d\n", achievementId, state, rc_state);
    
    if (rc_client_achievement_get_image_url(achievement, rc_state, url_buffer, sizeof(url_buffer)) == RC_OK) {
        printf("YabauseRA: Generated badge URL: %s\n", url_buffer);
        return url_buffer;
    }
    
    printf("YabauseRA: Failed to generate badge URL for achievement %d\n", achievementId);
    
    return nullptr;
}

const char* YabauseRA::Integration::getAchievementBadgeName(int achievementId) {
    if (!m_client) {
        return nullptr;
    }
    
    // Get the achievement info
    const rc_client_achievement_t* achievement = rc_client_get_achievement_info(m_client, achievementId);
    if (!achievement) {
        return nullptr;
    }
    
    // Return the badge name from the achievement structure
    return achievement->badge_name;
}

char* YabauseRA::Integration::createAchievementListJSON(int category, int grouping) {
    if (!m_client) {
        LOGE("Client not initialized");
        return nullptr;
    }
    
    // Create achievement list using rcheevos
    rc_client_achievement_list_t* rc_list = rc_client_create_achievement_list(
        m_client, 
        static_cast<uint32_t>(category), 
        static_cast<uint32_t>(grouping)
    );
    
    if (!rc_list) {
        LOGE("Failed to create achievement list");
        return nullptr;
    }
    
    LOGD("Created achievement list with %u buckets", rc_list->num_buckets);
    
    // Build JSON string using simple concatenation for safety
    std::string json = "{\"buckets\":[";
    
    for (uint32_t i = 0; i < rc_list->num_buckets; i++) {
        if (i > 0) json += ",";
        
        const auto& bucket = rc_list->buckets[i];
        json += "{";
        json += "\"label\":\"" + escapeJsonString(bucket.label) + "\",";
        json += "\"achievements\":[";
        
        for (uint32_t j = 0; j < bucket.num_achievements; j++) {
            if (j > 0) json += ",";
            
            const rc_client_achievement_t* achievement = bucket.achievements[j];
            json += "{";
            json += "\"id\":" + std::to_string(achievement->id) + ",";
            json += "\"title\":\"" + escapeJsonString(achievement->title) + "\",";
            json += "\"description\":\"" + escapeJsonString(achievement->description) + "\",";
            json += "\"badge_name\":\"" + escapeJsonString(achievement->badge_name) + "\",";
            json += "\"points\":" + std::to_string(achievement->points) + ",";
            json += "\"state\":" + std::to_string(static_cast<int>(achievement->state)) + ",";
            json += "\"unlocked\":" + std::to_string(achievement->unlocked) + ",";
            json += "\"unlock_time\":" + std::to_string(achievement->unlock_time) + ",";
            json += "\"measured_percent\":" + std::to_string(achievement->measured_percent) + ",";
            json += "\"measured_progress\":\"" + escapeJsonString(achievement->measured_progress) + "\"";
            json += "}";
            
            LOGD("  Achievement %u: ID=%d, Title='%s', Points=%d, State=%d, Unlocked=%d", 
                 j, achievement->id, achievement->title, achievement->points, 
                 static_cast<int>(achievement->state), achievement->unlocked);
        }
        
        json += "]}";
    }
    
    json += "]}";
    
    // Clean up the original rcheevos list
    rc_client_destroy_achievement_list(rc_list);
    
    // Allocate and copy JSON string for C caller
    char* result = static_cast<char*>(malloc(json.length() + 1));
    if (result) {
        strcpy(result, json.c_str());
        LOGD("Successfully created achievement list JSON (%zu chars)", json.length());
    }
    
    return result;
}

std::string YabauseRA::Integration::escapeJsonString(const char* str) {
    if (!str) return "";
    
    std::string result;
    for (const char* p = str; *p; ++p) {
        switch (*p) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += *p; break;
        }
    }
    return result;
}

}