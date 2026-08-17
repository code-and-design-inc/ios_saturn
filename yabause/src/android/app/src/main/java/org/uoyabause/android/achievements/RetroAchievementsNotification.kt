package org.uoyabause.android.achievements

import android.animation.AnimatorSet
import android.animation.ObjectAnimator
import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.util.Log
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.game.BaseGame
import java.io.File
import java.io.FileOutputStream

/**
 * RetroAchievements notification display system
 * Shows achievement unlocks, leaderboard submissions, etc.
 */
class RetroAchievementsNotification(
    private val context: Context,
) {
    // Game context for Firebase integration
    var currentGame: BaseGame? = null

    companion object {
        private const val TAG = "RANotification"
        private const val NOTIFICATION_DURATION = 4000L
        private const val ANIMATION_DURATION = 300L
        private const val MAX_VISIBLE_NOTIFICATIONS = 3
    }

    /**
     * Data class to track notification information
     */
    data class NotificationInfo(
        val view: View,
        val dismissJob: Job,
        val id: String = System.currentTimeMillis().toString(),
    )

    private val notificationScope = CoroutineScope(Dispatchers.Main + SupervisorJob())
    private val activeNotifications = mutableListOf<NotificationInfo>()
    private var notificationContainer: LinearLayout? = null
    private val httpClient =
        OkHttpClient
            .Builder()
            .connectTimeout(10, java.util.concurrent.TimeUnit.SECONDS)
            .readTimeout(30, java.util.concurrent.TimeUnit.SECONDS)
            .build()

    /**
     * Show achievement unlocked notification
     */
    fun showAchievementUnlocked(
        achievementId: Int,
        title: String,
        description: String,
        points: Int = 0,
        imageUrl: String? = null,
        isUnofficial: Boolean = false,
    ) {
        notificationScope.launch {
            // Download achievement image if URL provided
            val achievementImage = imageUrl?.let { downloadAchievementImage(it, "ach_$achievementId.png") }

            val notificationView = createAchievementNotification(title, description, points, achievementImage, isUnofficial)
            addNotificationToStack(notificationView)
        }
    }

    /**
     * Download and cache achievement image
     */
    private suspend fun downloadAchievementImage(
        imageUrl: String,
        filename: String,
    ): Bitmap? =
        withContext(Dispatchers.IO) {
            try {
                Log.d(TAG, "Downloading achievement image: $imageUrl")

                // Check if image is already cached
                val cacheDir = File(context.cacheDir, "achievements")
                if (!cacheDir.exists()) {
                    cacheDir.mkdirs()
                }

                val cachedFile = File(cacheDir, filename)
                if (cachedFile.exists()) {
                    Log.d(TAG, "Using cached image: $filename")
                    return@withContext BitmapFactory.decodeFile(cachedFile.absolutePath)
                }

                // Download image
                val request =
                    Request
                        .Builder()
                        .url(imageUrl)
                        .addHeader("User-Agent", "YabaSanshiro/1.17.7")
                        .build()

                val response = httpClient.newCall(request).execute()
                if (response.isSuccessful) {
                    response.body?.byteStream()?.use { inputStream ->
                        // Save to cache
                        FileOutputStream(cachedFile).use { outputStream ->
                            inputStream.copyTo(outputStream)
                        }

                        // Decode bitmap
                        val bitmap = BitmapFactory.decodeFile(cachedFile.absolutePath)
                        Log.d(TAG, "Successfully downloaded and cached achievement image: $filename")
                        return@withContext bitmap
                    }
                } else {
                    Log.e(TAG, "Failed to download image: HTTP ${response.code}")
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error downloading achievement image", e)
            }
            return@withContext null
        }

    /**
     * Add a notification to the stack with proper positioning
     */
    private fun addNotificationToStack(notificationView: View) {
        // Ensure notification container exists
        if (notificationContainer == null) {
            createNotificationContainer()
        }

        // Remove oldest notification if we've reached the limit
        if (activeNotifications.size >= MAX_VISIBLE_NOTIFICATIONS) {
            removeOldestNotification()
        }

        // Set up auto-dismiss job for this notification
        val dismissJob =
            notificationScope.launch {
                delay(NOTIFICATION_DURATION)
                removeNotification(notificationView)
            }

        // Create notification info
        val notificationInfo = NotificationInfo(notificationView, dismissJob)
        activeNotifications.add(notificationInfo)

        // Add to container and animate in
        showNotificationInStack(notificationView)
    }

    /**
     * Create the notification container if it doesn't exist
     */
    private fun createNotificationContainer() {
        val rootView = findRootView()
        if (rootView != null) {
            notificationContainer =
                LinearLayout(context).apply {
                    orientation = LinearLayout.VERTICAL
                    layoutParams =
                        FrameLayout
                            .LayoutParams(
                                ViewGroup.LayoutParams.WRAP_CONTENT,
                                ViewGroup.LayoutParams.WRAP_CONTENT,
                            ).apply {
                                gravity = Gravity.TOP or Gravity.START
                                setMargins(24, 24, 24, 24)
                            }
                }
            rootView.addView(notificationContainer)
        }
    }

    /**
     * Show notification in the stack with animation
     */
    private fun showNotificationInStack(notificationView: View) {
        notificationContainer?.let { container ->
            // Start with notification off-screen (slide from top)
            notificationView.translationY = -200f
            notificationView.alpha = 0f

            // Add to container
            container.addView(notificationView)

            // Animate in (slide down from top)
            val animatorSet =
                AnimatorSet().apply {
                    playTogether(
                        ObjectAnimator.ofFloat(notificationView, "translationY", -200f, 0f),
                        ObjectAnimator.ofFloat(notificationView, "alpha", 0f, 1f),
                    )
                    duration = ANIMATION_DURATION
                }
            animatorSet.start()
        } ?: run {
            // Fallback if container couldn't be created
            Log.e(TAG, "Could not create notification container, falling back to Toast")
            Toast.makeText(context, "Achievement Unlocked!", Toast.LENGTH_LONG).show()
        }
    }

    /**
     * Remove a specific notification from the stack
     */
    private fun removeNotification(notificationView: View) {
        // Find and remove the notification info
        val notificationInfo = activeNotifications.find { it.view == notificationView }
        if (notificationInfo != null) {
            // Cancel the dismiss job if it's still active
            notificationInfo.dismissJob.cancel()
            activeNotifications.remove(notificationInfo)
        }

        // Animate out and remove from container
        val animatorSet =
            AnimatorSet().apply {
                playTogether(
                    ObjectAnimator.ofFloat(notificationView, "translationY", 0f, -200f),
                    ObjectAnimator.ofFloat(notificationView, "alpha", 1f, 0f),
                )
                duration = ANIMATION_DURATION
            }

        animatorSet.start()

        // Remove from container after animation
        notificationScope.launch {
            delay(ANIMATION_DURATION)
            notificationContainer?.removeView(notificationView)

            // If no more notifications, remove the container
            if (activeNotifications.isEmpty()) {
                cleanupNotificationContainer()
            }
        }
    }

    /**
     * Remove the oldest notification to make room for new ones
     */
    private fun removeOldestNotification() {
        if (activeNotifications.isNotEmpty()) {
            val oldestNotification = activeNotifications.first()
            removeNotification(oldestNotification.view)
        }
    }

    /**
     * Clean up the notification container when no notifications are active
     */
    private fun cleanupNotificationContainer() {
        notificationContainer?.let { container ->
            val parent = container.parent as? ViewGroup
            parent?.removeView(container)
            notificationContainer = null
        }
    }

    /**
     * Show leaderboard submission notification and submit to Firebase
     */
    fun showLeaderboardSubmit(
        leaderboardId: Int,
        title: String,
        description: String,
        scoreString: String,
    ) {
        notificationScope.launch {
            // Show notification UI - display the formatted score string
            val notificationView = createLeaderboardNotification(title, description, scoreString)
            addNotificationToStack(notificationView)

            // Only submit to Firebase if we have actual score data (not empty from submission event)
            if (scoreString.isNotEmpty()) {
                // Parse score string to numeric value for Firebase storage
                val numericScore = parseScoreString(scoreString)

                // Submit to Firebase if game context is available
                submitToFirebase(leaderboardId.toString(), numericScore, title)
            } else {
                Log.d(TAG, "Received empty score string - showing notification only (submission in progress)")
            }
        }
    }

    /**
     * Parse RetroAchievements score string to numeric value for Firebase storage
     */
    private fun parseScoreString(scoreString: String): Long {
        Log.d(TAG, "Parsing score string: '$scoreString' (length: ${scoreString.length})")
        return try {
            // Handle common score formats from RetroAchievements
            val result =
                when {
                    // Time format: "01:23.45" or "1:23:45.67"
                    scoreString.contains(":") -> {
                        val parts = scoreString.split(":")
                        val seconds = parts.last().toDoubleOrNull() ?: 0.0
                        val minutes = parts.dropLast(1).lastOrNull()?.toLongOrNull() ?: 0L
                        val hours = if (parts.size > 2) parts[0].toLongOrNull() ?: 0L else 0L

                        // Convert to milliseconds for precision
                        ((hours * 3600 + minutes * 60) * 1000 + (seconds * 1000).toLong())
                    }
                    // Score with commas: "123,456"
                    scoreString.contains(",") -> {
                        scoreString.replace(",", "").toLongOrNull() ?: 0L
                    }
                    // Plain number: "12345"
                    else -> {
                        scoreString.trim().toLongOrNull() ?: 0L
                    }
                }
            Log.d(TAG, "Parsed score: '$scoreString' -> $result")
            result
        } catch (e: Exception) {
            Log.w(TAG, "Failed to parse score string: '$scoreString'", e)
            0L
        }
    }

    /**
     * Submit RetroAchievements leaderboard score to Firebase
     */
    private suspend fun submitToFirebase(
        leaderboardId: String,
        score: Long,
        leaderboardTitle: String,
    ) = withContext(Dispatchers.IO) {
        val game = currentGame
        if (game == null) {
            Log.w(TAG, "No game context available for Firebase submission")
            return@withContext
        }

        // Get current username from Firebase Auth or use a fallback
        val currentUser = com.google.firebase.auth.FirebaseAuth
            .getInstance()
            .currentUser
        if (currentUser == null) {
            Log.w(TAG, "No authenticated user for Firebase submission")
            return@withContext
        }

        val userName = currentUser.displayName ?: currentUser.email ?: "RetroAchievements User"

        Log.d(TAG, "Submitting RetroAchievements leaderboard score to Firebase: leaderboard=$leaderboardId, score=$score, user=$userName")

        try {
            game.submitRetroAchievementsScore(
                retroAchievementsLeaderboardId = leaderboardId,
                score = score,
                userName = userName,
                title = leaderboardTitle,
                onSuccess = {
                    Log.d(TAG, "Successfully submitted RetroAchievements score to Firebase")
                    // Optional: Show success feedback on main thread
                    notificationScope.launch(Dispatchers.Main) {
                        // Could show a small toast or update the notification
                    }
                },
                onFailure = { exception ->
                    Log.e(TAG, "Failed to submit RetroAchievements score to Firebase", exception)
                    // Optional: Show error feedback on main thread
                    notificationScope.launch(Dispatchers.Main) {
                        // Could show a small toast about the error
                    }
                },
            )
        } catch (e: Exception) {
            Log.e(TAG, "Error during Firebase submission", e)
        }
    }

    /**
     * Show rich presence update (optional, can be disabled)
     */
    fun showRichPresenceUpdate(richPresence: String) {
        // Rich presence updates are usually shown in a status bar or overlay
        // For now, just log it
        android.util.Log.d(TAG, "Rich presence: $richPresence")
    }

    /**
     * Create achievement notification view
     */
    private fun createAchievementNotification(
        title: String,
        description: String,
        points: Int = 0,
        achievementImage: Bitmap? = null,
        isUnofficial: Boolean = false,
    ): View {
        Log.d(TAG, "Creating achievement notification $title $description $points isUnofficial:$isUnofficial")
        // Get screen dimensions for responsive sizing
        val displayMetrics = context.resources.displayMetrics
        val screenWidth = displayMetrics.widthPixels

        val container =
            FrameLayout(context).apply {
                layoutParams =
                    LinearLayout
                        .LayoutParams(
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            setMargins(0, 8, 0, 8) // Vertical spacing for stacking
                        }
            }

        // Create notification background
        val borderColor = if (isUnofficial) "#FFA500" else "#FFD700" // Orange for unofficial, gold for official
        var background =
            GradientDrawable().apply {
                cornerRadius = 16f
                setColor(Color.parseColor("#AA000000"))
                setStroke(2, Color.parseColor(borderColor))
            }

        // Create content view with sufficient padding for text
        val contentView =
            FrameLayout(context).apply {
                layoutParams =
                    FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                    )
                setPadding(16, 16, 16, 16) // Increased padding for better text spacing
                setBackground(background)
                minimumHeight = 110 // Increased minimum height to accommodate text properly
            }

        // Achievement icon (use downloaded image or placeholder) - smaller for compact layout
        val icon =
            ImageView(context).apply {
                layoutParams =
                    FrameLayout.LayoutParams(48, 48).apply {
                        // Smaller icon for compact layout
                        gravity = Gravity.START or Gravity.TOP
                        topMargin = 4
                    }

                if (achievementImage != null) {
                    setImageBitmap(achievementImage)
                    scaleType = ImageView.ScaleType.CENTER_CROP
                    // Add border to achievement image
                    val borderDrawable =
                        GradientDrawable().apply {
                            cornerRadius = 6f
                            setStroke(1, Color.parseColor(borderColor))
                        }
                    background = borderDrawable
                } else {
                    setImageResource(android.R.drawable.star_on)
                    setColorFilter(Color.parseColor(borderColor))
                }
            }

        // Text container with proper spacing
        val textContainer =
            LinearLayout(context).apply {
                layoutParams =
                    FrameLayout
                        .LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            leftMargin = 56 // Space for smaller icon
                            rightMargin = 8 // Space for points text
                            gravity = Gravity.TOP // Align to top instead of center to prevent overlap
                        }
                orientation = LinearLayout.VERTICAL
                setPadding(0, 4, 0, 4)
            }

        // Header text (Achievement Unlocked or Unofficial Achievement Unlocked)
        val headerText =
            TextView(context).apply {
                text =
                    if (isUnofficial) {
                        context.getString(
                            R.string.retroachievements_unofficial_achievement,
                        )
                    } else {
                        context.getString(R.string.retroachievements_achievement_unlocked)
                    }
                textSize = 10f
                setTextColor(Color.parseColor("#AAAAAA"))
                setTypeface(null, android.graphics.Typeface.BOLD)
                maxLines = 1
                setSingleLine(true)
                layoutParams =
                    LinearLayout
                        .LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            bottomMargin = 4 // Increased margin for better spacing
                        }
            }

        // Title text
        val titleText =
            TextView(context).apply {
                text = title
                textSize = 14f
                setTextColor(Color.WHITE)
                setTypeface(null, android.graphics.Typeface.BOLD)
                maxLines = 1
                setSingleLine(true)
                layoutParams =
                    LinearLayout
                        .LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            bottomMargin = 4 // Increased margin for better spacing
                        }
            }

        // Description text
        val descText =
            TextView(context).apply {
                text = description
                textSize = 11f
                setTextColor(Color.parseColor("#CCCCCC"))
                maxLines = 2
                layoutParams =
                    LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                    )
            }

        // Points text (if points > 0) - positioned separately in top right
        val pointsText =
            if (points > 0) {
                TextView(context).apply {
                    text = "${points}${context.getString(R.string.retroachievements_points_abbreviation)}"
                    textSize = 10f
                    setTextColor(Color.parseColor(borderColor))
                    setTypeface(null, android.graphics.Typeface.BOLD)
                    layoutParams =
                        FrameLayout
                            .LayoutParams(
                                ViewGroup.LayoutParams.WRAP_CONTENT,
                                ViewGroup.LayoutParams.WRAP_CONTENT,
                            ).apply {
                                gravity = Gravity.END or Gravity.TOP
                                topMargin = 4
                                rightMargin = 4
                            }
                }
            } else {
                null
            }

        // Add all text views to the linear layout container
        textContainer.addView(headerText)
        textContainer.addView(titleText)
        textContainer.addView(descText)

        // Add all components to the content view
        contentView.addView(icon)
        contentView.addView(textContainer)
        pointsText?.let { contentView.addView(it) }
        container.addView(contentView)

        return container
    }

    /**
     * Create leaderboard notification view
     */
    private fun createLeaderboardNotification(
        title: String,
        description: String,
        scoreString: String,
    ): View {
        // Get screen dimensions for responsive sizing
        val displayMetrics = context.resources.displayMetrics
        val screenWidth = displayMetrics.widthPixels
        // val notificationWidth = (screenWidth / 4) // 1/4 of screen width for better stacking

        val container =
            FrameLayout(context).apply {
                layoutParams =
                    LinearLayout
                        .LayoutParams(
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            setMargins(0, 8, 0, 8) // Vertical spacing for stacking
                        }
            }

        // Create notification background with blue border for leaderboard
        val borderColor = "#00BFFF"
        var background =
            GradientDrawable().apply {
                cornerRadius = 16f
                setColor(Color.parseColor("#AA000000"))
                setStroke(2, Color.parseColor(borderColor))
            }

        // Create content view with sufficient padding for text
        val contentView =
            FrameLayout(context).apply {
                layoutParams =
                    FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                    )
                setPadding(16, 12, 16, 12) // Reduced padding for compact size
                setBackground(background)
                minimumHeight = 100 // Ensure minimum height for text content
            }

        // Leaderboard icon - smaller for compact layout
        val icon =
            ImageView(context).apply {
                layoutParams =
                    FrameLayout.LayoutParams(48, 48).apply {
                        // Smaller icon for compact layout
                        gravity = Gravity.START or Gravity.TOP
                        topMargin = 4
                    }
                setImageResource(android.R.drawable.ic_menu_sort_by_size) // Leaderboard icon
                setColorFilter(Color.parseColor(borderColor))
            }

        // Text container with proper spacing
        val textContainer =
            LinearLayout(context).apply {
                layoutParams =
                    FrameLayout
                        .LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            leftMargin = 56 // Space for smaller icon
                            rightMargin = 8 // Space for score text
                            gravity = Gravity.CENTER_VERTICAL
                        }
                orientation = LinearLayout.VERTICAL
                setPadding(0, 4, 0, 4)
            }

        // Header text
        val headerText =
            TextView(context).apply {
                text = context.getString(R.string.retroachievements_leaderboard_notification)
                textSize = 10f
                setTextColor(Color.parseColor("#AAAAAA"))
                setTypeface(null, android.graphics.Typeface.BOLD)
                layoutParams =
                    LinearLayout
                        .LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            bottomMargin = 2
                        }
            }

        // Title text
        val titleText =
            TextView(context).apply {
                text = title
                textSize = 14f
                setTextColor(Color.WHITE)
                setTypeface(null, android.graphics.Typeface.BOLD)
                maxLines = 1
                setSingleLine(true)
                layoutParams =
                    LinearLayout
                        .LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            bottomMargin = 2
                        }
            }

        // Description text
        val descText =
            TextView(context).apply {
                text = description
                textSize = 11f
                setTextColor(Color.parseColor("#CCCCCC"))
                maxLines = 2
                layoutParams =
                    LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                    )
            }

        // Score text (positioned separately in top right)
        val scoreText =
            TextView(context).apply {
                text =
                    if (scoreString.isNotEmpty() &&
                        scoreString != "0"
                    ) {
                        scoreString
                    } else {
                        context.getString(R.string.retroachievements_submit_score)
                    }
                textSize = 10f
                setTextColor(Color.parseColor(borderColor))
                setTypeface(null, android.graphics.Typeface.BOLD)
                layoutParams =
                    FrameLayout
                        .LayoutParams(
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            gravity = Gravity.END or Gravity.TOP
                            topMargin = 4
                            rightMargin = 4
                        }
            }

        // Add all text views to the linear layout container
        textContainer.addView(headerText)
        textContainer.addView(titleText)
        textContainer.addView(descText)

        // Add all components to the content view
        contentView.addView(icon)
        contentView.addView(textContainer)
        contentView.addView(scoreText)
        container.addView(contentView)

        return container
    }

    /**
     * Find a suitable root view for the notification
     */
    private fun findRootView(): ViewGroup? {
        // Method 1: Try to cast context as Activity
        val activity = context as? android.app.Activity
        activity?.findViewById<ViewGroup>(android.R.id.content)?.let {
            Log.d(TAG, "Found root view via Activity.findViewById")
            return it
        }

        // Method 2: Try to get current activity via reflection or other means
        try {
            val currentActivity = getCurrentActivity()
            currentActivity?.findViewById<ViewGroup>(android.R.id.content)?.let {
                Log.d(TAG, "Found root view via getCurrentActivity")
                return it
            }
        } catch (e: Exception) {
            Log.w(TAG, "Could not get current activity", e)
        }

        // Method 3: Try WindowManager approach
        try {
            val windowManager = context.getSystemService(Context.WINDOW_SERVICE) as? android.view.WindowManager
            if (windowManager != null) {
                Log.d(TAG, "Using WindowManager approach")
                // This would require additional setup for overlay windows
                // For now, return null to trigger fallback
            }
        } catch (e: Exception) {
            Log.w(TAG, "WindowManager approach failed", e)
        }

        Log.w(TAG, "Could not find suitable root view")
        return null
    }

    /**
     * Try to get the current activity
     */
    private fun getCurrentActivity(): android.app.Activity? {
        try {
            // If context is already an Activity
            if (context is android.app.Activity) {
                return context
            }

            // Try to get activity from ApplicationContext
            if (context is android.app.Application) {
                // This would require tracking the current activity
                // For now, return null
                return null
            }
        } catch (e: Exception) {
            Log.w(TAG, "Error getting current activity", e)
        }
        return null
    }

    /**
     * Show login success notification following rcheevos guidelines
     * @param username The logged in username
     * @param displayName Display name (may be different from username)
     * @param points User's total points
     */
    fun showLoginSuccess(
        username: String,
        displayName: String? = null,
        points: Int = 0,
    ) {
        notificationScope.launch {
            // Create login success notification using existing method
            val title = "RetroAchievements Login"
            val message = "Logged in as ${displayName ?: username}${if (points > 0) " ($points points)" else ""}"

            // Create a custom notification view for login success (green theme)
            val notificationView = createLoginSuccessNotification(title, message)
            addNotificationToStack(notificationView)
        }
    }

    /**
     * Create login success notification with green theme
     */
    private fun createLoginSuccessNotification(
        title: String,
        message: String,
    ): View {
        // Get screen dimensions for responsive sizing
        val displayMetrics = context.resources.displayMetrics
        val screenWidth = displayMetrics.widthPixels
        val notificationWidth = (screenWidth / 3) // 1/3 of screen width

        val container =
            FrameLayout(context).apply {
                layoutParams =
                    FrameLayout
                        .LayoutParams(
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            setMargins(24, 24, 24, 24)
                            gravity = Gravity.TOP or Gravity.START // Left top positioning
                        }
            }

        // Create notification background with green theme for login success
        val background =
            GradientDrawable().apply {
                cornerRadius = 16f
                setColor(Color.parseColor("#AA1B5E20")) // Dark green with transparency
                setStroke(2, Color.parseColor("#4CAF50")) // Green border
            }

        // Create content view
        val contentView =
            FrameLayout(context).apply {
                layoutParams =
                    FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                    )
                setBackground(background)
                setPadding(16, 16, 16, 16)
            }

        // Create main layout
        val mainLayout =
            LinearLayout(context).apply {
                orientation = LinearLayout.VERTICAL
                layoutParams =
                    FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                    )
            }

        // Title text
        val titleText =
            TextView(context).apply {
                text = context.getString(R.string.retroachievements_login_notification)
                setTextColor(Color.WHITE)
                textSize = 14f
                typeface = Typeface.DEFAULT_BOLD
                layoutParams =
                    LinearLayout
                        .LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            setMargins(0, 0, 0, 8)
                        }
            }

        // Message text
        val messageText =
            TextView(context).apply {
                text = message
                setTextColor(Color.WHITE)
                textSize = 12f
                layoutParams =
                    LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                    )
            }

        mainLayout.addView(titleText)
        mainLayout.addView(messageText)
        contentView.addView(mainLayout)
        container.addView(contentView)

        return container
    }

    /**
     * Show game placard notification following rcheevos guidelines
     * @param gameTitle The game title
     * @param imageUrl Game badge image URL (optional)
     * @param unlockedAchievements Number of unlocked achievements
     * @param totalAchievements Total number of achievements
     */
    fun showGamePlacard(
        gameTitle: String,
        imageUrl: String? = null,
        unlockedAchievements: Int = 0,
        totalAchievements: Int = 0,
    ) {
        notificationScope.launch {
            // Create game placard notification
            val notificationView = createGamePlacardNotification(gameTitle, imageUrl, unlockedAchievements, totalAchievements)
            addNotificationToStack(notificationView)
        }
    }

    /**
     * Create game placard notification with blue/purple theme
     */
    private fun createGamePlacardNotification(
        gameTitle: String,
        imageUrl: String?,
        unlockedAchievements: Int,
        totalAchievements: Int,
    ): View {
        // Get screen dimensions for responsive sizing
        val displayMetrics = context.resources.displayMetrics
        val screenWidth = displayMetrics.widthPixels
        val notificationWidth = (screenWidth / 2) // Wider for game placard (1/2 screen width)

        val container =
            FrameLayout(context).apply {
                layoutParams =
                    FrameLayout
                        .LayoutParams(
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            setMargins(24, 24, 24, 24)
                            gravity = Gravity.TOP or Gravity.END // Right top positioning
                        }
            }

        // Create notification background with blue/purple theme for game placard
        val background =
            GradientDrawable().apply {
                cornerRadius = 16f
                setColor(Color.parseColor("#AA1A237E")) // Deep blue with transparency
                setStroke(2, Color.parseColor("#3F51B5")) // Blue border
            }

        // Create content view
        val contentView =
            FrameLayout(context).apply {
                layoutParams =
                    FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                    )
                setBackground(background)
                setPadding(16, 16, 16, 16)
            }

        // Create main layout (horizontal for image + text)
        val mainLayout =
            LinearLayout(context).apply {
                orientation = LinearLayout.HORIZONTAL
                layoutParams =
                    FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                    )
            }

        // Game badge image (placeholder for now)
        val imageView =
            ImageView(context).apply {
                layoutParams =
                    LinearLayout.LayoutParams(48, 48).apply {
                        setMargins(0, 0, 12, 0)
                    }
                scaleType = ImageView.ScaleType.CENTER_CROP
                setImageResource(android.R.drawable.ic_menu_gallery) // Placeholder
            }

        // Text layout
        val textLayout =
            LinearLayout(context).apply {
                orientation = LinearLayout.VERTICAL
                layoutParams =
                    LinearLayout.LayoutParams(
                        0,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                        1f,
                    )
            }

        // Game title
        val titleText =
            TextView(context).apply {
                text = gameTitle
                setTextColor(Color.WHITE)
                textSize = 14f
                typeface = Typeface.DEFAULT_BOLD
                layoutParams =
                    LinearLayout
                        .LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            setMargins(0, 0, 0, 4)
                        }
            }

        // Achievement progress message
        val progressText =
            TextView(context).apply {
                text =
                    if (totalAchievements == 0) {
                        context.getString(R.string.retroachievements_no_achievements_message)
                    } else {
                        context.getString(R.string.retroachievements_game_achievements_message, unlockedAchievements, totalAchievements)
                    }
                setTextColor(Color.WHITE)
                textSize = 12f
                layoutParams =
                    LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                    )
            }

        textLayout.addView(titleText)
        textLayout.addView(progressText)
        mainLayout.addView(imageView)
        mainLayout.addView(textLayout)
        contentView.addView(mainLayout)
        container.addView(contentView)

        // Download game badge image if URL provided
        if (imageUrl != null) {
            notificationScope.launch {
                val gameImage = downloadGameImage(imageUrl, "game_${gameTitle.hashCode()}.png")
                if (gameImage != null) {
                    imageView.setImageBitmap(gameImage)
                }
            }
        }

        return container
    }

    /**
     * Download and cache game image
     */
    private suspend fun downloadGameImage(
        imageUrl: String,
        filename: String,
    ): Bitmap? =
        withContext(Dispatchers.IO) {
            try {
                Log.d(TAG, "Downloading game image: $imageUrl")

                // Use same caching logic as achievement images
                val cacheDir = File(context.cacheDir, "games")
                if (!cacheDir.exists()) {
                    cacheDir.mkdirs()
                }

                val cachedFile = File(cacheDir, filename)
                if (cachedFile.exists()) {
                    Log.d(TAG, "Using cached game image: $filename")
                    return@withContext BitmapFactory.decodeFile(cachedFile.absolutePath)
                }

                // Download image
                val request =
                    Request
                        .Builder()
                        .url(imageUrl)
                        .addHeader("User-Agent", "YabaSanshiro/1.17.7")
                        .build()

                val response = httpClient.newCall(request).execute()
                if (response.isSuccessful) {
                    response.body?.byteStream()?.use { inputStream ->
                        // Save to cache
                        FileOutputStream(cachedFile).use { outputStream ->
                            inputStream.copyTo(outputStream)
                        }

                        // Decode bitmap
                        val bitmap = BitmapFactory.decodeFile(cachedFile.absolutePath)
                        Log.d(TAG, "Successfully downloaded and cached game image: $filename")
                        return@withContext bitmap
                    }
                } else {
                    Log.w(TAG, "Failed to download game image: ${response.code}")
                }
                null
            } catch (e: Exception) {
                Log.e(TAG, "Error downloading game image", e)
                null
            }
        }

    /**
     * Show game mastery notification
     * @param gameTitle The mastered game title
     * @param imageUrl Game badge image URL (optional)
     * @param achievementCount Number of achievements in the game
     * @param points Total points earned
     * @param isHardcore Whether mastery was achieved in hardcore mode
     * @param username The user's username for personalized message
     * @param playtime Optional playtime string
     */
    fun showGameMastery(
        gameTitle: String,
        imageUrl: String? = null,
        achievementCount: Int = 0,
        points: Int = 0,
        isHardcore: Boolean = false,
        username: String? = null,
        playtime: String? = null,
    ) {
        notificationScope.launch {
            // Download game image if URL provided
            val gameImage = imageUrl?.let { downloadGameImage(it, "game_${gameTitle.hashCode()}.png") }

            val notificationView =
                createGameMasteryNotification(
                    gameTitle,
                    gameImage,
                    achievementCount,
                    points,
                    isHardcore,
                    username,
                    playtime,
                )
            addNotificationToStack(notificationView)
        }
    }

    /**
     * Create game mastery notification with special styling
     */
    private fun createGameMasteryNotification(
        gameTitle: String,
        gameImage: Bitmap?,
        achievementCount: Int,
        points: Int,
        isHardcore: Boolean,
        username: String?,
        playtime: String?,
    ): View {
        // Get screen dimensions for responsive sizing
        val displayMetrics = context.resources.displayMetrics
        val screenWidth = displayMetrics.widthPixels
        val notificationWidth = (screenWidth / 2) // Half screen width for mastery

        val container =
            FrameLayout(context).apply {
                layoutParams =
                    FrameLayout
                        .LayoutParams(
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            setMargins(24, 24, 24, 24)
                            gravity = Gravity.CENTER // Center positioning for special mastery notification
                        }
            }

        // Create special mastery background with gold/platinum theme
        val borderColor = if (isHardcore) "#FF6B35" else "#FFD700" // Orange for hardcore, gold for standard
        val backgroundColor = if (isHardcore) "#AA4A148C" else "#AA1A237E" // Purple for hardcore, blue for standard
        var background =
            GradientDrawable().apply {
                cornerRadius = 20f
                setColor(Color.parseColor(backgroundColor))
                setStroke(3, Color.parseColor(borderColor)) // Thicker border for mastery
            }

        // Create content view
        val contentView =
            FrameLayout(context).apply {
                layoutParams =
                    FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                    )
                setPadding(20, 16, 20, 16) // More padding for special notification
                setBackground(background)
                minimumHeight = 120 // Taller for mastery notification
            }

        // Game image (larger for mastery)
        val imageView =
            ImageView(context).apply {
                layoutParams =
                    FrameLayout.LayoutParams(64, 64).apply {
                        // Larger image for mastery
                        gravity = Gravity.START or Gravity.TOP
                        topMargin = 8
                    }

                if (gameImage != null) {
                    setImageBitmap(gameImage)
                    scaleType = ImageView.ScaleType.CENTER_CROP
                    // Add special border to game image
                    val borderDrawable =
                        GradientDrawable().apply {
                            cornerRadius = 8f
                            setStroke(2, Color.parseColor(borderColor))
                        }
                    background = borderDrawable
                } else {
                    setImageResource(android.R.drawable.star_big_on)
                    setColorFilter(Color.parseColor(borderColor))
                }
            }

        // Text container with proper spacing
        val textContainer =
            LinearLayout(context).apply {
                layoutParams =
                    FrameLayout
                        .LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            leftMargin = 76 // Space for larger image
                            rightMargin = 8
                            gravity = Gravity.CENTER_VERTICAL
                        }
                orientation = LinearLayout.VERTICAL
                setPadding(0, 8, 0, 8)
            }

        // Header text with special mastery message
        val headerText =
            TextView(context).apply {
                text =
                    if (isHardcore) {
                        context.getString(
                            R.string.retroachievements_mastery_hardcore,
                        )
                    } else {
                        context.getString(R.string.retroachievements_mastery_completed)
                    }
                textSize = 12f
                setTextColor(Color.parseColor(borderColor))
                setTypeface(null, android.graphics.Typeface.BOLD)
                layoutParams =
                    LinearLayout
                        .LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            bottomMargin = 4
                        }
            }

        // Game title
        val titleText =
            TextView(context).apply {
                text = gameTitle
                textSize = 16f // Larger text for mastery
                setTextColor(Color.WHITE)
                setTypeface(null, android.graphics.Typeface.BOLD)
                maxLines = 2
                layoutParams =
                    LinearLayout
                        .LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            bottomMargin = 4
                        }
            }

        // Achievement and points info
        val achievementText =
            TextView(context).apply {
                text = context.getString(R.string.retroachievements_mastery_achievements_points, achievementCount, points)
                textSize = 11f
                setTextColor(Color.parseColor("#CCCCCC"))
                layoutParams =
                    LinearLayout
                        .LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            bottomMargin = 2
                        }
            }

        // Username and playtime (if provided)
        val userInfoText =
            if (username != null || playtime != null) {
                TextView(context).apply {
                    val info =
                        buildString {
                            username?.let { append(it) }
                            if (username != null && playtime != null) append(" | ")
                            playtime?.let { append(context.getString(R.string.retroachievements_mastery_playtime, it)) }
                        }
                    text = info
                    textSize = 10f
                    setTextColor(Color.parseColor("#AAAAAA"))
                    layoutParams =
                        LinearLayout.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        )
                }
            } else {
                null
            }

        // Add all text views to the linear layout container
        textContainer.addView(headerText)
        textContainer.addView(titleText)
        textContainer.addView(achievementText)
        userInfoText?.let { textContainer.addView(it) }

        // Add components to the content view
        contentView.addView(imageView)
        contentView.addView(textContainer)
        container.addView(contentView)

        return container
    }

    /**
     * Show server error notification
     * These are errors that won't be retried and should be shown to the user
     * @param errorMessage User-friendly error message
     */
    fun showServerError(errorMessage: String) {
        notificationScope.launch {
            val notificationView = createServerErrorNotification(errorMessage)
            addNotificationToStack(notificationView)
        }
    }

    /**
     * Create server error notification with red theme
     */
    private fun createServerErrorNotification(errorMessage: String): View {
        // Get screen dimensions for responsive sizing
        val displayMetrics = context.resources.displayMetrics
        val screenWidth = displayMetrics.widthPixels
        val notificationWidth = (screenWidth / 2) // Half screen width for error

        val container =
            FrameLayout(context).apply {
                layoutParams =
                    FrameLayout
                        .LayoutParams(
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            setMargins(24, 24, 24, 24)
                            gravity = Gravity.TOP or Gravity.CENTER_HORIZONTAL // Top center positioning for errors
                        }
            }

        // Create error notification background with red theme
        var background =
            GradientDrawable().apply {
                cornerRadius = 16f
                setColor(Color.parseColor("#AAB71C1C")) // Dark red with transparency
                setStroke(2, Color.parseColor("#F44336")) // Red border
            }

        // Create content view
        val contentView =
            FrameLayout(context).apply {
                layoutParams =
                    FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                    )
                setPadding(16, 16, 16, 16)
                background = background
            }

        // Error icon
        val errorIcon =
            ImageView(context).apply {
                layoutParams =
                    FrameLayout.LayoutParams(32, 32).apply {
                        gravity = Gravity.START or Gravity.TOP
                        topMargin = 4
                    }
                setImageResource(android.R.drawable.ic_dialog_alert)
                setColorFilter(Color.parseColor("#F44336"))
            }

        // Text container
        val textContainer =
            LinearLayout(context).apply {
                layoutParams =
                    FrameLayout
                        .LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            leftMargin = 40 // Space for error icon
                            gravity = Gravity.CENTER_VERTICAL
                        }
                orientation = LinearLayout.VERTICAL
                setPadding(0, 4, 0, 4)
            }

        // Header text
        val headerText =
            TextView(context).apply {
                text = context.getString(R.string.retroachievements_server_error)
                textSize = 12f
                setTextColor(Color.parseColor("#F44336"))
                setTypeface(null, android.graphics.Typeface.BOLD)
                layoutParams =
                    LinearLayout
                        .LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            bottomMargin = 4
                        }
            }

        // Error message text
        val messageText =
            TextView(context).apply {
                text = errorMessage
                textSize = 11f
                setTextColor(Color.WHITE)
                maxLines = 3
                layoutParams =
                    LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                    )
            }

        // Add views to containers
        textContainer.addView(headerText)
        textContainer.addView(messageText)
        contentView.addView(errorIcon)
        contentView.addView(textContainer)
        container.addView(contentView)

        return container
    }

    /**
     * Clean up resources
     */
    fun cleanup() {
        notificationScope.cancel()

        // Cancel all active notification dismiss jobs
        activeNotifications.forEach { it.dismissJob.cancel() }
        activeNotifications.clear()

        // Clean up notification container
        cleanupNotificationContainer()

        // Clean up HTTP client
        httpClient.dispatcher.executorService.shutdown()
        httpClient.connectionPool.evictAll()
    }
}
