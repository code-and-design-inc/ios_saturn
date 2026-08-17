/*
YabaSanshiro RetroAchievements Game Mapper
Copyright 2025 YabaSanshiro Team

This file implements Saturn-specific game identification and mapping for RetroAchievements.
*/

#include "yabause_ra_integration.h"
#include "rc_hash.h"
#include <cstring>
#include <string>

#ifdef ANDROID
#include <android/log.h>
#define LOG_TAG "YabauseRAGameMapper"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <stdio.h>
#define LOGD(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define LOGE(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#endif

namespace YabauseRA {

/**
 * Saturn CD structure for game identification
 */
struct SaturnGameInfo {
    std::string product_code;
    std::string version;
    std::string release_date;
    std::string disc_info;
    std::string game_title;
};

/**
 * Extract game information from Saturn CD image
 * This reads the IP.BIN header from the Saturn disc
 */
bool extractSaturnGameInfo(const void* game_data, size_t data_size, SaturnGameInfo& info) {
    if (!game_data || data_size < 0x100) {
        LOGE("Invalid game data for Saturn game info extraction");
        return false;
    }

    const uint8_t* data = static_cast<const uint8_t*>(game_data);
    
    // Saturn IP.BIN header starts at offset 0x0
    // Check for Saturn disc signature
    const char saturn_signature[] = "SEGA SEGASATURN ";
    if (memcmp(data, saturn_signature, 16) != 0) {
        LOGD("Not a valid Saturn disc image");
        return false;
    }

    // Extract product code (offset 0x20, 10 bytes)
    char product_code[11] = {0};
    memcpy(product_code, data + 0x20, 10);
    info.product_code = std::string(product_code).substr(0, 10);
    
    // Extract version (offset 0x2A, 6 bytes)  
    char version[7] = {0};
    memcpy(version, data + 0x2A, 6);
    info.version = std::string(version).substr(0, 6);
    
    // Extract release date (offset 0x30, 8 bytes)
    char release_date[9] = {0};
    memcpy(release_date, data + 0x30, 8);
    info.release_date = std::string(release_date).substr(0, 8);
    
    // Extract disc info (offset 0x38, 8 bytes)
    char disc_info[9] = {0};
    memcpy(disc_info, data + 0x38, 8);
    info.disc_info = std::string(disc_info).substr(0, 8);
    
    // Extract game title (offset 0x60, 112 bytes)
    char game_title[113] = {0};
    memcpy(game_title, data + 0x60, 112);
    info.game_title = std::string(game_title).substr(0, 112);
    
    // Clean up strings (remove trailing spaces/nulls)
    auto trim = [](std::string& str) {
        str.erase(str.find_last_not_of(" \t\n\r\f\v\0") + 1);
    };
    
    trim(info.product_code);
    trim(info.version);
    trim(info.release_date);
    trim(info.disc_info);
    trim(info.game_title);
    
    LOGD("Saturn Game Info:");
    LOGD("  Product Code: %s", info.product_code.c_str());
    LOGD("  Version: %s", info.version.c_str());
    LOGD("  Release Date: %s", info.release_date.c_str());
    LOGD("  Disc Info: %s", info.disc_info.c_str());
    LOGD("  Title: %s", info.game_title.c_str());
    
    return true;
}

/**
 * Generate RetroAchievements hash for Saturn game
 * This uses the rcheevos library to generate a consistent hash
 */
std::string generateSaturnGameHash(const void* game_data, size_t data_size, const char* filename) {
    if (!game_data || data_size == 0) {
        LOGE("Invalid game data for hash generation");
        return "";
    }

    LOGD("Generating hash for Saturn game: %s (size: %zu)", filename ? filename : "unknown", data_size);
    
    // Use rc_hash to generate the game hash
    char hash[33] = {0}; // MD5 hash is 32 characters + null terminator
    
    if (rc_hash_generate_from_buffer(hash, RC_CONSOLE_SATURN, 
                                    static_cast<const uint8_t*>(game_data), data_size)) {
        LOGD("Generated hash: %s", hash);
        return std::string(hash);
    } else {
        LOGE("Failed to generate hash for Saturn game");
        return "";
    }
}

/**
 * Map Saturn game for RetroAchievements
 * This combines game identification and hash generation
 */
bool mapSaturnGame(const void* game_data, size_t data_size, const char* filename,
                   std::string& game_hash, std::string& game_id) {
    if (!game_data || data_size == 0) {
        return false;
    }
    
    LOGD("Mapping Saturn game: %s", filename ? filename : "unknown");
    
    // Extract game information
    SaturnGameInfo info;
    if (!extractSaturnGameInfo(game_data, data_size, info)) {
        LOGE("Failed to extract Saturn game information");
        return false;
    }
    
    // Generate hash for RetroAchievements
    game_hash = generateSaturnGameHash(game_data, data_size, filename);
    if (game_hash.empty()) {
        LOGE("Failed to generate game hash");
        return false;
    }
    
    // Use product code as game identifier
    game_id = info.product_code;
    if (game_id.empty()) {
        // Fallback to filename if no product code
        if (filename) {
            game_id = std::string(filename);
            // Remove path and extension
            size_t last_slash = game_id.find_last_of("/\\");
            if (last_slash != std::string::npos) {
                game_id = game_id.substr(last_slash + 1);
            }
            size_t last_dot = game_id.find_last_of('.');
            if (last_dot != std::string::npos) {
                game_id = game_id.substr(0, last_dot);
            }
        } else {
            game_id = "unknown_saturn_game";
        }
    }
    
    LOGD("Game mapping complete - ID: %s, Hash: %s", game_id.c_str(), game_hash.c_str());
    return true;
}

/**
 * Validate Saturn disc image
 * Check if the provided data is a valid Saturn disc
 */
bool isSaturnDisc(const void* game_data, size_t data_size) {
    if (!game_data || data_size < 0x100) {
        return false;
    }
    
    const uint8_t* data = static_cast<const uint8_t*>(game_data);
    const char saturn_signature[] = "SEGA SEGASATURN ";
    
    return memcmp(data, saturn_signature, 16) == 0;
}

} // namespace YabauseRA