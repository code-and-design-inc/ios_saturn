package org.uoyabause.android.achievements

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
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import org.devmiyax.yabasanshiro.R
import java.io.File
import java.io.FileOutputStream
import java.util.concurrent.ConcurrentHashMap

/**
 * RetroAchievements persistent overlay system
 * Manages on-screen progress indicators, leaderboard trackers, and challenge indicators
 */
class RetroAchievementsOverlay(
    private val context: Context,
) {
    companion object {
        private const val TAG = "RAOverlay"
        private const val ANIMATION_DURATION = 300L

        @Volatile
        private var instance: RetroAchievementsOverlay? = null

        fun getInstance(context: Context): RetroAchievementsOverlay = instance ?: synchronized(this) {
            instance ?: RetroAchievementsOverlay(context.applicationContext).also { instance = it }
        }
    }

    private val overlayScope = CoroutineScope(Dispatchers.Main + SupervisorJob())
    private val httpClient =
        OkHttpClient
            .Builder()
            .connectTimeout(10, java.util.concurrent.TimeUnit.SECONDS)
            .readTimeout(30, java.util.concurrent.TimeUnit.SECONDS)
            .build()

    // Overlay containers
    private var overlayContainer: FrameLayout? = null

    // Progress indicators (multiple instances)
    private val progressIndicators = ConcurrentHashMap<Int, View>()
    private var progressIndicatorContainer: LinearLayout? = null

    // Leaderboard trackers (multiple instances)
    private val leaderboardTrackers = ConcurrentHashMap<Int, View>()

    // Challenge indicators (multiple instances)
    private val challengeIndicators = ConcurrentHashMap<Int, View>()

    /**
     * Initialize overlay system with the root view container
     */
    fun initialize(rootView: FrameLayout) {
        Log.d(TAG, "Initializing RetroAchievements overlay system")

        // Create overlay container
        overlayContainer =
            FrameLayout(context).apply {
                layoutParams =
                    FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT,
                    )
                isClickable = false
                isFocusable = false
            }

        rootView.addView(overlayContainer)
        Log.d(TAG, "Overlay container added to root view")
    }

    /**
     * Clean up overlay system
     */
    fun cleanup() {
        Log.d(TAG, "Cleaning up overlay system")
        overlayScope.cancel()

        // Run network cleanup on a background thread to avoid NetworkOnMainThreadException
        Thread {
            try {
                httpClient.dispatcher.executorService.shutdown()
                httpClient.connectionPool.evictAll()
            } catch (e: Exception) {
                Log.e(TAG, "Error cleaning up HTTP client", e)
            }
        }.start()

        hideAllProgressIndicators()
        hideAllLeaderboardTrackers()
        hideAllChallengeIndicators()

        overlayContainer?.let { container ->
            (container.parent as? ViewGroup)?.removeView(container)
        }
        overlayContainer = null
    }

    // =================== Progress Indicators ===================

