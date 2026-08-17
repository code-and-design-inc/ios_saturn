# RetroAchievements Integration Design Document

## Overview

This document describes the architecture and design of the RetroAchievements integration for YabaSanshiro Android app. The integration follows the rcheevos client library guidelines and provides seamless achievement tracking, leaderboards, and rich presence functionality.

## Architecture

### Component Hierarchy

```
Yabause.kt (Main Activity)
    ↓
RetroAchievementsAuthManager (Authentication & State Management)
    ↓
RetroAchievementsManager (Core Integration & UI Callbacks)
    ↓
Native C++ Integration (yabause_ra_integration.cpp)
    ↓
rcheevos Library (Client-based architecture)
```

## Core Components

### 1. RetroAchievementsAuthManager

**Location**: `org.uoyabause.android.auth.RetroAchievementsAuthManager`

**Responsibilities**:
- User authentication (login/logout)
- Login state management
- Credential storage and auto-login
- Integration with existing Firebase/Discord auth system

**Key Methods**:
- `loginRetroAchievements(username, password)`: Initiate login process
- `logoutRetroAchievements()`: Handle logout and cleanup
- `isRetroAchievementsLoggedIn()`: Check current login state
- `onNativeLoginComplete(success, username)`: Handle native callback completion

**Design Patterns**:
- Singleton pattern for global access
- Observer pattern for auth state changes
- Coroutines for asynchronous operations

### 2. RetroAchievementsManager

**Location**: `org.uoyabause.android.achievements.RetroAchievementsManager`

**Responsibilities**:
- HTTP request handling for RetroAchievements API
- Achievement notifications and UI callbacks
- Hardcore mode management
- Game-specific settings persistence
- JNI bridge to native integration

**Key Methods**:
- `initialize()`: Initialize JNI and native integration
- `loginUser(username, password, callback)`: Handle login requests
- `setHardcoreEnabled(enabled)`: Control hardcore mode with login state validation
- `isUserLoggedIn()`: Check login state via AuthManager
- `onLoginStateChanged(isLoggedIn)`: Handle login state changes

**Design Patterns**:
- Singleton pattern for global access
- Callback pattern for async operations
- Bridge pattern for JNI communication

### 3. Native Integration

**Location**: `yabause/src/retroachievements/yabause_ra_integration.cpp`

**Responsibilities**:
- Saturn memory system integration
- rcheevos client management
- Achievement condition evaluation
- Memory read/write hooks

**Key Classes**:
- `YabauseRA::Integration`: Main integration class
- Memory callback functions for Saturn emulator integration

## Data Flow

### Login Process

```
1. User enters credentials in UI
2. Yabause.kt → RetroAchievementsAuthManager.loginRetroAchievements()
3. AuthManager → RetroAchievementsManager.loginUser()
4. Manager → Native JNI → rcheevos login
5. Native callback → Manager.onLoginComplete()
6. Manager → AuthManager.onNativeLoginComplete()
7. AuthManager updates state and triggers game loading
```

### Achievement Unlock Process

```
1. Saturn emulator memory change
2. Native integration reads memory via callbacks
3. rcheevos evaluates achievement conditions
4. Achievement triggered → Native event callback
5. JNI callback → RetroAchievementsManager.onAchievementUnlocked()
6. Manager → UI notification system
7. Display achievement popup/notification
```

### Hardcore Mode Control

```
1. Check login state via RetroAchievementsAuthManager.isRetroAchievementsLoggedIn()
2. If not logged in → Force hardcore mode OFF
3. If logged in → Apply user/game-specific settings
4. Save settings per-game in SharedPreferences
5. On logout → Force hardcore mode OFF and clear active achievements
```

## State Management

### Login State Synchronization

The system maintains login state in multiple layers:

1. **Android Layer**: `RetroAchievementsAuthManager.isRetroAchievementsLoggedIn`
2. **Native Layer**: `rc_client_get_user_info()` result
3. **Truth Source**: Android layer is authoritative

