package org.uoyabause.android.auth

import android.content.Context
import android.content.SharedPreferences
import android.util.Log
import com.google.firebase.auth.FirebaseUser
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.uoyabause.android.YabauseRunnable
import org.uoyabause.android.achievements.RetroAchievementsManager
import java.io.File

/**
 * RetroAchievements Authentication Manager
 * Integrates RetroAchievements login with existing Firebase/Discord authentication
 * Following "Pattern 2: Integrated Authentication System" from ra_plan.md
 * Uses global SharedPreferences (independent of Firebase/Google account)
 */
class RetroAchievementsAuthManager(
    private val context: Context,
) {
    companion object {
        private const val TAG = "RAAuthManager"
        private const val PREFS_NAME = "retroachievements_auth"
        private const val GLOBAL_PREFS_NAME = "retroachievements_auth_global"
        private const val KEY_USERNAME = "ra_username"
        private const val KEY_API_TOKEN = "ra_api_token"
        private const val KEY_AUTO_LOGIN = "ra_auto_login"

        // Statistics cache preferences
        private const val STATS_PREF_PREFIX = "retroachievements_stats_"

        fun getStatsPreferenceName(username: String): String = STATS_PREF_PREFIX + username

        @Volatile
        private var instance: RetroAchievementsAuthManager? = null

        fun getInstance(context: Context): RetroAchievementsAuthManager = instance ?: synchronized(this) {
            instance ?: RetroAchievementsAuthManager(context.applicationContext).also { instance = it }
        }
    }

    private val firebaseAuthManager = FirebaseAuthManager(context)
    private val retroAchievementsManager = RetroAchievementsManager.getInstance(context)
    private val authScope = CoroutineScope(Dispatchers.Main + SupervisorJob())

    // Authentication state
    private var isRetroAchievementsLoggedIn = false
    private var currentRAUsername: String? = null
    private var yabaseActivity: org.uoyabause.android.Yabause? = null
    private var lastKnownFirebaseUid: String? = null

    // Callbacks
    var onAuthStateChanged: ((Boolean, String?) -> Unit)? = null
    var onLoginResult: ((Boolean, String?) -> Unit)? = null
    var onUIUpdateRequired: (() -> Unit)? = null

    init {
        // Perform one-time migration from old storage format
        performOldDataMigration()
    }

    /**
     * Get global SharedPreferences for RetroAchievements data
     * Independent of Firebase/Google account
     * @return SharedPreferences for RA authentication
     */
    private fun getGlobalPrefs(): SharedPreferences =
        context.getSharedPreferences(GLOBAL_PREFS_NAME, Context.MODE_PRIVATE)

    /**
     * Check if Firebase user has switched
     * RA login state is now independent of Firebase user, so no clearing is needed
     * @param currentFirebaseUid Current Firebase user ID
     */
    private fun checkAndHandleUserSwitch(currentFirebaseUid: String) {
        if (lastKnownFirebaseUid != null && lastKnownFirebaseUid != currentFirebaseUid) {
            Log.d(TAG, "Firebase user switched from $lastKnownFirebaseUid to $currentFirebaseUid (RA state preserved)")
        }
        lastKnownFirebaseUid = currentFirebaseUid
    }

    /**
     * Clear RetroAchievements login state when user switches
     */
    private fun clearRetroAchievementsLoginState() {
        Log.d(TAG, "Clearing RetroAchievements login state due to user switch")

        isRetroAchievementsLoggedIn = false
        currentRAUsername = null

        // Notify RetroAchievements manager of logout
        try {
            retroAchievementsManager.onLoginStateChanged(false)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to notify login state change on user switch", e)
        }

        // Notify listeners of auth state change
        onAuthStateChanged?.invoke(false, null)
    }

    /**
     * Check if saved RA credentials exist and auto-login if enabled
     * Uses global prefs (independent of Firebase user)
     */
    private fun checkAndAutoLogin() {
        try {
            val prefs = getGlobalPrefs()
            val hasUsername = prefs.contains(KEY_USERNAME)
            val hasApiToken = prefs.contains(KEY_API_TOKEN)
            val hasCredentials = hasUsername && hasApiToken
            val autoLoginEnabled = prefs.getBoolean(KEY_AUTO_LOGIN, false)

            val storedUsername = prefs.getString(KEY_USERNAME, null)
            Log.d(TAG, "Auto-login check (global):")
            Log.d(TAG, "  - hasUsername=$hasUsername (value: $storedUsername)")
            Log.d(TAG, "  - hasApiToken=$hasApiToken")
            Log.d(TAG, "  - hasCredentials=$hasCredentials")
            Log.d(TAG, "  - autoLoginEnabled=$autoLoginEnabled")

            if (hasCredentials && autoLoginEnabled) {
                val username = prefs.getString(KEY_USERNAME, null)
                val password = prefs.getString(KEY_API_TOKEN, null)

                if (username != null && password != null) {
                    Log.d(TAG, "Auto-logging in RA user: $username")
                    authScope.launch {
                        val originalLoginCallback = onLoginResult
                        onLoginResult = { success, error ->
                            if (success) {
                                Log.d(TAG, "Auto-login successful, triggering UI update")
                                onUIUpdateRequired?.invoke()
                            } else {
                                Log.w(TAG, "Auto-login failed: $error")
                            }
                            onLoginResult = originalLoginCallback
                        }

                        loginRetroAchievements(username, password, false)
                    }
                } else {
                    Log.w(TAG, "Saved credentials are null")
                }
            } else {
                Log.d(TAG, "Auto-login not performed - hasCredentials=$hasCredentials, autoLoginEnabled=$autoLoginEnabled")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error checking auto-login", e)
        }
    }

    /**
     * Initialize the authentication manager
     */
    fun initialize() {
        Log.d(TAG, "Initializing RetroAchievements authentication...")

        // Initialize RetroAchievements manager
        if (!retroAchievementsManager.initialize()) {
            Log.e(TAG, "Failed to initialize RetroAchievements manager")
            return
        }

        // Set initial Firebase user ID for tracking user switches
        val currentUser = firebaseAuthManager.getCurrentUser()
        lastKnownFirebaseUid = currentUser?.uid
        Log.d(TAG, "Initial Firebase user ID: $lastKnownFirebaseUid")

        // Set up authentication state listeners
        setupAuthStateListeners()

        // Check for saved credentials and auto-login if enabled
        if (isAutoLoginEnabled() && hasSavedCredentials()) {
            autoLogin()
        }
    }

    /**
     * Login to RetroAchievements with username and password
     * @param username RA username
     * @param password RA password
     * @param rememberCredentials Whether to save credentials for auto-login (default: true)
     */
    fun loginRetroAchievements(
        username: String,
        password: String,
        rememberCredentials: Boolean = true,
    ) {
        Log.d(TAG, "Attempting RetroAchievements login for user: $username")

        authScope.launch {
            try {
                // Ensure RetroAchievements manager is initialized before login
                if (!retroAchievementsManager.initialize()) {
                    Log.e(TAG, "Failed to initialize RetroAchievements manager")
                    onLoginResult?.invoke(false, "Failed to initialize RetroAchievements")
                    return@launch
                }

                retroAchievementsManager.loginUser(username, password) { success, error ->
                    if (success) {
                        Log.d(TAG, "RetroAchievements login request submitted successfully")

                        if (rememberCredentials) {
                            saveCredentialsGlobal(username, password)
                        }

                        // IMPORTANT: Do NOT call onLoginResult here - only wait for native callback
                        // The actual login result will come via onNativeLoginComplete
                        Log.d(TAG, "Login request submitted, waiting for native login completion callback...")

                        // Set up a timeout to handle cases where onNativeLoginComplete is never called
                        authScope.launch {
                            kotlinx.coroutines.delay(10000) // 10 second timeout

                            // If we're still not logged in after 10 seconds, consider it a failure
                            if (!isRetroAchievementsLoggedIn) {
                                Log.e(TAG, "Login timeout - no response from native layer after 10 seconds")
                                withContext(Dispatchers.Main) {
                                    isRetroAchievementsLoggedIn = false
                                    currentRAUsername = null
                                    onAuthStateChanged?.invoke(false, null)
                                    onLoginResult?.invoke(false, "Login timeout")
                                }
                            }
                        }
                    } else {
                        Log.e(TAG, "RetroAchievements login request failed to submit: $error")
                        isRetroAchievementsLoggedIn = false
                        currentRAUsername = null

                        onAuthStateChanged?.invoke(false, null)
                        onLoginResult?.invoke(false, error)
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "RetroAchievements login exception", e)
                onLoginResult?.invoke(false, e.message)
            }
        }
    }

    /**
     * Logout from RetroAchievements
     */
    fun logoutRetroAchievements() {
        Log.d(TAG, "Logging out from RetroAchievements")

        isRetroAchievementsLoggedIn = false
        currentRAUsername = null

        // Notify RetroAchievements manager of logout to force disable hardcore mode
        Log.d(TAG, "Notifying RetroAchievements manager of logout")
        try {
            retroAchievementsManager.onLoginStateChanged(false)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to notify login state change on logout", e)
        }

        // Clear saved credentials if desired
        clearSavedCredentials()

        onAuthStateChanged?.invoke(false, null)
    }

    /**
     * Check if user is logged in to RetroAchievements
     */
    fun isRetroAchievementsLoggedIn(): Boolean = isRetroAchievementsLoggedIn

    /**
     * Get current RetroAchievements username
     */
    fun getCurrentRAUsername(): String? = currentRAUsername

    /**
     * Get current RetroAchievements API key/token
     * Note: This is now deprecated in favor of getUserApiTokenNative() which gets the session token
     */
    fun getCurrentRAApiKey(): String? = try {
        val prefs = getGlobalPrefs()
        prefs.getString(KEY_API_TOKEN, null)
    } catch (e: Exception) {
        android.util.Log.e(TAG, "Error getting RA API key", e)
        null
    }

    /**
     * Get comprehensive authentication status
     * Combines Firebase, Discord, and RetroAchievements status
     */
    suspend fun getAuthenticationStatus(): AuthStatus {
        val firebaseUser = firebaseAuthManager.getCurrentUser()
        val discordLinked = firebaseAuthManager.isDiscordLinked()

        return AuthStatus(
            isFirebaseLoggedIn = firebaseUser != null,
            firebaseUser = firebaseUser,
            isDiscordLinked = discordLinked,
            isRetroAchievementsLoggedIn = isRetroAchievementsLoggedIn,
            retroAchievementsUsername = currentRAUsername,
        )
    }

    /**
     * Get current game code from native layer
     */
    private fun getCurrentGameCode(): String? = try {
        YabauseRunnable.getCurrentGameCode()
    } catch (e: Exception) {
        Log.w(TAG, "Failed to get current game code: ${e.message}")
        null
    }

    /**
     * Enable/disable auto-login
     */
    fun setAutoLoginEnabled(enabled: Boolean) {
        getGlobalPrefs()
            .edit()
            .putBoolean(KEY_AUTO_LOGIN, enabled)
            .apply()
    }

    /**
     * Check if auto-login is enabled
     */
    fun isAutoLoginEnabled(): Boolean = getGlobalPrefs().getBoolean(KEY_AUTO_LOGIN, false)

    /**
     * Set the Yabause activity for game path access
     * This should be called from the main game activity
     */
    fun setYabaseActivity(activity: org.uoyabause.android.Yabause?) {
        yabaseActivity = activity
    }

    /**
     * Called when native login is actually complete
     * This ensures we only trigger game loading when rcheevos is truly ready
     */
    fun onNativeLoginComplete(
        success: Boolean,
        username: String?,
    ) {
        Log.d(TAG, "Native login completion: success=$success, username=$username")

        if (success && username != null) {
            Log.d(TAG, "Setting login state after native confirmation")
            isRetroAchievementsLoggedIn = true
            currentRAUsername = username

            // Ensure credentials are saved in global prefs after successful login
            val prefs = getGlobalPrefs()

            // Enable auto-login and save username, but preserve KEY_API_TOKEN as the
            // password — rc_client_begin_login_with_password needs the actual password
            // for auto-login on restart.
            prefs
                .edit()
                .putString(KEY_USERNAME, username)
                .putBoolean(KEY_AUTO_LOGIN, true)
                .apply()
            Log.d(TAG, "Enabled auto-login for $username")

            // Cache user statistics and avatar URL immediately after successful login
            Log.d(TAG, "About to call cacheUserStatsFromNative for: $username")
            cacheUserStatsFromNative(username)

            // Notify RetroAchievements manager of successful login
            try {
                retroAchievementsManager.onLoginStateChanged(true)
            } catch (e: Exception) {
                Log.e(TAG, "Failed to notify login state change on logi n", e)
            }

            // Now trigger auth state change with game loading
            onAuthStateChanged?.invoke(true, username)
            // Notify UI of successful login
            onLoginResult?.invoke(true, null)
        } else {
            Log.e(TAG, "Native login failed or no username provided")
            isRetroAchievementsLoggedIn = false
            currentRAUsername = null

            // Do NOT clear saved credentials on failure — the failure may be transient
            // (network error, server timeout). Credentials are only cleared on explicit
            // logout so that auto-login can retry on the next app start.
            Log.w(TAG, "Login failed - credentials preserved for future auto-login attempts")

            // Notify RetroAchievements manager of login failure
            try {
                retroAchievementsManager.onLoginStateChanged(false)
            } catch (e: Exception) {
                Log.e(TAG, "Failed to notify login state change on login failure", e)
            }

            onAuthStateChanged?.invoke(false, null)
            // Notify UI of failed login
            onLoginResult?.invoke(false, "Login failed")
        }
    }

    /**
     * Cache user statistics from native layer after successful login
     * Stores in RA username-based SharedPreferences (independent of Firebase)
     */
    private fun cacheUserStatsFromNative(username: String) {
        try {
            Log.d(TAG, "Caching user stats from native layer for: $username")

            val userStats = org.uoyabause.android.achievements.RetroAchievementsManager
                .getUserStatsNative()
            val avatarUrl = org.uoyabause.android.achievements.RetroAchievementsManager
                .getUserAvatarUrlNative()

            Log.d(TAG, "Native getUserStatsNative() returned: $userStats")
            Log.d(TAG, "Native getUserAvatarUrlNative() returned: $avatarUrl")

            if (userStats != null && userStats.size >= 3) {
                val hardcoreScore = userStats[0]
                val softcoreScore = userStats[1]
                val unreadMessages = userStats[2]

                Log.d(
                    TAG,
                    "Parsed native stats: hardcore=$hardcoreScore, softcore=$softcoreScore, unread=$unreadMessages, avatar=$avatarUrl",
                )

                // Cache the stats using RA username-based prefs
                val statsPrefs = context.getSharedPreferences(getStatsPreferenceName(username), Context.MODE_PRIVATE)
                val success =
                    statsPrefs
                        .edit()
                        .putString("ra_username", username)
                        .putInt("hardcore_score", hardcoreScore)
                        .putInt("softcore_score", softcoreScore)
                        .putInt("unread_messages", unreadMessages)
                        .putString("avatar_url", avatarUrl)
                        .putLong("last_update", System.currentTimeMillis())
                        .commit()

                Log.d(TAG, "SharedPreferences commit result: $success for RA user: $username")
                Log.d(
                    TAG,
                    "Successfully cached RA stats for $username at login: hardcore=$hardcoreScore, softcore=$softcoreScore, unread=$unreadMessages, avatar=$avatarUrl",
                )
            } else {
                Log.w(TAG, "Failed to get user stats from native layer - userStats is null or insufficient size")
                if (userStats != null) {
                    Log.w(TAG, "userStats array size: ${userStats.size}, content: ${userStats.joinToString()}")
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error caching user stats from native layer", e)
        }
    }

    /**
     * Auto-login using saved credentials (global prefs)
     */
    private fun autoLogin() {
        val prefs = getGlobalPrefs()
        val username = prefs.getString(KEY_USERNAME, null)
        val password = prefs.getString(KEY_API_TOKEN, null)

        if (username != null && password != null) {
            Log.d(TAG, "Attempting auto-login for user: $username")
            loginRetroAchievements(username, password, false)
        }
    }

    /**
     * Check if we have saved credentials
     */
    private fun hasSavedCredentials(): Boolean {
        val prefs = getGlobalPrefs()
        val username = prefs.getString(KEY_USERNAME, null)
        val password = prefs.getString(KEY_API_TOKEN, null)
        return username != null && password != null
    }

    /**
     * Save credentials to global prefs (independent of Firebase)
     * Note: In production, use Android Keystore for password storage
     */
    private fun saveCredentialsGlobal(
        username: String,
        password: String,
    ) {
        val prefs = getGlobalPrefs()
        prefs
            .edit()
            .putString(KEY_USERNAME, username)
            .putString(KEY_API_TOKEN, password)
            .putBoolean(KEY_AUTO_LOGIN, true)
            .apply()
        Log.d(TAG, "Saved RA credentials with auto-login enabled in global prefs, RA user: $username")
    }

    /**
     * Clear saved credentials from global prefs
     */
    private fun clearSavedCredentials() {
        getGlobalPrefs()
            .edit()
            .remove(KEY_USERNAME)
            .remove(KEY_API_TOKEN)
            .apply()
    }

    /**
     * Set up authentication state listeners
     */
    private fun setupAuthStateListeners() {
        // Firebase auth state listener can be added here if needed
        // for coordinated authentication flow
    }

    /**
     * Try to load current game for RetroAchievements if one is running
     * NOTE: This method is now deprecated. Game loading is handled automatically
     * via the onAuthStateChanged callback in Yabause.kt after successful login.
     */
    private fun tryLoadCurrentGame() {
        // This method is no longer used - game loading happens via onAuthStateChanged callback
        Log.d(TAG, "tryLoadCurrentGame called but is deprecated - game loading handled by onAuthStateChanged")
    }

    /**
     * Perform one-time migration from old storage formats to global prefs
     */
    private fun performOldDataMigration() {
        try {
            val migrationKey = "ra_migration_v2_completed"
            val migrationPrefs = context.getSharedPreferences("ra_migration", Context.MODE_PRIVATE)

            if (!migrationPrefs.getBoolean(migrationKey, false)) {
                Log.d(TAG, "Performing RetroAchievements data migration to global prefs...")
                migrateFromOldStorage()
                migrateFromFirebaseUidStorage()

                migrationPrefs.edit().putBoolean(migrationKey, true).apply()
                Log.d(TAG, "RetroAchievements data migration to global prefs completed")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error during RetroAchievements data migration", e)
        }
    }

    /**
     * Migrate from the original shared prefs (retroachievements_auth) to global
     */
    private fun migrateFromOldStorage() {
        try {
            val oldPrefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            val oldData = oldPrefs.all

            if (oldData.isEmpty()) {
                Log.d(TAG, "No old RetroAchievements data to migrate")
                return
            }

            Log.d(TAG, "Migrating ${oldData.size} old RetroAchievements entries to global storage")

            val globalPrefs = getGlobalPrefs()
            // Only migrate if global prefs don't already have credentials
            if (!globalPrefs.contains(KEY_USERNAME)) {
                val editor = globalPrefs.edit()
                oldData.forEach { (key, value) ->
                    when (value) {
                        is String -> editor.putString(key, value)
                        is Boolean -> editor.putBoolean(key, value)
                        is Int -> editor.putInt(key, value)
                        is Long -> editor.putLong(key, value)
                        is Float -> editor.putFloat(key, value)
                    }
                }
                editor.apply()
                Log.d(TAG, "Migrated old RetroAchievements data to global prefs")
            }

            oldPrefs.edit().clear().apply()
            Log.d(TAG, "Cleared old RetroAchievements data")
        } catch (e: Exception) {
            Log.e(TAG, "Error during old storage migration", e)
        }
    }

    /**
     * Migrate from Firebase UID-based prefs (retroachievements_auth_{uid}) to global
     * Scans shared_prefs directory for any Firebase UID-based RA prefs files
     */
    private fun migrateFromFirebaseUidStorage() {
        try {
            val globalPrefs = getGlobalPrefs()
            // Only migrate if global prefs don't already have credentials
            if (globalPrefs.contains(KEY_USERNAME)) {
                Log.d(TAG, "Global prefs already have credentials, skipping Firebase UID migration")
                return
            }

            val prefsDir = File(context.applicationInfo.dataDir, "shared_prefs")
            if (!prefsDir.exists()) return

            val uidPrefsFiles = prefsDir.listFiles { file ->
                file.name.startsWith("retroachievements_auth_") &&
                    file.name != "retroachievements_auth_global.xml" &&
                    file.name != "retroachievements_auth.xml" &&
                    file.name.endsWith(".xml")
            }

            if (uidPrefsFiles.isNullOrEmpty()) {
                Log.d(TAG, "No Firebase UID-based RA prefs files found")
                return
            }

            // Find the most recently modified file (likely the active user's data)
            val newestFile = uidPrefsFiles.maxByOrNull { it.lastModified() }
            if (newestFile != null) {
                val prefsName = newestFile.nameWithoutExtension
                Log.d(TAG, "Migrating from Firebase UID-based prefs: $prefsName")

                val uidPrefs = context.getSharedPreferences(prefsName, Context.MODE_PRIVATE)
                val username = uidPrefs.getString(KEY_USERNAME, null)
                val apiToken = uidPrefs.getString(KEY_API_TOKEN, null)

                if (username != null && apiToken != null) {
                    val editor = globalPrefs.edit()
                    uidPrefs.all.forEach { (key, value) ->
                        when (value) {
                            is String -> editor.putString(key, value)
                            is Boolean -> editor.putBoolean(key, value)
                            is Int -> editor.putInt(key, value)
                            is Long -> editor.putLong(key, value)
                            is Float -> editor.putFloat(key, value)
                        }
                    }
                    editor.apply()
                    Log.d(TAG, "Migrated RA credentials from $prefsName to global prefs (user: $username)")
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error during Firebase UID-based migration", e)
        }
    }

    /**
     * Get RetroAchievements user information
     * @return Map with RA user info or null if not found
     */
    fun getRetroAchievementsUserInfo(): Map<String, Any?>? {
        val prefs = getGlobalPrefs()
        if (prefs.contains("ra_username")) {
            return mapOf(
                "username" to prefs.getString("ra_username", null),
                "hardcoreScore" to prefs.getInt("hardcore_score", 0),
                "softcoreScore" to prefs.getInt("softcore_score", 0),
                "unreadMessages" to prefs.getInt("unread_messages", 0),
                "avatarUrl" to prefs.getString("avatar_url", null),
                "lastUpdate" to prefs.getLong("last_update", 0),
                "autoLogin" to prefs.getBoolean(KEY_AUTO_LOGIN, false),
            )
        }
        return null
    }

    /**
     * Manually trigger auto-login check
     * Now independent of Firebase user state
     */
    fun triggerAutoLoginForCurrentUser() {
        Log.d(TAG, "Manually triggering auto-login check (global)")

        // If already logged in, no need to auto-login
        if (isRetroAchievementsLoggedIn) {
            Log.d(TAG, "Already logged in to RA, skipping auto-login")
            return
        }

        checkAndAutoLogin()
    }

    /**
     * Clean up resources
     */
    fun cleanup() {
        Log.d(TAG, "Cleaning up RetroAchievements auth manager...")
        authScope.cancel()
        retroAchievementsManager.cleanup()
    }

    /**
     * Data class representing comprehensive authentication status
     */
    data class AuthStatus(
        val isFirebaseLoggedIn: Boolean,
        val firebaseUser: FirebaseUser?,
        val isDiscordLinked: Boolean,
        val isRetroAchievementsLoggedIn: Boolean,
        val retroAchievementsUsername: String?,
    ) {
        fun isFullyAuthenticated(): Boolean = isFirebaseLoggedIn && isRetroAchievementsLoggedIn

        fun getDisplayName(): String = when {
            retroAchievementsUsername != null -> retroAchievementsUsername
            firebaseUser?.displayName != null -> firebaseUser.displayName!!
            firebaseUser?.email != null -> firebaseUser.email!!
            else -> "Unknown User"
        }
    }
}