/**
     * Show or update progress indicator for a specific achievement
     */
    fun showProgressIndicator(
        achievementId: Int,
        title: String,
        progress: String,
        imageUrl: String,
        progressPercent: Int,
    ) {
        overlayScope.launch {
            Log.d(TAG, "Showing/updating progress indicator: $achievementId - $title - $progress ($progressPercent%)")

            // Download image if needed
            val achievementImage =
                if (imageUrl.isNotEmpty()) {
                    downloadImageAsync(imageUrl)
                } else {
                    null
                }

            // Check if this achievement already has a progress indicator
            val existingView = progressIndicators[achievementId]
            if (existingView != null) {
                // Update existing indicator
                updateProgressIndicatorContent(existingView, achievementId, title, progress, progressPercent, achievementImage)
            } else {
                // Create new indicator
                val indicatorView = createProgressIndicatorView(achievementId, title, progress, progressPercent, achievementImage)
                progressIndicators[achievementId] = indicatorView
                addProgressIndicatorToStack(indicatorView)
            }
        }
    }

    /**
     * Update progress indicator content (backward compatibility method)
     * Uses achievement ID 0 as default for single indicator
     */
    fun updateProgressIndicator(
        achievementId: Int,
        title: String,
        progress: String,
        imageUrl: String,
        progressPercent: Int,
    ) {
        showProgressIndicator(achievementId, title, progress, imageUrl, progressPercent)
    }

    /**
     * Show the progress indicator on screen (backward compatibility method)
     * This method is deprecated, use showProgressIndicator() instead
     */
    @Deprecated("Use showProgressIndicator(achievementId, title, progress, imageUrl, progressPercent) instead")
    fun showProgressIndicator() {
        // This method is kept for backward compatibility but does nothing
        // The new showProgressIndicator with parameters will handle the display
        Log.w(TAG, "showProgressIndicator() without parameters is deprecated")
    }

    /**
     * Hide the progress indicator from screen (backward compatibility method)
     * This will hide all progress indicators
     */
    @Deprecated("Use hideProgressIndicator(achievementId) instead")
    fun hideProgressIndicator() {
        hideAllProgressIndicators()
    }

    /**
     * Hide specific progress indicator
     */
    fun hideProgressIndicator(achievementId: Int) {
        overlayScope.launch {
            Log.d(TAG, "Hiding progress indicator: $achievementId")

            progressIndicators[achievementId]?.let { view ->
                removeProgressIndicatorFromStack(view)
                progressIndicators.remove(achievementId)
            }
        }
    }

    /**
     * Hide all progress indicators
     */
    private fun hideAllProgressIndicators() {
        progressIndicators.keys.forEach { achievementId ->
            hideProgressIndicator(achievementId)
        }
    }

    /**
     * Add progress indicator to the stack
     */
    private fun addProgressIndicatorToStack(indicatorView: View) {
        // Create container if it doesn't exist
        if (progressIndicatorContainer == null) {
            createProgressIndicatorContainer()
        }

        progressIndicatorContainer?.let { container ->
            // Animate in from top
            indicatorView.alpha = 0f
            indicatorView.translationY = -100f

            container.addView(indicatorView)

            val fadeIn = ObjectAnimator.ofFloat(indicatorView, "alpha", 0f, 1f)
            val slideIn = ObjectAnimator.ofFloat(indicatorView, "translationY", -100f, 0f)

            fadeIn.duration = ANIMATION_DURATION
            slideIn.duration = ANIMATION_DURATION

            fadeIn.start()
            slideIn.start()
        }
    }

    /**
     * Remove progress indicator from the stack
     */
    private fun removeProgressIndicatorFromStack(indicatorView: View) {
        // Animate out to top
        val fadeOut = ObjectAnimator.ofFloat(indicatorView, "alpha", 1f, 0f)
        val slideOut = ObjectAnimator.ofFloat(indicatorView, "translationY", 0f, -100f)

        fadeOut.duration = ANIMATION_DURATION
        slideOut.duration = ANIMATION_DURATION

        fadeOut.start()
        slideOut.start()

        // Remove from container after animation
        overlayScope.launch {
            delay(ANIMATION_DURATION)
            progressIndicatorContainer?.removeView(indicatorView)

            // If no more indicators, remove the container
            if (progressIndicators.isEmpty()) {
                cleanupProgressIndicatorContainer()
            }
        }
    }

    /**
     * Create the progress indicator container if it doesn't exist
     */
    private fun createProgressIndicatorContainer() {
        overlayContainer?.let { parent ->
            progressIndicatorContainer =
                LinearLayout(context).apply {
                    orientation = LinearLayout.VERTICAL
                    layoutParams =
                        FrameLayout
                            .LayoutParams(
                                ViewGroup.LayoutParams.WRAP_CONTENT,
                                ViewGroup.LayoutParams.WRAP_CONTENT,
                            ).apply {
                                gravity = Gravity.TOP or Gravity.END
                                topMargin = 16
                                rightMargin = 16
                            }
                }
            parent.addView(progressIndicatorContainer)
        }
    }

    /**
     * Clean up the progress indicator container when no indicators are active
     */
    private fun cleanupProgressIndicatorContainer() {
        progressIndicatorContainer?.let { container ->
            overlayContainer?.removeView(container)
            progressIndicatorContainer = null
        }
    }

    /**
     * Create progress indicator view
     */
    private fun createProgressIndicatorView(
        achievementId: Int,
        title: String,
        progress: String,
        progressPercent: Int,
        achievementImage: Bitmap?,
    ): View {
        val displayMetrics = context.resources.displayMetrics
        val screenWidth = displayMetrics.widthPixels
        val indicatorWidth = 180 // Smaller width for better stacking

        val container =
            FrameLayout(context).apply {
                layoutParams =
                    LinearLayout
                        .LayoutParams(
                            indicatorWidth,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            setMargins(0, 8, 0, 8) // Vertical spacing for stacking
                        }
            }

        val content =
            LinearLayout(context).apply {
                orientation = LinearLayout.VERTICAL
                layoutParams =
                    FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.WRAP_CONTENT,
                    )
                setPadding(8, 6, 8, 6) // Smaller padding

                // Background with rounded corners
                background =
                    GradientDrawable().apply {
                        setColor(Color.parseColor("#DD000000"))
                        cornerRadius = 8f // Smaller corner radius
                        setStroke(1, Color.parseColor("#FFD700")) // Thinner border
                    }
            }

        // Achievement icon - smaller
        val iconView =
            ImageView(context).apply {
                layoutParams =
                    LinearLayout.LayoutParams(32, 32).apply {
                        // Smaller icon
                        gravity = Gravity.CENTER_HORIZONTAL
                        bottomMargin = 4
                    }
                scaleType = ImageView.ScaleType.CENTER_CROP

                if (achievementImage != null) {
                    setImageBitmap(achievementImage)
                } else {
                    setImageResource(R.drawable.missing)
                }
            }

        // Achievement title - smaller text
        val titleView =
            TextView(context).apply {
                text = title
                textSize = 12f // Smaller text
                setTextColor(Color.WHITE)
                gravity = Gravity.CENTER
                maxLines = 2 // Allow 2 lines for longer titles
                typeface = Typeface.DEFAULT_BOLD
                layoutParams =
                    LinearLayout
                        .LayoutParams(
                            LinearLayout.LayoutParams.MATCH_PARENT,
                            LinearLayout.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            bottomMargin = 2
                        }
            }

        // Progress text - smaller
        val progressView =
            TextView(context).apply {
                text = progress
                textSize = 11f // Smaller text
                setTextColor(Color.parseColor("#FFD700"))
                gravity = Gravity.CENTER
                typeface = Typeface.MONOSPACE // Fixed-width for consistency
                layoutParams =
                    LinearLayout
                        .LayoutParams(
                            LinearLayout.LayoutParams.MATCH_PARENT,
                            LinearLayout.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            bottomMargin = 2
                        }
            }

        // Progress percentage - smaller
        val percentView =
            TextView(context).apply {
                text = "$progressPercent%"
                textSize = 10f // Smaller text
                setTextColor(Color.parseColor("#CCCCCC"))
                gravity = Gravity.CENTER
                layoutParams =
                    LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.MATCH_PARENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT,
                    )
            }

        content.addView(iconView)
        content.addView(titleView)
        content.addView(progressView)
        content.addView(percentView)
        container.addView(content)

        return container
    }

    /**
     * Update existing progress indicator content
     */
    private fun updateProgressIndicatorContent(
        view: View,
        achievementId: Int,
        title: String,
        progress: String,
        progressPercent: Int,
        achievementImage: Bitmap?,
    ) {
        val content = (view as ViewGroup).getChildAt(0) as LinearLayout

        // Update icon
        val iconView = content.getChildAt(0) as ImageView
        if (achievementImage != null) {
            iconView.setImageBitmap(achievementImage)
        }

        // Update title
        val titleView = content.getChildAt(1) as TextView
        titleView.text = title

        // Update progress
        val progressView = content.getChildAt(2) as TextView
        progressView.text = progress

        // Update percentage
        val percentView = content.getChildAt(3) as TextView
        percentView.text = "$progressPercent%"
    }

    // =================== Leaderboard Trackers ===================

