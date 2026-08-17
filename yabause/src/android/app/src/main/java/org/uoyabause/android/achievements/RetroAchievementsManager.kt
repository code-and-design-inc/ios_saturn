package org.uoyabause.android.achievements

import android.content.Context
import android.util.Log
import com.frybits.harmony.getHarmonySharedPreferences
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.Call
import okhttp3.Callback
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import okhttp3.Response
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.GameInfo
import org.uoyabause.android.YabauseStorage
import org.uoyabause.android.auth.RetroAchievementsAuthManager
import java.io.IOException
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.Semaphore
import java.util.concurrent.atomic.AtomicInteger

/**
 * RetroAchievements integration manager for Android
 * Handles HTTP requests, authentication, and UI callbacks
 */
class RetroAchievementsManager(
    private val context: Context,
) {
    private val keyHardcoreMode = "ra_hardcore_mode"

    companion object {
        private const val TAG = "RetroAchievements"

        @Volatile
        private var instance: RetroAchievementsManager? = null

        fun getInstance(context: Context): RetroAchievementsManager = instance ?: synchronized(this) {
            instance ?: RetroAchievementsManager(context.applicationContext).also { instance = it }
        }

        /**
         * Handle HTTP request from native layer
         */
        @JvmStatic
        fun handleHttpRequest(
            url: String,
            postData: String?,
            callbackPtr: Long,
        ) {
            Log.d(TAG, "HTTP Request: $url (postData: ${postData?.length ?: 0} bytes)")

            // Get the current instance
            val manager = instance
            if (manager == null) {
                Log.e(TAG, "RetroAchievements manager not initialized")
                handleHttpResponseNative(callbackPtr, 0, "")
                return
            }

            manager.coroutineScope.launch {
                try {
                    val request =
                        if (postData != null) {
                            // POST request
                            val mediaType = "application/x-www-form-urlencoded".toMediaType()
                            val body = postData.toRequestBody(mediaType)
                            Request
                                .Builder()
                                .url(url)
                                .post(body)
                                .addHeader("User-Agent", "YabaSanshiro/1.17.7")
                                .build()
                        } else {
                            // GET request
                            Request
                                .Builder()
                                .url(url)
                                .get()
                                .addHeader("User-Agent", "YabaSanshiro/1.17.7")
                                .build()
                        }

                    manager.httpClient.newCall(request).enqueue(
                        object : Callback {
                            override fun onFailure(
                                call: Call,
                                e: IOException,
                            ) {
                                Log.e(TAG, "HTTP request failed: ${e.message}", e)
                                // For timeouts and connection failures, return 0 to indicate no response
                                // rcheevos will handle retries automatically
                                handleHttpResponseNative(callbackPtr, 0, "")
                            }

                            override fun onResponse(
                                call: Call,
                                response: Response,
                            ) {
                                try {
                                    val responseBody = response.body?.string() ?: ""

                                    // For successful responses, pass the actual response
                                    // For server errors (4xx, 5xx), also pass the response so rcheevos can handle it
                                    handleHttpResponseNative(callbackPtr, response.code, responseBody)
                                } catch (e: Exception) {
                                    Log.e(TAG, "Error reading HTTP response", e)
                                    // If we can't read the response, treat as no response
                                    handleHttpResponseNative(callbackPtr, 0, "")
                                }
                            }
                        },
                    )
                } catch (e: Exception) {
                    Log.e(TAG, "HTTP request exception", e)
                    handleHttpResponseNative(callbackPtr, 0, "")
                }
            }
        }

        // Note: JNI calls onAchievementUnlocked, onLeaderboardSubmit, onRichPresenceUpdate methods directly
        // These static callback methods are no longer needed

        // Native method declarations
        @JvmStatic
        private external fun handleHttpResponseNative(
            callbackPtr: Long,
            httpCode: Int,
            response: String,
        )

        /**
         * Native method to get achievement list
         */
        @JvmStatic
        external fun getAchievementListNative(): Array<AchievementListFragment.AchievementItem>

        /**
         * Get user statistics from native layer
         * Returns array: [score, score_softcore, num_unread_messages]
         */
        @JvmStatic
        external fun getUserStatsNative(): IntArray?

        /**
         * Get user avatar URL from native layer
         */
        @JvmStatic
        external fun getUserAvatarUrlNative(): String?

        /**
         * Get user API token (Web API key) from native layer
         * @return API token for HTTP API calls or null if not available
         */
        @JvmStatic
        external fun getUserApiTokenNative(): String?
    }

    private val httpClient =
        OkHttpClient
            .Builder()
            .retryOnConnectionFailure(true)
            .connectTimeout(15, java.util.concurrent.TimeUnit.SECONDS)
            .readTimeout(45, java.util.concurrent.TimeUnit.SECONDS)
            .writeTimeout(45, java.util.concurrent.TimeUnit.SECONDS)
            .build()

    private val pendingCallbacks = ConcurrentHashMap<Long, (Int, String?) -> Unit>()
    private val coroutineScope = CoroutineScope(Dispatchers.IO + SupervisorJob())

    // Current activity reference for UI operations
    private var currentActivity: android.app.Activity? = null
    private var overlaySystem: RetroAchievementsOverlay? = null

    // Cache for current progress indicator data
    private data class ProgressIndicatorData(
        val achievementId: Int,
        val title: String,
        val progress: String,
        val imageUrl: String,
        val progressPercent: Int,
    )

    private var currentProgressIndicator: ProgressIndicatorData? = null

    // Flag to control whether login notifications should be shown
    private var showLoginNotifications = true

    // Automatic hardcore mode management
    private var wasHardcoreEnabledBeforeGame = false
    private var hasAutomaticallyDisabledHardcore = false

    // Flag to trigger a full progress sync on next game list display after login
    private var pendingPostLoginSync = false

    // Game-specific hardcore mode persistence
    private var currentGameCode: String? = null

    /**
     * Set the current game code for per-game settings persistence
     * Should be called when a game is loaded
     */
    fun setCurrentGameCode(gameCode: String?) {
        Log.d(TAG, "Setting current game code: $gameCode")
        currentGameCode = gameCode

        // Load hardcore setting for this game when game code is set
        if (gameCode != null) {
            loadHardcoreSettingForCurrentGame()
        }
    }

    /**
     * Save hardcore mode setting for the current game
     */
    private fun saveHardcoreSettingForCurrentGame(enabled: Boolean) {
        val gameCode = currentGameCode
        if (gameCode != null) {
            try {
                val key = gameCode.replace(" ", "-")
                val gamePreference = context.getHarmonySharedPreferences(key)
                gamePreference
                    .edit()
                    .putBoolean(keyHardcoreMode, enabled)
                    .apply()
                Log.d(TAG, "Saved hardcore setting for game '$gameCode': $enabled")
            } catch (e: Exception) {
                Log.e(TAG, "Error saving hardcore setting for game '$gameCode'", e)
            }
        }
    }

    /**
     * Load hardcore mode setting for the current game
     */
    private fun loadHardcoreSettingForCurrentGame() {
        val gameCode = currentGameCode
        if (gameCode != null) {
            try {
                val key = gameCode.replace(" ", "-")
                val gamePreference = context.getHarmonySharedPreferences(key)
                val savedHardcore = gamePreference.getBoolean(keyHardcoreMode, true)

                Log.d(TAG, "Loaded hardcore setting for game '$gameCode': $savedHardcore")

                // Check login state before applying hardcore setting
                val userLoggedIn = isUserLoggedIn()
                Log.d(TAG, "User login state when applying hardcore setting: $userLoggedIn")

                // Only apply hardcore setting if user is logged in
                if (userLoggedIn) {
                    setHardcoreEnabledNative(savedHardcore)
                    Log.d(TAG, "Applied saved hardcore setting for game '$gameCode': $savedHardcore")
                } else {
                    // Force hardcore mode off when not logged in
                    setHardcoreEnabledNative(false)
                    Log.d(TAG, "User not logged in - forced hardcore mode off for game '$gameCode' (saved setting was: $savedHardcore)")
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error loading hardcore setting for game '$gameCode'", e)
            }
        }
    }

    // Event callbacks
    var onAchievementUnlocked: ((Int, String, String, Int, String?, Boolean) -> Unit)? = null
    var onLeaderboardSubmit: ((Int, String, String, String) -> Unit)? = null
    var onRichPresenceUpdate: ((String) -> Unit)? = null
    var onGameLoaded: ((Boolean, String?) -> Unit)? = null

    /**
     * Set the current activity for UI operations
     * Should be called from the main game activity
     */
    fun setCurrentActivity(activity: android.app.Activity?) {
        Log.d(TAG, "Setting current activity: ${activity?.javaClass?.simpleName}")
        currentActivity = activity

        // Enable login notifications when activity is set (user is actively using the app)
        showLoginNotifications = (activity != null)

        // Initialize overlay system if activity is set
        if (activity != null) {
            initializeOverlaySystem(activity)
        } else {
            cleanupOverlaySystem()
        }
    }

    /**
     * Initialize the overlay system
     */
    private fun initializeOverlaySystem(activity: android.app.Activity) {
        try {
            // Find the overlay container (ext_fragment from main.xml)
            val overlayContainer = activity.findViewById<android.widget.FrameLayout>(R.id.ext_fragment)
            if (overlayContainer != null) {
                overlaySystem = RetroAchievementsOverlay.getInstance(context)
                overlaySystem?.initialize(overlayContainer)
                Log.d(TAG, "Overlay system initialized successfully")
            } else {
                Log.w(TAG, "Could not find overlay container (ext_fragment)")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to initialize overlay system", e)
        }
    }

    /**
     * Clean up the overlay system
     */
    private fun cleanupOverlaySystem() {
        overlaySystem?.cleanup()
        overlaySystem = null
    }

    /**
     * Initialize RetroAchievements system
     */
    fun initialize(): Boolean {
        Log.d(TAG, "Initializing RetroAchievements manager...")

        // Initialize native RetroAchievements integration
        return try {
            // First initialize JNI callbacks
            val callbackInitResult = initializeCallbacksNative(this)
            if (callbackInitResult != 0) {
                Log.e(TAG, "Failed to initialize RetroAchievements JNI callbacks: $callbackInitResult")
                return false
            }

            // Then initialize the core RetroAchievements system
            val initResult = initializeNative()

            // Ensure hardcore mode is disabled if user is not logged in
            if (initResult && !isUserLoggedIn()) {
                Log.d(TAG, "User not logged in - disabling hardcore mode")
                setHardcoreEnabledNative(false)
            }

            initResult
        } catch (e: Exception) {
            Log.e(TAG, "Failed to initialize RetroAchievements", e)
            false
        }
    }

    /**
     * Login user with username and password
     */
    fun loginUser(
        username: String,
        password: String,
        callback: (Boolean, String?) -> Unit,
    ) {
        Log.d(TAG, "Logging in user: $username")

        coroutineScope.launch {
            try {
                val result = loginUserNative(username, password)
                withContext(Dispatchers.Main) {
                    callback(result, if (result) null else "Login failed")
                }
            } catch (e: Exception) {
                Log.e(TAG, "Login error", e)
                withContext(Dispatchers.Main) {
                    callback(false, e.message)
                }
            }
        }
    }

    /**
     * Load game for RetroAchievements
     */
    // fun loadGame(gamePath: String, callback: (Boolean, String?) -> Unit) {
    //    loadGame(gamePath, null, callback)
    // }

/**
     * Load game for RetroAchievements with game code
     */
    fun loadGame(
        gamePath: String,
        callback: (Boolean, String?) -> Unit,
    ) {
        Log.d(TAG, "Loading game: $gamePath (gameCode: $currentGameCode)")

        // Check if user is logged in before attempting game load
        if (!isUserLoggedIn()) {
            Log.e(TAG, "Cannot load game - user not logged in to RetroAchievements")
            callback(false, "User not logged in to RetroAchievements")
            return
        }

        // Restore hardcore mode from previous game first
        restoreHardcoreModeAfterGame()

        coroutineScope.launch {
            try {
                val result = loadGameNative(gamePath)

                if (result) {
                    // Game loaded successfully - manage hardcore mode automatically
                    manageHardcoreModeForCurrentGame()
                }

                withContext(Dispatchers.Main) {
                    callback(result, if (result) null else "Game load failed")
                }
            } catch (e: Exception) {
                Log.e(TAG, "Game load error", e)
                withContext(Dispatchers.Main) {
                    callback(false, e.message)
                }
            }
        }
    }

    /**
     * Check if user is logged in (native method)
     */
    fun isUserLoggedIn(): Boolean = try {
        // Use Android-side auth manager for more reliable login state
        val authManager = RetroAchievementsAuthManager.getInstance(context)
        val isLoggedIn = authManager.isRetroAchievementsLoggedIn()
        Log.d(TAG, "Login status check: Android=$isLoggedIn")
        isLoggedIn
    } catch (e: Exception) {
        Log.e(TAG, "Error checking login status", e)
        false
    }

    /**
     * Process frame for RetroAchievements (call from emulation loop)
     */
    fun doFrame() {
        doFrameNative()
    }

    /**
     * Set hardcore mode
     */
    fun setHardcoreEnabled(enabled: Boolean) {
        Log.d(TAG, "Setting hardcore mode: $enabled (gameCode: $currentGameCode)")

        // Check if user is logged in - hardcore mode should only be enabled when logged in
        if (enabled && !isUserLoggedIn()) {
            Log.w(TAG, "Cannot enable hardcore mode: user not logged in to RetroAchievements")
            // Force hardcore mode off when not logged in
            setHardcoreEnabledNative(false)
            saveHardcoreSettingForCurrentGame(false)
            return
        }

        setHardcoreEnabledNative(enabled)

        // Save the setting for the current game
        saveHardcoreSettingForCurrentGame(enabled)
    }

    /**
     * Get current hardcore mode status
     */
    fun isHardcoreEnabled(): Boolean {
        // If user is not logged in, hardcore mode should always be disabled
        if (!isUserLoggedIn()) {
            return false
        }
        return isHardcoreEnabledNative()
    }

    /**
     * Check if the current game requires RetroAchievements processing
     * (has achievements, leaderboards, or rich presence)
     */
    fun isProcessingRequired(): Boolean = isProcessingRequiredNative()

    /**
     * Automatically manage hardcore mode based on game's RetroAchievements support
     * Disables hardcore for games without achievements and re-enables when needed
     * Only applies if user hasn't explicitly saved a preference for this game
     */
    fun manageHardcoreModeForCurrentGame() {
        val gameCode = currentGameCode
        val hasUserPreference =
            if (gameCode != null) {
                try {
                    val key = gameCode.replace(" ", "-")
                    val gamePreference = context.getHarmonySharedPreferences(key)
                    gamePreference.contains(keyHardcoreMode)
                } catch (e: Exception) {
                    false
                }
            } else {
                false
            }

        if (hasUserPreference) {
            Log.d(TAG, "User has saved hardcore preference for game '$gameCode' - skipping automatic management")
            return
        }

        // Delay the check to allow RetroAchievements to fully load the game data
        android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({
            checkAndManageHardcoreModeDelayed(gameCode)
        }, 2000) // Wait 2 seconds for RA to load game data
    }

    private fun checkAndManageHardcoreModeDelayed(gameCodeAtTime: String?) {
        // Verify we're still on the same game
        if (currentGameCode != gameCodeAtTime) {
            Log.d(TAG, "Game changed during delay, skipping hardcore management")
            return
        }

        val hasUserPreference =
            if (gameCodeAtTime != null) {
                try {
                    val key = gameCodeAtTime.replace(" ", "-")
                    val gamePreference = context.getHarmonySharedPreferences(key)
                    gamePreference.contains(keyHardcoreMode)
                } catch (e: Exception) {
                    false
                }
            } else {
                false
            }

        if (hasUserPreference) {
            Log.d(TAG, "User has saved hardcore preference for game '$gameCodeAtTime' - skipping delayed management")
            return
        }

        val processingRequired = isProcessingRequired()
        Log.d(TAG, "Delayed hardcore check for game '$gameCodeAtTime': processingRequired=$processingRequired")

        if (!processingRequired) {
            // Game has no RetroAchievements functionality - disable hardcore if enabled
            if (isHardcoreEnabled() && !hasAutomaticallyDisabledHardcore) {
                wasHardcoreEnabledBeforeGame = true
                hasAutomaticallyDisabledHardcore = true
                setHardcoreEnabled(false)
                Log.d(TAG, "Automatically disabled hardcore mode for game without RetroAchievements support (delayed)")
            }
        } else {
            Log.d(TAG, "Game has RetroAchievements support - keeping current hardcore mode setting")
        }
    }

    /**
     * Restore hardcore mode when unloading game or before loading next game
     */
    fun restoreHardcoreModeAfterGame() {
        if (hasAutomaticallyDisabledHardcore) {
            if (wasHardcoreEnabledBeforeGame) {
                setHardcoreEnabled(true)
                Log.d(TAG, "Restored hardcore mode after game unload")
            }
            hasAutomaticallyDisabledHardcore = false
            wasHardcoreEnabledBeforeGame = false
        }
    }

    /**
     * Clean up resources
     */
    fun cleanup() {
        Log.d(TAG, "Cleaning up RetroAchievements manager...")

        // Restore hardcore mode before cleanup
        restoreHardcoreModeAfterGame()

        cleanupOverlaySystem()
        coroutineScope.cancel()

        // OkHttp cleanup involves closing SSL sockets which is network I/O.
        // Must run off the main thread to avoid NetworkOnMainThreadException.
        Thread {
            httpClient.dispatcher.executorService.shutdown()
            httpClient.connectionPool.evictAll()
        }.start()
    }

    /**
     * Callback methods called from native code
     */

/**
     * Called when game loading is complete
     * @param success Whether game loaded successfully
     * @param message Optional message (can be null)
     */
    fun onGameLoadComplete(
        success: Boolean,
        message: String?,
    ) {
        Log.d(TAG, "*** onGameLoadComplete CALLED *** - success=$success, message=$message")

        // Check different message types
        if (!message.isNullOrEmpty()) {
            // Check for login failure indicators first
            if (message.lowercase().contains("error") ||
                message.lowercase().contains("fail") ||
                message.lowercase().contains("invalid") ||
                message.lowercase().contains("unauthorized") ||
                message.lowercase().contains("denied")
            ) {
                Log.e(TAG, "*** DETECTED LOGIN FAILURE via onGameLoadComplete *** - message: $message")
                // Route login failure to proper handler
                notifyAuthManagerLoginComplete(false, message)
                return
            }
        }

        // Handle success=false case which might indicate login failure
        if (!success && !message.isNullOrEmpty()) {
            Log.e(TAG, "*** POSSIBLE LOGIN FAILURE via onGameLoadComplete *** - success=false, message: $message")
            // This might be a login failure being routed through onGameLoadComplete
            notifyAuthManagerLoginComplete(false, message)
            return
        }

        // Check for successful game-related messages
        if (success && !message.isNullOrEmpty()) {
            // Parse achievement counts from the message
            Log.d(TAG, "Detected game placard notification via onGameLoadComplete: $message")
            val (unlocked, total) = parseAchievementCounts(message)
            showGamePlacardWithCounts("Game", null, unlocked, total)
            // Notify that game is loaded successfully
            onGameLoaded?.invoke(true, null)
            return
        }

        // This is an actual game load completion (success without message)
        if (success) {
            Log.d(TAG, "Game loaded successfully (no placard message)")
            onGameLoaded?.invoke(true, null)
        }
        // Note: Hardcore mode settings are now applied directly in Yabause.kt after gameCode is confirmed
    }

    /**
     * Parse achievement counts from game placard message
     */
    private fun parseAchievementCounts(message: String?): Pair<Int, Int> {
        if (message.isNullOrEmpty()) {
            return Pair(0, 0)
        }
        try {
            // Look for pattern "You have X of Y achievements unlocked"
            val regex = Regex("You have (\\d+) of (\\d+) achievements unlocked")
            val matchResult = regex.find(message)

            if (matchResult != null) {
                val unlocked = matchResult.groupValues[1].toInt()
                val total = matchResult.groupValues[2].toInt()
                Log.d(TAG, "Parsed achievement counts: $unlocked/$total")
                return Pair(unlocked, total)
            }

            // Check for "This game has no achievements"
            if (message.contains("no achievements")) {
                Log.d(TAG, "Game has no achievements")
                return Pair(0, 0)
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error parsing achievement counts from message: $message", e)
        }

        // Default fallback
        return Pair(0, 0)
    }

    /**
     * Called when login process is complete
     * @param success Whether login was successful
     * @param message Optional message (can be null)
     */
    fun onLoginComplete(
        success: Boolean,
        message: String?,
    ) {
        Log.d(TAG, "*** onLoginComplete CALLED *** - success=$success, message=$message")

        // Notify auth manager of actual login completion
        if (success) {
            // Login actually completed in native layer
            Log.d(TAG, "Notifying auth manager of successful login completion")
            notifyAuthManagerLoginComplete(true, message)
        } else {
            // Login failed - do not show success notification
            Log.e(TAG, "Login failed: ${message ?: "Unknown error"}")
            notifyAuthManagerLoginComplete(false, message)
        }
    }

    /**
     * Notify the auth manager of login completion
     * This will be called from native layer when login is actually complete
     */
    private fun notifyAuthManagerLoginComplete(
        success: Boolean,
        username: String?,
    ) {
        // Get auth manager instance and notify
        val authManager = org.uoyabause.android.auth.RetroAchievementsAuthManager
            .getInstance(context)
        authManager.onNativeLoginComplete(success, username)
    }

    /**
     * Called when the native layer needs to make an HTTP request
     * @param url Request URL
     * @param postData POST data (null for GET)
     * @param callbackPtr Native callback pointer
     */
    fun onHttpRequest(
        url: String,
        postData: String?,
        callbackPtr: Long,
    ) {
        // Log.d(TAG, "HTTP request: $url")
        handleHttpRequest(url, postData, callbackPtr)
    }

    /**
     * JNI callback methods called from native layer
     */
    fun onAchievementUnlocked(
        achievementId: Int,
        title: String,
        description: String,
        points: Int,
        badge: String?,
        isOfficial: Boolean,
    ) {
        Log.d(TAG, "Achievement unlocked: $title ($points points) ${if (!isOfficial) "[UNOFFICIAL]" else ""}")

        // Only trigger callback - let the callback handler show the notification
        // This prevents duplicate notifications
        onAchievementUnlocked?.invoke(achievementId, title, description, points, badge, isOfficial)

        // Locally increment DB so the game list reflects the unlock immediately
        currentGameCode?.let { code ->
            coroutineScope.launch(Dispatchers.IO) {
                val gameInfo = YabauseStorage.dao.findByProductNumber(code)
                gameInfo?.let {
                    it.raUnlocked += 1
                    if (isHardcoreEnabled()) it.raUnlockedHardcore += 1
                    YabauseStorage.dao.update(it)
                    Log.d(TAG, "Locally incremented RA unlock count for $code: ${it.raUnlocked}/${it.raTotal}")
                }
            }
        }
    }

    fun onLeaderboardSubmit(
        leaderboardId: Int,
        title: String,
        description: String,
        scoreString: String,
    ) {
        Log.d(TAG, "Leaderboard score submitted: $scoreString to '$title' (ID: $leaderboardId)")

        // Only trigger callback - let the callback handler show the notification
        // This prevents duplicate notifications
        onLeaderboardSubmit?.invoke(leaderboardId, title, description, scoreString)
    }

    fun onRichPresenceUpdate(richPresence: String) {
        Log.d(TAG, "Rich presence: $richPresence")

        // Trigger callback if set
        onRichPresenceUpdate?.invoke(richPresence)
    }

    /**
     * Called when game placard should be displayed
     * @param gameTitle The game title
     * @param imageUrl Game badge image URL (optional)
     * @param unlockedAchievements Number of unlocked achievements
     * @param totalAchievements Total number of achievements
     */
    fun onGamePlacard(
        gameTitle: String,
        imageUrl: String?,
        unlockedAchievements: Int,
        totalAchievements: Int,
    ) {
        Log.d(TAG, "Game placard: $gameTitle - $unlockedAchievements/$totalAchievements achievements")
        showGamePlacardWithCounts(gameTitle, imageUrl, unlockedAchievements, totalAchievements)
    }

    /**
     * Called when a game is mastered (all achievements unlocked)
     * @param gameTitle The name of the mastered game
     * @param imageUrl Game badge image URL (optional)
     * @param achievementCount Total number of achievements in the game
     * @param points Total points earned
     * @param isHardcore Whether mastery was achieved in hardcore mode
     * @param username The user's username for personalized message
     * @param playtime Optional playtime string
     */
    fun onGameMastery(
        gameTitle: String,
        imageUrl: String?,
        achievementCount: Int,
        points: Int,
        isHardcore: Boolean,
        username: String?,
        playtime: String?,
    ) {
        Log.d(TAG, "Game mastered: $gameTitle - $achievementCount achievements, $points points, hardcore: $isHardcore")
        val displayContext = currentActivity ?: context
        val notification = RetroAchievementsNotification(displayContext)
        notification.showGameMastery(gameTitle, imageUrl, achievementCount, points, isHardcore, username, playtime)
    }

    /**
     * Called when a server error occurs that won't be retried
     * These should be shown to the user as per rcheevos guidelines
     * @param api The API that failed (e.g., "unlock_achievement", "submit_lboard_entry")
     * @param errorMessage The error message from the server
     */
    fun onServerError(
        api: String,
        errorMessage: String,
    ) {
        Log.e(TAG, "Server error - $api: $errorMessage")

        // Create a user-friendly error message
        val userMessage =
            when {
                api.contains("unlock") -> "Failed to unlock achievement: $errorMessage"
                api.contains("lboard") || api.contains("leaderboard") -> "Failed to submit leaderboard score: $errorMessage"
                api.contains("login") -> "Login failed: $errorMessage"
                api.contains("game") -> "Failed to load game data: $errorMessage"
                else -> "RetroAchievements error: $errorMessage"
            }

        // Show error notification to user
        val displayContext = currentActivity ?: context
        val notification = RetroAchievementsNotification(displayContext)
        notification.showServerError(userMessage)
    }

    /**
     * Called when a leaderboard tracker should be shown on screen
     * @param trackerId Unique ID for this tracker
     * @param display Text to display for the tracker (e.g., "0:07.53")
     */
    fun onLeaderboardTrackerShow(
        trackerId: Int,
        display: String,
    ) {
        Log.d(TAG, "Leaderboard tracker show: ID=$trackerId, display='$display'")
        overlaySystem?.showLeaderboardTracker(trackerId, display)
    }

    /**
     * Called when a leaderboard tracker should be hidden
     * @param trackerId Unique ID for the tracker to hide
     */
    fun onLeaderboardTrackerHide(trackerId: Int) {
        Log.d(TAG, "Leaderboard tracker hide: ID=$trackerId")
        overlaySystem?.hideLeaderboardTracker(trackerId)
    }

    /**
     * Called when a leaderboard tracker display should be updated
     * @param trackerId Unique ID for the tracker to update
     * @param display Updated text to display (e.g., "0:08.95")
     */
    fun onLeaderboardTrackerUpdate(
        trackerId: Int,
        display: String,
    ) {
        Log.d(TAG, "Leaderboard tracker update: ID=$trackerId, display='$display'")
        overlaySystem?.updateLeaderboardTracker(trackerId, display)
    }

    /**
     * Called when a challenge indicator should be shown on screen
     * @param achievementId Achievement ID for this challenge
     * @param title Achievement title
     * @param imageUrl URL to the achievement badge image
     */
    fun onChallengeIndicatorShow(
        achievementId: Int,
        title: String,
        imageUrl: String,
    ) {
        Log.d(TAG, "Challenge indicator show: achievement=$achievementId, title='$title', imageUrl='$imageUrl'")
        overlaySystem?.showChallengeIndicator(achievementId, title, imageUrl)
    }

    /**
     * Called when a challenge indicator should be hidden
     * @param achievementId Achievement ID for the indicator to hide
     */
    fun onChallengeIndicatorHide(achievementId: Int) {
        Log.d(TAG, "Challenge indicator hide: achievement=$achievementId")
        overlaySystem?.hideChallengeIndicator(achievementId)
    }

    /**
     * Called when the single progress indicator should be shown on screen
     * The UPDATE event has already been called to set the data, so just make it visible
     */
    fun onProgressIndicatorShow() {
        Log.d(TAG, "Progress indicator show: making current progress indicator visible")
        currentProgressIndicator?.let { data ->
            overlaySystem?.showProgressIndicator(
                data.achievementId,
                data.title,
                data.progress,
                data.imageUrl,
                data.progressPercent,
            )
        }
    }

    /**
     * Called when the progress indicator should be hidden from screen
     */
    fun onProgressIndicatorHide() {
        Log.d(TAG, "Progress indicator hide: hiding current progress indicator")
        currentProgressIndicator?.let { data ->
            overlaySystem?.hideProgressIndicator(data.achievementId)
        }
    }

    /**
     * Called when the progress indicator content should be updated
     * @param achievementId Achievement ID for this progress indicator
     * @param title Achievement title
     * @param progress Progress text (e.g., "3/5 enemies defeated")
     * @param imageUrl URL to the locked achievement badge image
     * @param progressPercent Progress as percentage (0-100)
     */
    fun onProgressIndicatorUpdate(
        achievementId: Int,
        title: String,
        progress: String,
        imageUrl: String,
        progressPercent: Int,
    ) {
        Log.d(
            TAG,
            "Progress indicator update: achievement=$achievementId, title='$title', progress='$progress', percent=$progressPercent%, imageUrl='$imageUrl'",
        )
        currentProgressIndicator = ProgressIndicatorData(achievementId, title, progress, imageUrl, progressPercent)
        overlaySystem?.updateProgressIndicator(achievementId, title, progress, imageUrl, progressPercent)
    }

    /**
     * Show login success popup following rcheevos guidelines
     * @param displayName User's display name or username
     * @param points User's total points
     */
    private fun showLoginSuccessPopup(
        displayName: String,
        points: Int,
    ) {
        Log.d(TAG, "Showing login success popup for: $displayName ($points points)")

        // Use current activity if available, otherwise fall back to context
        val displayContext = currentActivity ?: context

        // Show notification using RetroAchievementsNotification
        val notification = RetroAchievementsNotification(displayContext)
        notification.showLoginSuccess(displayName, displayName, points)
    }

    /**
     * Show game placard with achievement counts
     * @param gameTitle Game title
     * @param imageUrl Game badge image URL (optional)
     * @param unlockedAchievements Number of unlocked achievements
     * @param totalAchievements Total number of achievements
     */
    private fun showGamePlacardWithCounts(
        gameTitle: String,
        imageUrl: String?,
        unlockedAchievements: Int,
        totalAchievements: Int,
    ) {
        Log.d(TAG, "Showing game placard for: $gameTitle ($unlockedAchievements/$totalAchievements)")

        // Use current activity if available, otherwise fall back to context
        val displayContext = currentActivity ?: context

        // Show notification using RetroAchievementsNotification
        val notification = RetroAchievementsNotification(displayContext)
        notification.showGamePlacard(gameTitle, imageUrl, unlockedAchievements, totalAchievements)
    }

    /**
     * Handle media change for multi-disc games (e.g., disc swapping)
     * Important for Saturn games that span multiple discs
     * @param newMediaPath Path to the new disc/media file
     * @return true if media change was initiated successfully
     */
    fun changeMedia(newMediaPath: String): Boolean {
        Log.d(TAG, "Changing media to: $newMediaPath")
        return try {
            changeMediaNative(newMediaPath)
        } catch (e: Exception) {
            Log.e(TAG, "Error changing media", e)
            false
        }
    }

    /**
     * Generate game hash for RetroAchievements Game ID resolution
     * @param gamePath Path to the game file (ISO, CHD, CUE, etc.) or content:// URI
     * @return MD5 hash (32 characters) or null if failed
     */
    suspend fun generateGameHash(gamePath: String?): String? {
        if (gamePath.isNullOrEmpty()) {
            Log.w(TAG, "Cannot generate hash: path is null or empty")
            return null
        }

        return withContext(Dispatchers.IO) {
            var pfd: android.os.ParcelFileDescriptor? = null
            try {
                val actualPath = if (gamePath.startsWith("content://")) {
                    // Handle content:// URI
                    val uri = android.net.Uri.parse(gamePath)
                    pfd = context.contentResolver.openFileDescriptor(uri, "r")
                    if (pfd == null) {
                        Log.w(TAG, "Cannot generate hash: failed to open content URI - $gamePath")
                        return@withContext null
                    }

                    // Get original filename from URI to preserve extension
                    val cursor = context.contentResolver.query(uri, null, null, null, null)
                    val originalFilename = cursor?.use {
                        if (it.moveToFirst()) {
                            val nameIndex = it.getColumnIndex(android.provider.OpenableColumns.DISPLAY_NAME)
                            if (nameIndex >= 0) it.getString(nameIndex) else null
                        } else {
                            null
                        }
                    }

                    // Pass /proc/self/fd/XX path with extension appended
                    // pfd must stay open until native hash generation completes
                    // The native chd_open_track strips the extension for fd paths
                    val fdPath = "/proc/self/fd/${pfd.fd}"
                    val extension = originalFilename?.substringAfterLast('.', "") ?: ""
                    val pathWithExt = if (extension.isNotEmpty()) "$fdPath.$extension" else fdPath
                    Log.d(TAG, "Using fd path with extension: $pathWithExt (fd=${pfd.fd})")
                    pathWithExt
                } else {
                    // Regular file path
                    val file = java.io.File(gamePath)
                    if (!file.exists()) {
                        Log.w(TAG, "Cannot generate hash: file does not exist - $gamePath")
                        return@withContext null
                    }
                    if (!file.canRead()) {
                        Log.w(TAG, "Cannot generate hash: file is not readable - $gamePath")
                        return@withContext null
                    }
                    gamePath
                }

                Log.d(TAG, "Generating game hash for: $actualPath")
                val hash = generateGameHashNative(actualPath)
                if (hash != null && hash.length == 32) {
                    Log.d(TAG, "Generated hash: $hash")
                    hash
                } else {
                    Log.w(TAG, "Hash generation failed or invalid format")
                    null
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error generating game hash", e)
                null
            } finally {
                // Close the file descriptor after native code is done
                pfd?.let {
                    try {
                        it.close()
                        Log.d(TAG, "Closed ParcelFileDescriptor")
                    } catch (e: Exception) {
                        Log.w(TAG, "Failed to close ParcelFileDescriptor", e)
                    }
                }
            }
        }
    }

    /**
     * Resolve RetroAchievements Game ID and achievement data from game hash
     * @param hash MD5 hash of the game (32 characters)
     * @return AchievementProgress object with game data or null if not found/error
     */
    suspend fun resolveGameId(hash: String): AchievementProgress? =
        withContext(Dispatchers.IO) {
            try {
                // Validate hash format
                if (hash.isEmpty() || hash.length != 32 || !hash.matches(Regex("^[a-fA-F0-9]{32}$"))) {
                    Log.w(TAG, "Invalid hash format: $hash")
                    return@withContext null
                }

                Log.d(TAG, "Resolving Game ID for hash: $hash")

                // Get authentication credentials
                val authManager = RetroAchievementsAuthManager.getInstance(context)
                val username = authManager.getCurrentRAUsername()
                val sessionToken = getUserApiTokenNative() // This is the 16-char session token

                // Debug: Log credential retrieval (partially obscured for security)
                Log.d(TAG, "API credentials - username: ${username?.take(3)}***, sessionToken: ${if (sessionToken.isNullOrEmpty()) "NULL/EMPTY" else "present (${sessionToken.length} chars)"}")

                if (username.isNullOrEmpty() || sessionToken.isNullOrEmpty()) {
                    Log.w(TAG, "Missing RA credentials for API request")
                    return@withContext null
                }

                // Build API request using dorequest.php endpoint (POST request)
                // This is the same endpoint that rcheevos uses internally
                // Parameters: r=achievementsets, u=username, t=session_token, m=hash
                val url = "https://retroachievements.org/dorequest.php"
                val postData = "r=achievementsets&u=$username&t=$sessionToken&m=$hash"
                Log.d(TAG, "Making POST API request to: $url with data: ${postData.replace(Regex("t=[^&]+"), "t=***")}")

                val requestBody = postData.toRequestBody("application/x-www-form-urlencoded".toMediaType())

                val request =
                    Request
                        .Builder()
                        .url(url)
                        .post(requestBody)
                        .addHeader("User-Agent", "YabaSanshiro/1.17.7")
                        .build()

                // Execute request with timeout handling
                val response = httpClient.newCall(request).execute()

                if (!response.isSuccessful) {
                    Log.w(TAG, "API request failed with code: ${response.code}")
                    return@withContext null
                }

                // Parse JSON response
                val responseBody = response.body?.string() ?: ""
                if (responseBody.isEmpty()) {
                    Log.w(TAG, "Empty response body")
                    return@withContext null
                }

                Log.d(TAG, "API Response (first 1000 chars): ${responseBody.take(1000)}")
                // Check if UserProgress is in the response
                if (responseBody.contains("UserProgress")) {
                    Log.d(TAG, "UserProgress found in response")
                } else {
                    Log.w(TAG, "UserProgress NOT found in response - user progress data unavailable")
                }

                // Parse response from dorequest.php?r=achievementsets
                // Response format: {"Success":true,"GameId":24213,"Title":"After Burner II","Sets":[{"Achievements":[...]}], ...}
                val json = org.json.JSONObject(responseBody)

                // Check if request was successful
                val success = json.optBoolean("Success", false)
                if (!success) {
                    val error = json.optString("Error", "Unknown error")
                    Log.w(TAG, "API returned error: $error")
                    return@withContext null
                }

                // Extract game ID (note: field name is "GameId" not "GameID")
                val gameId = json.optInt("GameId", 0)

                if (gameId <= 0) {
                    Log.w(TAG, "Game not found in RA database for hash: $hash")
                    return@withContext null
                }

                // Extract achievement count from Sets array
                val sets = json.optJSONArray("Sets")
                var totalAchievements = 0
                if (sets != null) {
                    for (i in 0 until sets.length()) {
                        val set = sets.optJSONObject(i)
                        val achievements = set?.optJSONArray("Achievements")
                        if (achievements != null) {
                            totalAchievements += achievements.length()
                        }
                    }
                }

                // UserProgress is not included in dorequest.php?r=achievementsets response
                // We'll fetch it separately if needed, for now default to 0
                val unlockedAchievements = 0
                val unlockedHardcore = 0
                val completionPercent = "0.0"
                val completionPercentHardcore = "0.0"

                // Extract image icon URL
                val imageIconUrl = json.optString("ImageIconUrl", "")

                Log.d(TAG, "Resolved Game ID: $gameId with $totalAchievements achievements ($unlockedAchievements unlocked) for hash: $hash")

                // Return AchievementProgress with game data including user progress
                AchievementProgress(
                    gameId = gameId,
                    totalAchievements = totalAchievements,
                    unlockedAchievements = unlockedAchievements,
                    unlockedHardcore = unlockedHardcore,
                    completionPercent = completionPercent,
                    completionPercentHardcore = completionPercentHardcore,
                    imageIcon = imageIconUrl,
                )
            } catch (e: java.net.SocketTimeoutException) {
                Log.e(TAG, "Network timeout while resolving Game ID", e)
                null
            } catch (e: org.json.JSONException) {
                Log.e(TAG, "JSON parse error while resolving Game ID", e)
                null
            } catch (e: Exception) {
                Log.e(TAG, "Error resolving Game ID", e)
                null
            }
        }

    /**
     * Fetch all user progress for Sega Saturn (console ID 39)
     * Returns a map of gameId -> (unlocked, unlockedHardcore)
     */
    suspend fun fetchAllUserProgress(): Map<Int, Pair<Int, Int>>? =
        withContext(Dispatchers.IO) {
            try {
                // Get authentication credentials
                val authManager = RetroAchievementsAuthManager.getInstance(context)
                val username = authManager.getCurrentRAUsername()
                val sessionToken = getUserApiTokenNative()

                if (username.isNullOrEmpty() || sessionToken.isNullOrEmpty()) {
                    Log.w(TAG, "Missing RA credentials for all progress request")
                    return@withContext null
                }

                // Build API request using dorequest.php with r=allprogress
                // c=39 is Sega Saturn console ID
                val url = "https://retroachievements.org/dorequest.php"
                val postData = "r=allprogress&u=$username&t=$sessionToken&c=39"
                Log.d(TAG, "Fetching all user progress for Sega Saturn")

                val requestBody = postData.toRequestBody("application/x-www-form-urlencoded".toMediaType())
                val request = Request
                    .Builder()
                    .url(url)
                    .post(requestBody)
                    .addHeader("User-Agent", "YabaSanshiro/1.17.7")
                    .build()

                val response = httpClient.newCall(request).execute()
                if (!response.isSuccessful) {
                    Log.w(TAG, "All progress request failed with code: ${response.code}")
                    return@withContext null
                }

                val responseBody = response.body?.string() ?: ""
                if (responseBody.isEmpty()) {
                    Log.w(TAG, "Empty all progress response")
                    return@withContext null
                }

                Log.d(TAG, "All progress response received (${responseBody.length} bytes)")

                // Parse response: {"Success":true,"Response":{"gameId":{"Achievements":X,"Unlocked":Y,"UnlockedHardcore":Z},...}}
                val json = org.json.JSONObject(responseBody)
                val success = json.optBoolean("Success", false)
                if (!success) {
                    Log.w(TAG, "All progress request unsuccessful")
                    return@withContext null
                }

                val responseData = json.optJSONObject("Response")
                if (responseData == null) {
                    Log.w(TAG, "No Response object in all progress data")
                    return@withContext null
                }

                // Parse each game's progress
                val progressMap = mutableMapOf<Int, Pair<Int, Int>>()
                val keys = responseData.keys()
                while (keys.hasNext()) {
                    val gameIdStr = keys.next()
                    val gameId = gameIdStr.toIntOrNull() ?: continue
                    val gameData = responseData.optJSONObject(gameIdStr) ?: continue

                    val unlocked = gameData.optInt("Unlocked", 0)
                    val unlockedHardcore = gameData.optInt("UnlockedHardcore", 0)

                    progressMap[gameId] = Pair(unlocked, unlockedHardcore)
                }

                Log.d(TAG, "Fetched progress for ${progressMap.size} games")
                progressMap
            } catch (e: Exception) {
                Log.e(TAG, "Error fetching all user progress", e)
                null
            }
        }

    /**
     * Get achievement progress for a specific game and user
     * @param gameId RetroAchievements Game ID
     * @param username RetroAchievements username
     * @return AchievementProgress object or null if error/not found
     */
    suspend fun getGameProgress(
        gameId: Int,
        username: String,
    ): AchievementProgress? =
        withContext(Dispatchers.IO) {
            try {
                // Validate inputs
                if (gameId <= 0) {
                    Log.w(TAG, "Invalid game ID: $gameId")
                    return@withContext null
                }

                if (username.isEmpty()) {
                    Log.w(TAG, "Empty username")
                    return@withContext null
                }

                Log.d(TAG, "Getting progress for game $gameId, user $username")

                // Get session token for authentication
                val sessionToken = getUserApiTokenNative() // 16-char session token

                // Debug: Log credential retrieval (partially obscured for security)
                Log.d(TAG, "Progress API credentials - username: ${username.take(3)}***, sessionToken: ${if (sessionToken.isNullOrEmpty()) "NULL/EMPTY" else "present (${sessionToken.length} chars)"}")

                if (sessionToken.isNullOrEmpty()) {
                    Log.w(TAG, "Missing RA session token for progress request")
                    return@withContext null
                }

                // Build API request using dorequest.php endpoint (POST request)
                // Note: The achievementsets response already includes achievement data
                // This function fetches the data that was returned during game identification
                val url = "https://retroachievements.org/dorequest.php"
                val postData = "r=achievementsets&u=$username&t=$sessionToken&m=$gameId"
                Log.d(TAG, "Making POST progress API request to: $url with data: ${postData.replace(Regex("t=[^&]+"), "t=***")}")

                val requestBody = postData.toRequestBody("application/x-www-form-urlencoded".toMediaType())

                val request =
                    Request
                        .Builder()
                        .url(url)
                        .post(requestBody)
                        .addHeader("User-Agent", "YabaSanshiro/1.17.7")
                        .build()

                // Execute request
                val response = httpClient.newCall(request).execute()

                if (!response.isSuccessful) {
                    Log.w(TAG, "API request failed with code: ${response.code}")
                    return@withContext null
                }

                // Parse JSON response
                val responseBody = response.body?.string() ?: ""
                if (responseBody.isEmpty()) {
                    Log.w(TAG, "Empty response body")
                    return@withContext null
                }

                Log.d(TAG, "API Response (first 200 chars): ${responseBody.take(200)}")

                val json = org.json.JSONObject(responseBody)

                // Parse response fields with default values for missing fields
                val totalAchievements = json.optInt("NumAchievements", 0)
                val userProgress = json.optJSONObject("UserProgress")

                val unlockedAchievements = userProgress?.optInt("NumAchieved", 0) ?: 0
                val unlockedHardcore = userProgress?.optInt("NumAchievedHardcore", 0) ?: 0
                val completionPercent = userProgress?.optString("PctWon", "0.0") ?: "0.0"
                val completionPercentHardcore = userProgress?.optString("PctWonHardcore", "0.0") ?: "0.0"

                // Get game icon
                val imageIcon = json.optString("ImageIcon", "")

                val progress =
                    AchievementProgress(
                        gameId = gameId,
                        totalAchievements = totalAchievements,
                        unlockedAchievements = unlockedAchievements,
                        unlockedHardcore = unlockedHardcore,
                        completionPercent = completionPercent,
                        completionPercentHardcore = completionPercentHardcore,
                        imageIcon = if (imageIcon.isNotEmpty()) {
                            "https://retroachievements.org$imageIcon"
                        } else {
                            ""
                        },
                    )

                Log.d(
                    TAG,
                    "Progress: ${progress.unlockedAchievements}/${progress.totalAchievements} (${progress.completionPercent}%)",
                )

                progress
            } catch (e: java.net.SocketTimeoutException) {
                Log.e(TAG, "Network timeout while getting game progress", e)
                null
            } catch (e: org.json.JSONException) {
                Log.e(TAG, "JSON parse error while getting game progress", e)
                null
            } catch (e: Exception) {
                Log.e(TAG, "Error getting game progress", e)
                null
            }
        }

    // Native method declarations
    private external fun initializeCallbacksNative(callback: RetroAchievementsManager): Int

    private external fun initializeNative(): Boolean

    private external fun loginUserNative(
        username: String,
        password: String,
    ): Boolean

    private external fun loadGameNative(gamePath: String): Boolean

    private external fun doFrameNative()

    private external fun setHardcoreEnabledNative(enabled: Boolean)

    private external fun isHardcoreEnabledNative(): Boolean

    private external fun isProcessingRequiredNative(): Boolean

    private external fun isUserLoggedInNative(): Boolean

    private external fun changeMediaNative(newMediaPath: String): Boolean

    private external fun generateGameHashNative(gamePath: String): String?

    /**
     * Called when login state changes (login/logout)
     * @param isLoggedIn true if user is now logged in, false if logged out
     */
    fun onLoginStateChanged(isLoggedIn: Boolean) {
        Log.d(TAG, "Login state changed: isLoggedIn=$isLoggedIn")

        if (isLoggedIn) {
            // User logged in - schedule a full progress sync on next game list display
            pendingPostLoginSync = true
            Log.d(TAG, "Scheduled post-login progress sync")
        } else {
            // User logged out - force disable hardcore mode
            Log.d(TAG, "User logged out - forcing hardcore mode off")
            try {
                setHardcoreEnabledNative(false)
                // Also update any saved preferences to reflect this
                saveHardcoreSettingForCurrentGame(false)
            } catch (e: Exception) {
                Log.e(TAG, "Failed to disable hardcore mode on logout", e)
            }
        }
    }

    /**
     * Check if cached RA data is fresh (within TTL)
     * @param gameInfo Game to check
     * @return true if cache is fresh, false if stale or missing
     */
    fun isCacheFresh(gameInfo: GameInfo): Boolean {
        // Permanently skip games not supported by RA
        if (gameInfo.raNotSupported) return true
        // Fresh if game ID has already been resolved
        return gameInfo.raGameId != null
    }

    /**
     * Check if game has cached RA data (regardless of freshness)
     * @param gameInfo Game to check
     * @return true if has cache, false otherwise
     */
    fun hasCache(gameInfo: GameInfo): Boolean = gameInfo.raLastUpdate != null || gameInfo.raTotal > 0 || gameInfo.raNotSupported

    /**
     * Update RetroAchievements progress for a single game
     * @param gameInfo Game to update
     * @return true if updated successfully, false otherwise
     */
    suspend fun updateSingleGameProgress(gameInfo: GameInfo): Boolean {
        try {
            // Skip if RA not supported for this game
            if (gameInfo.raNotSupported) {
                Log.d(TAG, "Skipping update for ${gameInfo.game_title}: raNotSupported=true")
                return false
            }

            // Get or generate hash
            var hash = gameInfo.raHash
            if (hash.isNullOrEmpty()) {
                hash = generateGameHash(gameInfo.file_path)
                if (hash == null) {
                    Log.w(TAG, "Failed to generate hash for ${gameInfo.game_title}")
                    return false
                }
                gameInfo.raHash = hash
            }

            // Resolve Game ID and get achievement data
            val gameData = resolveGameId(hash)
            if (gameData == null) {
                Log.w(TAG, "Game not found in RA database: ${gameInfo.game_title}")
                gameInfo.raNotSupported = true
                gameInfo.raLastUpdate = java.util.Date()
                // Update database
                YabauseStorage.dao.update(gameInfo)
                return false
            }

            // Store game ID and mark as supported
            gameInfo.raGameId = gameData.gameId
            gameInfo.raNotSupported = false // Clear not-supported flag on success
            gameInfo.raTotal = gameData.totalAchievements

            // User progress will be fetched via batch update with allprogress API
            // For now, default to 0 (will be updated by batch process)
            gameInfo.raUnlocked = 0
            gameInfo.raLastUpdate = java.util.Date()

            // Save to database
            YabauseStorage.dao.update(gameInfo)

            Log.d(
                TAG,
                "Updated ${gameInfo.game_title}: gameId=${gameInfo.raGameId}, ${gameInfo.raUnlocked}/${gameInfo.raTotal}",
            )
            return true
        } catch (e: Exception) {
            Log.e(TAG, "Error updating single game progress for ${gameInfo.game_title}", e)
            return false
        }
    }

    /**
     * Update RA progress only when actually needed:
     * - On first call after login: fetchAllUserProgress() once to sync all games
     * - Always: resolve Game ID for any new games (raGameId == null)
     *
     * @param allGames Full game list
     * @param onProgressUpdate Callback for progress updates (current, total)
     */
    suspend fun updateProgressIfNeeded(
        allGames: List<GameInfo>,
        onProgressUpdate: (current: Int, total: Int) -> Unit = { _, _ -> },
    ) {
        // Phase 1: Post-login sync — fetch all progress once
        if (pendingPostLoginSync) {
            pendingPostLoginSync = false
            Log.d(TAG, "Post-login sync: fetching all user progress")
            val allProgress = fetchAllUserProgress()
            if (allProgress != null) {
                allGames
                    .filter { it.raGameId != null && !it.raNotSupported }
                    .forEach { game ->
                        allProgress[game.raGameId]?.let { (unlocked, hardcore) ->
                            game.raUnlocked = unlocked
                            game.raUnlockedHardcore = hardcore
                            game.raLastUpdate = java.util.Date()
                            YabauseStorage.dao.update(game)
                        }
                    }
                Log.d(TAG, "Post-login sync complete: applied progress to ${allProgress.size} games")
            }
        }

        // Phase 2: Identify new games that have no RA Game ID yet
        val newGames = allGames.filter { !it.raNotSupported && it.raGameId == null }
        if (newGames.isNotEmpty()) {
            Log.d(TAG, "Resolving Game ID for ${newGames.size} new game(s)")
            updateAchievementProgressBatch(newGames, onProgressUpdate)
        }
    }

    /**
     * Update RetroAchievements progress for multiple games in batch
     * Uses Semaphore to limit concurrent API calls to 5
     *
     * @param games List of games to update
     * @param onProgressUpdate Callback for progress updates (current, total)
     */
    suspend fun updateAchievementProgressBatch(
        games: List<GameInfo>,
        onProgressUpdate: (current: Int, total: Int) -> Unit,
    ) {
        val total = games.size
        val completed = AtomicInteger(0)
        val semaphore = Semaphore(3) // Limit to 3 concurrent API calls (reduced from 5)

        Log.d(TAG, "Starting batch update for $total games")

        // Fetch all user progress once at the beginning
        val allProgress = fetchAllUserProgress()
        if (allProgress != null) {
            Log.d(TAG, "Fetched progress for ${allProgress.size} games, will apply to game list")
        } else {
            Log.w(TAG, "Failed to fetch all progress, will proceed without unlock counts")
        }

        coroutineScope
            .launch(Dispatchers.IO) {
                games
                    .mapIndexed { index, game ->
                        launch {
                            try {
                                // Add staggered delay to avoid rate limiting (50ms * index)
                                // This ensures requests are spaced out over time
                                kotlinx.coroutines.delay(50L * index)

                                // Acquire semaphore (wait if 3 tasks already running)
                                semaphore.acquire()

                                // Update single game
                                updateSingleGameProgress(game)

                                // Apply progress data if available
                                if (allProgress != null && game.raGameId != null) {
                                    val progressData = allProgress[game.raGameId]
                                    if (progressData != null) {
                                        game.raUnlocked = progressData.first // Use regular unlocked count
                                        game.raUnlockedHardcore = progressData.second // Hardcore unlocked count
                                        YabauseStorage.dao.update(game)
                                        Log.d(TAG, "Applied progress to ${game.game_title}: ${progressData.first} unlocked (HC: ${progressData.second})")
                                    }
                                }

                                // Report progress
                                val current = completed.incrementAndGet()
                                withContext(Dispatchers.Main) {
                                    onProgressUpdate(current, total)
                                }
                            } catch (e: Exception) {
                                Log.e(TAG, "Error in batch update for ${game.game_title}", e)
                                // Continue processing other games
                                val current = completed.incrementAndGet()
                                withContext(Dispatchers.Main) {
                                    onProgressUpdate(current, total)
                                }
                            } finally {
                                // Release semaphore
                                semaphore.release()
                            }
                        }
                    }.forEach { it.join() } // Wait for all jobs to complete
            }.join()

        Log.d(TAG, "Batch update complete: $completed/$total games processed")
    }

    /**
     * Fetch leaderboard scores for a specific game.
     * Returns a list of formatted strings like "Title: Score (Rank #N)"
     * Returns null on error, empty list if no leaderboards.
     */
    suspend fun fetchGameLeaderboards(gameId: Int): List<String>? =
        withContext(Dispatchers.IO) {
            try {
                val authManager = RetroAchievementsAuthManager.getInstance(context)
                val username = authManager.getCurrentRAUsername()
                val sessionToken = getUserApiTokenNative()

                if (username.isNullOrEmpty() || sessionToken.isNullOrEmpty()) return@withContext null

                val doRequestUrl = "https://retroachievements.org/dorequest.php"

                // Step 1: Fetch game patch data to get leaderboard ID list
                val patchBody =
                    "r=patch&u=$username&t=$sessionToken&g=$gameId"
                        .toRequestBody("application/x-www-form-urlencoded".toMediaType())
                val patchRequest =
                    Request
                        .Builder()
                        .url(doRequestUrl)
                        .post(patchBody)
                        .addHeader("User-Agent", "YabaSanshiro/1.17.7")
                        .build()

                val patchResponse = httpClient.newCall(patchRequest).execute()
                if (!patchResponse.isSuccessful) {
                    Log.w(TAG, "Patch request failed: ${patchResponse.code}")
                    return@withContext null
                }

                val patchJson =
                    org.json.JSONObject(patchResponse.body?.string() ?: return@withContext null)
                if (!patchJson.optBoolean("Success", false)) return@withContext null

                val leaderboardsArray =
                    patchJson
                        .optJSONObject("PatchData")
                        ?.optJSONArray("Leaderboards")
                        ?: return@withContext emptyList()

                if (leaderboardsArray.length() == 0) return@withContext emptyList()

                // Step 2: For each leaderboard (limit 5), fetch entries around the user
                val results = mutableListOf<String>()
                for (i in 0 until minOf(leaderboardsArray.length(), 5)) {
                    val lb = leaderboardsArray.optJSONObject(i) ?: continue
                    val lbId = lb.optInt("ID", 0)
                    val lbTitle = lb.optString("Title", "Unknown")
                    if (lbId == 0) continue

                    val lbinfoBody =
                        "r=lbinfo&i=$lbId&u=$username&c=3"
                            .toRequestBody("application/x-www-form-urlencoded".toMediaType())
                    val lbinfoRequest =
                        Request
                            .Builder()
                            .url(doRequestUrl)
                            .post(lbinfoBody)
                            .addHeader("User-Agent", "YabaSanshiro/1.17.7")
                            .build()

                    val lbinfoResponse = httpClient.newCall(lbinfoRequest).execute()
                    if (!lbinfoResponse.isSuccessful) continue

                    val lbinfoJson =
                        org.json.JSONObject(lbinfoResponse.body?.string() ?: continue)
                    if (!lbinfoJson.optBoolean("Success", false)) continue

                    val lbData = lbinfoJson.optJSONObject("LeaderboardData") ?: continue
                    val totalEntries = lbData.optInt("TotalEntries", 0)
                    val entries = lbData.optJSONArray("Entries") ?: continue

                    for (j in 0 until entries.length()) {
                        val entry = entries.optJSONObject(j) ?: continue
                        if (entry.optString("User", "").equals(username, ignoreCase = true)) {
                            val score = entry.optLong("Score", 0)
                            val rank = entry.optInt("Rank", 0)
                            if (rank > 0) {
                                val rankText = if (totalEntries > 0) "#$rank / $totalEntries" else "#$rank"
                                results.add("$lbTitle: $score ($rankText)")
                            }
                            break
                        }
                    }
                }
                results
            } catch (e: Exception) {
                Log.e(TAG, "Error fetching leaderboards", e)
                null
            }
        }
}
