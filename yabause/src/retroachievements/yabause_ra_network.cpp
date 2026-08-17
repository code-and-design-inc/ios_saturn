/*
YabaSanshiro RetroAchievements Network Interface
Copyright 2025 YabaSanshiro Team

This file implements HTTP request handling for RetroAchievements API communication.
*/

#include "yabause_ra_integration.h"
#include "rc_client.h"
#include <string>
#include <memory>

#ifdef ANDROID
#include <android/log.h>
#define LOG_TAG "YabauseRANetwork"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <stdio.h>
#define LOGD(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define LOGE(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#endif

namespace YabauseRA {

/**
 * HTTP request handler implementation
 * This forwards requests to the platform-specific HTTP implementation
 */
void handleHttpRequest(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callback_data) {
    if (!request || !callback) {
        LOGE("Invalid request or callback");
        if (callback) {
            rc_api_server_response_t response = {0};
            response.http_status_code = 0; // Error status
            callback(&response, callback_data);
        }
        return;
    }

    LOGD("HTTP Request: %s", request->url);
    
    // On Android, this will be handled by the JNI layer which forwards to Java HTTP client
    // On other platforms, we would implement platform-specific HTTP handling here
    
    Integration* integration = Integration::getInstance();
    if (integration && integration->isInitialized()) {
        // Forward to the server callback which should be set by the platform layer
        integration->handleServerRequest(request, callback, callback_data);
    } else {
        LOGE("Integration not initialized for HTTP request");
        rc_api_server_response_t response = {0};
        response.http_status_code = 0; // Error status
        callback(&response, callback_data);
    }
}

/**
 * Initialize network handling
 * Platform-specific network setup can be done here
 */
bool initializeNetwork() {
    LOGD("Initializing network handling...");
    
    // For Android, network initialization is handled by the Java layer
    // For other platforms, SSL/network library initialization would go here
    
    return true;
}

/**
 * Shutdown network handling
 */
void shutdownNetwork() {
    LOGD("Shutting down network handling...");
    
    // Platform-specific cleanup would go here
}

/**
 * Process HTTP response
 * This is called from the platform layer when an HTTP response is received
 */
void processHttpResponse(rc_client_server_callback_t callback, void* callback_data, 
                        int http_code, const char* response_body, size_t response_size) {
    if (!callback) {
        LOGE("Invalid callback for HTTP response");
        return;
    }
    
    LOGD("HTTP Response: code=%d, size=%zu", http_code, response_size);
    
    rc_api_server_response_t response = {0};
    response.http_status_code = http_code;
    
    if (http_code == 200 && response_body && response_size > 0) {
        // Success - forward response to rc_client
        response.body = response_body;
        response.body_length = response_size;
    } else {
        // Error - notify rc_client of failure
        LOGE("HTTP request failed with code %d", http_code);
        response.body = nullptr;
        response.body_length = 0;
    }
    
    callback(&response, callback_data);
}

} // namespace YabauseRA