/**
     * Show leaderboard tracker
     */
    fun showLeaderboardTracker(
        trackerId: Int,
        display: String,
    ) {
        overlayScope.launch {
            Log.d(TAG, "Showing leaderboard tracker: $trackerId - $display")

            hideLeaderboardTracker(trackerId) // Remove existing if present

            val trackerView = createLeaderboardTrackerView(trackerId, display)
            leaderboardTrackers[trackerId] = trackerView

            overlayContainer?.addView(trackerView)

            // Animate in
            trackerView.alpha = 0f
            trackerView.translationX = -trackerView.width.toFloat()

            val fadeIn = ObjectAnimator.ofFloat(trackerView, "alpha", 0f, 1f)
            val slideIn = ObjectAnimator.ofFloat(trackerView, "translationX", -trackerView.width.toFloat(), 0f)

            fadeIn.duration = ANIMATION_DURATION
            slideIn.duration = ANIMATION_DURATION

            fadeIn.start()
            slideIn.start()
        }
    }

    /**
     * Update leaderboard tracker display
     */
    fun updateLeaderboardTracker(
        trackerId: Int,
        display: String,
    ) {
        overlayScope.launch {
            Log.d(TAG, "Updating leaderboard tracker: $trackerId - $display")

            leaderboardTrackers[trackerId]?.let { view ->
                val textView = (view as ViewGroup).getChildAt(0) as TextView
                textView.text = display
            }
        }
    }

    /**
     * Hide leaderboard tracker
     */
    fun hideLeaderboardTracker(trackerId: Int) {
        overlayScope.launch {
            Log.d(TAG, "Hiding leaderboard tracker: $trackerId")

            leaderboardTrackers[trackerId]?.let { view ->
                // Animate out
                val fadeOut = ObjectAnimator.ofFloat(view, "alpha", 1f, 0f)
                val slideOut = ObjectAnimator.ofFloat(view, "translationX", 0f, -view.width.toFloat())

                fadeOut.duration = ANIMATION_DURATION
                slideOut.duration = ANIMATION_DURATION

                fadeOut.start()
                slideOut.start()

                // Remove from container after animation
                overlayScope.launch {
                    delay(ANIMATION_DURATION)
                    overlayContainer?.removeView(view)
                    leaderboardTrackers.remove(trackerId)
                }
            }
        }
    }

    /**
     * Hide all leaderboard trackers
     */
    private fun hideAllLeaderboardTrackers() {
        leaderboardTrackers.keys.forEach { trackerId ->
            hideLeaderboardTracker(trackerId)
        }
    }

    /**
     * Create leaderboard tracker view
     */
    private fun createLeaderboardTrackerView(
        trackerId: Int,
        display: String,
    ): View {
        val container =
            FrameLayout(context).apply {
                layoutParams =
                    FrameLayout
                        .LayoutParams(
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                        ).apply {
                            gravity = Gravity.TOP or Gravity.START
                            topMargin = 16 + (leaderboardTrackers.size * 40) // Stack vertically
                            leftMargin = 16
                        }
            }

        val textView =
            TextView(context).apply {
                text = display
                textSize = 12f
                setTextColor(Color.WHITE)
                typeface = Typeface.MONOSPACE // Fixed-width font
                setPadding(12, 8, 12, 8)

                // Background
                background =
                    GradientDrawable().apply {
                        setColor(Color.parseColor("#CC000000"))
                        cornerRadius = 8f
                        setStroke(1, Color.parseColor("#FFD700"))
                    }
            }

        container.addView(textView)
        return container
    }

    // =================== Challenge Indicators ===================