**Why Android is authoritative**:
- Handles UI state consistency
- Manages credential storage
- Controls logout timing
- Integrates with existing auth systems

### Hardcore Mode State

Hardcore mode is controlled by:
1. **User preference**: Saved per-game in SharedPreferences
2. **Login state**: Automatically disabled when not logged in
3. **Native enforcement**: Managed by rcheevos client

**State Validation Rules**:
- `setHardcoreEnabled(true)` requires user to be logged in
- `isHardcoreEnabled()` returns false if not logged in (regardless of native state)
- Game load applies saved settings only if user is logged in
- Logout forces hardcore mode off

## Error Handling

### Network Errors

```kotlin
// HTTP requests include retry logic and timeout handling
try {
    // HTTP request
} catch (e: IOException) {
    Log.e(TAG, "Network error", e)
    callback.invoke(false, "Network error: ${e.message}")
}
```

### JNI Errors

```kotlin
// All JNI calls are wrapped in try-catch
try {
    nativeMethod()
} catch (e: UnsatisfiedLinkError) {
    Log.e(TAG, "JNI error", e)
    return false
}
```

### State Consistency

- Login state is validated before sensitive operations
- Hardcore mode is re-validated on game load and state changes
- Native callbacks include error checking and logging

## Configuration

### SharedPreferences Keys

**Global Settings** (`retroachievements_auth`):
- `ra_username`: Stored username
- `ra_api_token`: Stored API token/password
- `ra_auto_login`: Auto-login preference

**Per-Game Settings** (game code as preference name):
- `ra_hardcore_mode`: Hardcore mode setting for specific game

### Build Configuration

**ProGuard Rules** (`proguard-rules.pro`):
```proguard
-keep class org.uoyabause.android.achievements.RetroAchievementsManager { *; }
-keep class org.uoyabause.android.achievements.RetroAchievementsManager$* { *; }
-keepclassmembers class org.uoyabause.android.achievements.RetroAchievementsManager {
    public void onAchievementUnlocked(...);
    public void onLoginComplete(...);
    // ... other JNI callback methods
}
```

**CMakeLists.txt**:
```cmake
# RetroAchievements support
option(HAVE_RETROACHIEVEMENTS "Enable RetroAchievements support" ON)
if(HAVE_RETROACHIEVEMENTS)
    set( SOURCES ${SOURCES}
        jni/retroachievements_android.cpp
        jni/retroachievements_android.h
    )
endif()
```

## UI Integration

### Notification System

**Components**:
- `RetroAchievementsNotification`: Achievement unlock notifications
- `RetroAchievementsOverlay`: In-game overlay system
- `RetroAchievementsSettingsFragment`: Settings UI

**Design Principles**:
- Non-intrusive notifications that don't disrupt gameplay
- Consistent with rcheevos UI guidelines
- Configurable notification types and timing

### Settings Integration

**Integration Points**:
- Main settings menu includes RetroAchievements section
- Per-game hardcore mode settings
- Login/logout controls
- Notification preferences

## Performance Considerations

### Memory Management

- Native integration uses minimal memory footprint
- Achievement data is lazily loaded
- HTTP responses are streamed for large data
- UI notifications are recycled

### Threading

- Network operations use background threads (Coroutines)
- JNI callbacks execute on main thread
- Native integration runs on emulator thread
- UI updates are dispatched to main thread

### Caching

- Achievement data is cached in native layer
- User progress is synchronized periodically
- Network requests include proper cache headers

## Security

### Credential Storage

```kotlin
// TODO: Migrate to Android Keystore for production
private fun saveCredentials(username: String, password: String) {
    preferences.edit()
        .putString(KEY_USERNAME, username)
        .putString(KEY_API_TOKEN, password) // Should use Android Keystore
        .apply()
}
```

### Network Security

- All API requests use HTTPS
- Request/response data is validated
- No sensitive data in logs (production builds)

## Testing

### Unit Testing

**Testable Components**:
- `RetroAchievementsAuthManager` login logic
- `RetroAchievementsManager` HTTP handling
- Hardcore mode state validation