/**
     * Show challenge indicator
     */
    fun showChallengeIndicator(
        achievementId: Int,
        title: String,
        imageUrl: String,
    ) {
        overlayScope.launch {
            Log.d(TAG, "Showing challenge indicator: $achievementId - $title")

            hideChallengeIndicator(achievementId) // Remove existing if present

            // Download image if needed
            val achievementImage =
                if (imageUrl.isNotEmpty()) {
                    downloadImageAsync(imageUrl)
                } else {
                    null
                }

            val indicatorView = createChallengeIndicatorView(achievementId, title, achievementImage)
            challengeIndicators[achievementId] = indicatorView

            overlayContainer?.addView(indicatorView)

            // Animate in
            indicatorView.alpha = 0f
            indicatorView.scaleX = 0.5f
            indicatorView.scaleY = 0.5f

            val fadeIn = ObjectAnimator.ofFloat(indicatorView, "alpha", 0f, 1f)
            val scaleInX = ObjectAnimator.ofFloat(indicatorView, "scaleX", 0.5f, 1f)
            val scaleInY = ObjectAnimator.ofFloat(indicatorView, "scaleY", 0.5f, 1f)

            fadeIn.duration = ANIMATION_DURATION
            scaleInX.duration = ANIMATION_DURATION
            scaleInY.duration = ANIMATION_DURATION

            fadeIn.start()
            scaleInX.start()
            scaleInY.start()
        }
    }

    /**
     * Hide challenge indicator
     */
    fun hideChallengeIndicator(achievementId: Int) {
        overlayScope.launch {
            Log.d(TAG, "Hiding challenge indicator: $achievementId")

            challengeIndicators[achievementId]?.let { view ->
                // Animate out
                val fadeOut = ObjectAnimator.ofFloat(view, "alpha", 1f, 0f)
                val scaleOutX = ObjectAnimator.ofFloat(view, "scaleX", 1f, 0.5f)
                val scaleOutY = ObjectAnimator.ofFloat(view, "scaleY", 1f, 0.5f)

                fadeOut.duration = ANIMATION_DURATION
                scaleOutX.duration = ANIMATION_DURATION
                scaleOutY.duration = ANIMATION_DURATION

                fadeOut.start()
                scaleOutX.start()
                scaleOutY.start()

                // Remove from container after animation
                overlayScope.launch {
                    delay(ANIMATION_DURATION)
                    overlayContainer?.removeView(view)
                    challengeIndicators.remove(achievementId)
                }
            }
        }
    }

    /**
     * Hide all challenge indicators
     */
    private fun hideAllChallengeIndicators() {
        challengeIndicators.keys.forEach { achievementId ->
            hideChallengeIndicator(achievementId)
        }
    }

    /**
     * Create challenge indicator view
     */
    private fun createChallengeIndicatorView(
        achievementId: Int,
        title: String,
        achievementImage: Bitmap?,
    ): View {
        // Calculate top margin to avoid overlapping with progress indicators
        val progressIndicatorHeight =
            if (progressIndicators.isNotEmpty()) {
                // Estimate height: each indicator ~80px + 16px margin
                progressIndicators.size * 96 + 32
            } else {
                16
            }

        val container =
            FrameLayout(context).apply {
                layoutParams =
                    FrameLayout.LayoutParams(56, 56).apply {
                        gravity = Gravity.TOP or Gravity.END
                        topMargin = progressIndicatorHeight + (challengeIndicators.size * 64) // Stack below progress indicators
                        rightMargin = 16
                    }
            }

        val iconView =
            ImageView(context).apply {
                layoutParams =
                    FrameLayout.LayoutParams(48, 48).apply {
                        gravity = Gravity.CENTER
                    }
                scaleType = ImageView.ScaleType.CENTER_CROP

                if (achievementImage != null) {
                    setImageBitmap(achievementImage)
                } else {
                    setImageResource(R.drawable.missing)
                }

                // Background circle
                background =
                    GradientDrawable().apply {
                        shape = GradientDrawable.OVAL
                        setColor(Color.parseColor("#DD000000"))
                        setStroke(2, Color.parseColor("#FFD700"))
                    }
            }

        container.addView(iconView)
        return container
    }

    // =================== Helper Methods ===================

/**
     * Download image asynchronously
     */
    private suspend fun downloadImageAsync(imageUrl: String): Bitmap? =
        withContext(Dispatchers.IO) {
            try {
                // Check cache first
                val cacheDir = File(context.cacheDir, "retroachievements")
                if (!cacheDir.exists()) cacheDir.mkdirs()

                val filename = imageUrl.substringAfterLast('/').substringBefore('?')
                val cacheFile = File(cacheDir, filename)

                if (cacheFile.exists()) {
                    return@withContext BitmapFactory.decodeFile(cacheFile.absolutePath)
                }

                // Download if not cached
                val request = Request.Builder().url(imageUrl).build()
                val response = httpClient.newCall(request).execute()

                if (response.isSuccessful) {
                    response.body?.byteStream()?.use { inputStream ->
                        FileOutputStream(cacheFile).use { outputStream ->
                            inputStream.copyTo(outputStream)
                        }
                    }
                    return@withContext BitmapFactory.decodeFile(cacheFile.absolutePath)
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error downloading image: $imageUrl", e)
            }
            return@withContext null
        }
}