**Mock Dependencies**:
- Network layer for API testing
- SharedPreferences for state testing
- JNI layer for integration testing

### Integration Testing

**Test Scenarios**:
- Login/logout flows
- Achievement unlock simulation
- Hardcore mode state transitions
- Network error handling

## Debugging

### Logging Strategy

**Log Tags**:
- `RAAuthManager`: Authentication operations
- `RetroAchievements`: Core manager operations
- `YabauseRA`: Native integration (C++)

**Log Levels**:
- `DEBUG`: Normal operation flow
- `INFO`: Important state changes
- `WARN`: Recoverable errors
- `ERROR`: Critical failures

**Key Debug Points**:
```kotlin
// Login state validation
Log.d(TAG, "Login status check: Android=$isLoggedIn")

// Hardcore mode changes
Log.d(TAG, "Setting hardcore mode: $enabled (gameCode: $currentGameCode)")

// Network operations
Log.d(TAG, "HTTP request: ${request.url}")
```

## Future Improvements

### Short Term

1. **Enhanced Security**: Migrate to Android Keystore for credential storage
2. **Better Error Handling**: More granular error types and recovery strategies
3. **Performance Optimization**: Reduce JNI call overhead

### Long Term

1. **Offline Mode**: Cache achievement data for offline play
2. **Social Features**: Friend system and social leaderboards
3. **Advanced Analytics**: Achievement progression tracking
4. **Multi-Platform Sync**: Share progress across platforms

## Maintenance Guidelines

### Code Style

- Follow Android/Kotlin conventions
- Use meaningful variable and method names
- Include comprehensive documentation
- Maintain consistent error handling patterns

### Adding New Features

1. **Update this design document** with new architecture decisions
2. **Add appropriate logging** for debugging
3. **Include error handling** for all failure cases
4. **Update ProGuard rules** if adding new JNI methods
5. **Add unit tests** for new functionality

### Common Pitfalls

1. **Login State Sync**: Always use `RetroAchievementsAuthManager` as truth source
2. **Hardcore Mode**: Validate login state before enabling
3. **JNI Package Paths**: Update all references when moving classes
4. **Memory Leaks**: Properly cleanup native resources and callbacks
5. **Thread Safety**: Ensure UI updates happen on main thread

### Debugging Common Issues

**"UnsatisfiedLinkError"**:
- Check JNI function signatures match exactly
- Verify package paths in native function names
- Ensure ProGuard rules preserve JNI methods

**"Login state inconsistency"**:
- Verify `RetroAchievementsAuthManager` is being used consistently
- Check `onNativeLoginComplete` is being called properly
- Validate logout cleanup is complete

**"Hardcore mode re-enabled after logout"**:
- Ensure `isUserLoggedIn()` uses Android auth manager
- Check game load sequence calls login state validation
- Verify logout triggers `onLoginStateChanged(false)`

## Dependencies

### External Libraries

- **rcheevos**: Core RetroAchievements client library
- **OkHttp**: HTTP client for API requests
- **Kotlin Coroutines**: Asynchronous programming
- **Harmony SharedPreferences**: Per-game settings storage

### Internal Dependencies

- **YabauseRunnable**: Main emulator integration
- **YabauseAudio**: Audio system integration
- **Firebase Auth**: Existing authentication system

## API Reference

### RetroAchievements Web API

- **Base URL**: `https://retroachievements.org/API/`
- **Authentication**: Username/Password or API Token
- **Rate Limiting**: Respect API rate limits
- **Documentation**: [RetroAchievements API Docs](https://api-docs.retroachievements.org/)

### rcheevos Client API

- **Event Callbacks**: Achievement unlocks, leaderboard updates
- **Memory Interface**: Read/write Saturn memory
- **State Management**: Save/load achievement progress
- **Documentation**: [rcheevos GitHub](https://github.com/RetroAchievements/rcheevos)

---

**Last Updated**: 2025-08-03
**Version**: 1.0
**Author**: Claude Code AI Assistant