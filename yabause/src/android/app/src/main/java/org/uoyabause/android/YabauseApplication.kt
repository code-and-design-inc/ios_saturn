/*  Copyright 2019 devMiyax(smiyaxdev@gmail.com)

    This file is part of YabaSanshiro.

    YabaSanshiro is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    YabaSanshiro is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with YabaSanshiro; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/
package org.uoyabause.android

import android.app.Activity
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.widget.TextView
import androidx.appcompat.app.AppCompatDelegate
import androidx.core.content.pm.PackageInfoCompat
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.lifecycleScope
import androidx.multidex.MultiDex
import androidx.multidex.MultiDexApplication
import com.android.billingclient.api.BillingFlowParams
import com.google.android.gms.analytics.GoogleAnalytics
import com.google.android.gms.analytics.Tracker
import com.google.android.material.button.MaterialButton
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.firebase.FirebaseApp
import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.messaging.FirebaseMessaging
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeoutOrNull
import org.devmiyax.yabasanshiro.BuildConfig
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.auth.RetroAchievementsAuthManager
import org.uoyabause.android.billing.BillingClientWrapper
import org.uoyabause.android.storage.PreferencesManager

class YabauseApplication : MultiDexApplication() {
    private var mTracker: Tracker? = null
    val tag = "YabauseApplication"

    private val applicationScope = CoroutineScope(SupervisorJob() + Dispatchers.Main)

    private val authStateListener =
        FirebaseAuth.AuthStateListener { firebaseAuth ->
            val user = firebaseAuth.currentUser
            if (user != null && !user.isAnonymous) {
                Log.d(tag, "User logged in: ${user.uid}")
                // Register FCM token when user logs in
                initializeFcmToken()
            } else {
                Log.d(tag, "User logged out (or anonymous report session)")
            }
        }

    override fun attachBaseContext(base: Context) {
        super.attachBaseContext(base)
        MultiDex.install(this)
    }

    override fun onCreate() {
        super.onCreate()
        appContext = applicationContext

        AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_YES)

        // Perform version migration before any other initialization
        PreferencesManager.performVersionMigrationIfNeeded(appContext, getVersionCode())

        GameInfo.initSigin(appContext)

        FirebaseApp.initializeApp(applicationContext)

        // Initialize RetroAchievements authentication at app startup
        // This will handle auto-login if enabled, without showing notifications
        initializeRetroAchievements()

        // Setup Firebase Auth state listener to register FCM token on login
        FirebaseAuth.getInstance().addAuthStateListener(authStateListener)

        // Initialize FCM token registration for admin notifications
        initializeFcmToken()

        // Create notification channel for admin report notifications
        createNotificationChannel()

        // Subscribe to the release-announcement FCM topic
        subscribeToReleaseTopic()

        // Log.d(tag,"Firebase token: " + FirebaseInstanceId.getInstance().getToken() );
    }

    /**
     * Initialize RetroAchievements authentication at application startup
     * This performs early initialization and auto-login without notifications
     */
    private fun initializeRetroAchievements() {
        try {
            Log.d(tag, "Initializing RetroAchievements at application startup...")

            // Get authentication manager instance (creates if needed)
            val authManager = RetroAchievementsAuthManager.getInstance(appContext)

            // Initialize authentication manager which will handle auto-login if enabled
            // Note: This initialization is lightweight and doesn't show login notifications
            authManager.initialize()

            Log.d(tag, "RetroAchievements authentication initialized at application level")
        } catch (e: Exception) {
            Log.e(tag, "Failed to initialize RetroAchievements at application startup", e)
        }
    }

    /**
     * Initialize FCM token registration for admin notifications
     * Registers FCM token to Firestore if the current user is an admin
     */
    private fun initializeFcmToken() {
        applicationScope.launch(Dispatchers.IO) {
            try {
                Log.d(tag, "Initializing FCM token registration...")
                FCMTokenManager(applicationContext).registerFcmToken()
                Log.d(tag, "FCM token registration completed")
            } catch (e: Exception) {
                Log.e(tag, "Failed to register FCM token", e)
            }
        }
    }

    /**
     * Create notification channel for admin report notifications (Android 8.0+)
     */
    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            try {
                val channelId = "admin_reports"
                val channelName = getString(R.string.notification_channel_admin_reports)
                val channelDescription = getString(R.string.notification_channel_admin_reports_desc)
                val importance = NotificationManager.IMPORTANCE_HIGH

                val channel =
                    NotificationChannel(channelId, channelName, importance).apply {
                        description = channelDescription
                        enableLights(true)
                        enableVibration(true)
                    }

                val notificationManager = getSystemService(NotificationManager::class.java)
                notificationManager?.createNotificationChannel(channel)

                val releaseChannel =
                    NotificationChannel(
                        CHANNEL_ID_RELEASE,
                        getString(R.string.notification_channel_release),
                        NotificationManager.IMPORTANCE_DEFAULT,
                    ).apply {
                        description = getString(R.string.notification_channel_release_desc)
                    }
                notificationManager?.createNotificationChannel(releaseChannel)

                Log.d(tag, "Notification channel created: $channelId")
            } catch (e: Exception) {
                Log.e(tag, "Failed to create notification channel", e)
            }
        }
    }

    /**
     * Subscribe to the release-announcement FCM topic.
     * Topic subscription is server-side state and does not require the
     * POST_NOTIFICATIONS permission; the FCM SDK retries on failure.
     */
    private fun subscribeToReleaseTopic() {
        FirebaseMessaging
            .getInstance()
            .subscribeToTopic(FCM_TOPIC_GAME_UPDATES)
            .addOnCompleteListener { task ->
                if (task.isSuccessful) {
                    Log.d(tag, "Subscribed to FCM topic: $FCM_TOPIC_GAME_UPDATES")
                } else {
                    Log.e(tag, "Failed to subscribe to FCM topic: $FCM_TOPIC_GAME_UPDATES", task.exception)
                }
            }
    } // To enable debug logging use: adb shell setprop log.tag.GAv4 DEBUG

/**
     * Gets the default [Tracker] for this [Application].
     * @return tracker
     */
    @get:Synchronized
    val defaultTracker: Tracker?
        get() {
            if (mTracker == null) {
                val analytics = GoogleAnalytics.getInstance(this)
                // To enable debug logging use: adb shell setprop log.tag.GAv4 DEBUG
                mTracker = analytics.newTracker(R.xml.global_tracker)
                mTracker!!.enableAdvertisingIdCollection(true)
            }
            return mTracker
        }

    companion object {
        lateinit var appContext: Context
            private set

        private const val PREF_SUBSCRIBED = "subscribed"

        // FCM topic for release announcements; must match
        // notify-firebase-push.py DEFAULT_TOPIC and iOS NotificationCategory.gameUpdates.
        const val FCM_TOPIC_GAME_UPDATES = "yabasanshiro_game_updates"

        // Notification channel for release announcements; must match
        // notify-firebase-push.py ANDROID_CHANNEL_ID.
        const val CHANNEL_ID_RELEASE = "release_notifications"

        // Runtime flag: true when remote config disables subscription checking
        // (all users treated as Pro without persisting to SharedPreferences)
        @Volatile
        var isSubscriptionDisabledByRemoteConfig = false

        fun isPro(): Boolean {
            if (isSubscriptionDisabledByRemoteConfig) {
                return true
            }
            if (BuildConfig.DEBUG) {
                return true // Debug ビルドは Pro 権限で動作する
            }
            val prefs: SharedPreferences? =
                appContext.getSharedPreferences(
                    "private",
                    Context.MODE_PRIVATE,
                )
            var hasDonated = false
            var hasSubscription = false
            if (prefs != null) {
                hasDonated = prefs.getBoolean("donated", false)
                hasSubscription = prefs.getBoolean(PREF_SUBSCRIBED, false)
            }
            if (BuildConfig.BUILD_TYPE == "pro" || hasDonated || hasSubscription) {
                return true
            }
            return false
        }

        fun setSubscriptionState(active: Boolean) {
            Log.i(TAG, "setSubscriptionState: $active")
            val prefs = appContext.getSharedPreferences("private", Context.MODE_PRIVATE)
            prefs.edit().putBoolean(PREF_SUBSCRIBED, active).apply()
        }

        private const val TAG = "YabauseApplication"
        private const val BILLING_TIMEOUT_MS = 10_000L

        /**
         * Launches the premium subscription purchase flow.
         * Creates a temporary BillingClientWrapper, queries the product details,
         * and launches the Google Play billing flow.
         */
        fun launchProSubscription(activity: Activity) {
            val lifecycleOwner = activity as? LifecycleOwner ?: return
            val billingClient = BillingClientWrapper(activity)
            val billingConnectionState = MutableLiveData(false)
            billingClient.startBillingConnection(billingConnectionState)
            billingConnectionState.observe(lifecycleOwner) { connected ->
                if (connected) {
                    lifecycleOwner.lifecycleScope.launch {
                        try {
                            val productMap = withTimeoutOrNull(BILLING_TIMEOUT_MS) {
                                billingClient.productWithProductDetails
                                    .first { it.containsKey(BillingClientWrapper.PRO_ANNUAL_SUB) }
                            }
                            if (productMap == null) {
                                Log.e(TAG, "Timeout: could not fetch product details for premium")
                                billingClient.terminateBillingConnection()
                                return@launch
                            }
                            val productDetails = productMap[BillingClientWrapper.PRO_ANNUAL_SUB]!!
                            val offerDetails = productDetails.subscriptionOfferDetails
                            if (!offerDetails.isNullOrEmpty()) {
                                val offerToken = offerDetails[0].offerToken
                                val billingParams = BillingFlowParams
                                    .newBuilder()
                                    .setProductDetailsParamsList(
                                        listOf(
                                            BillingFlowParams.ProductDetailsParams
                                                .newBuilder()
                                                .setProductDetails(productDetails)
                                                .setOfferToken(offerToken)
                                                .build(),
                                        ),
                                    ).build()
                                billingClient.launchBillingFlow(activity, billingParams)
                            } else {
                                Log.e(TAG, "No offer details available for premium")
                                billingClient.terminateBillingConnection()
                            }
                        } catch (e: Exception) {
                            Log.e(TAG, "Error launching subscription flow", e)
                            billingClient.terminateBillingConnection()
                        }
                    }
                }
            }
        }

        /**
         * Callback for when the user chooses the subscription option in the Pro gate dialog.
         */
        interface OnSubscribeCallback {
            fun onSubscribe()
        }

        fun checkDonated(
            ctx: Context,
            additionalMessage: String = "",
            onSubscribe: OnSubscribeCallback? = null,
        ): Int {
            var rtn = -1
            if (BuildConfig.BUILD_TYPE != "pro" && BuildConfig.BUILD_TYPE != "debug") {
                val prefs = ctx.getSharedPreferences("private", MODE_PRIVATE)
                val hasDonated = prefs.getBoolean("donated", false)
                val hasSubscription = prefs.getBoolean(PREF_SUBSCRIBED, false)
                if (!hasDonated && !hasSubscription) {
                    // If subscription is disabled by remote config, show dialog without subscription option
                    if (isSubscriptionDisabledByRemoteConfig) {
                        showProGateDialog(ctx, null, additionalMessage, onSubscribe)
                        return rtn
                    }
                    val activity = ctx as? Activity
                    val lifecycleOwner = activity as? LifecycleOwner
                    if (activity == null || lifecycleOwner == null) {
                        showProGateDialog(ctx, null, additionalMessage, onSubscribe)
                        return rtn
                    }
                    val billingClient = BillingClientWrapper(ctx)
                    val billingConnectionState = MutableLiveData(false)
                    billingClient.startBillingConnection(billingConnectionState)
                    billingConnectionState.observe(lifecycleOwner) { connected ->
                        if (connected) {
                            lifecycleOwner.lifecycleScope.launch {
                                try {
                                    val productMap = withTimeoutOrNull(BILLING_TIMEOUT_MS) {
                                        billingClient.productWithProductDetails
                                            .first { it.containsKey(BillingClientWrapper.PRO_ANNUAL_SUB) }
                                    }
                                    val formattedPrice = productMap
                                        ?.get(BillingClientWrapper.PRO_ANNUAL_SUB)
                                        ?.subscriptionOfferDetails
                                        ?.firstOrNull()
                                        ?.pricingPhases
                                        ?.pricingPhaseList
                                        ?.firstOrNull()
                                        ?.formattedPrice
                                    billingClient.terminateBillingConnection()
                                    showProGateDialog(ctx, formattedPrice, additionalMessage, onSubscribe)
                                } catch (e: Exception) {
                                    Log.e(TAG, "Error fetching subscription price", e)
                                    billingClient.terminateBillingConnection()
                                    showProGateDialog(ctx, null, additionalMessage, onSubscribe)
                                }
                            }
                        }
                    }
                    return rtn
                }
            }
            return 0
        }

        private fun showProGateDialog(
            ctx: Context,
            formattedPrice: String?,
            additionalMessage: String,
            onSubscribe: OnSubscribeCallback?,
        ) {
            val dialogView = LayoutInflater.from(ctx).inflate(R.layout.dialog_pro_gate, null)

            val tvAdditionalMessage = dialogView.findViewById<TextView>(R.id.tv_additional_message)
            val btnSubscribe = dialogView.findViewById<MaterialButton>(R.id.btn_subscribe)
            val btnBuyPro = dialogView.findViewById<MaterialButton>(R.id.btn_buy_pro)
            val btnCancel = dialogView.findViewById<MaterialButton>(R.id.btn_cancel)

            // Show additional message if provided
            if (additionalMessage.isNotEmpty()) {
                tvAdditionalMessage.text = additionalMessage
                tvAdditionalMessage.visibility = View.VISIBLE
            }

            // Configure subscribe button
            if (isSubscriptionDisabledByRemoteConfig) {
                btnSubscribe.visibility = View.GONE
                // Adjust D-pad focus chain: Buy Pro becomes the primary focus target
                btnBuyPro.nextFocusUpId = R.id.btn_buy_pro
                btnCancel.nextFocusDownId = R.id.btn_cancel
            } else {
                if (formattedPrice != null) {
                    btnSubscribe.text =
                        ctx.getString(R.string.pro_gate_subscribe_with_price, formattedPrice)
                }
            }

            val dialog = MaterialAlertDialogBuilder(ctx)
                .setView(dialogView)
                .create()

            btnSubscribe.setOnClickListener {
                dialog.dismiss()
                if (onSubscribe != null) {
                    onSubscribe.onSubscribe()
                } else if (ctx is Activity) {
                    launchProSubscription(ctx)
                }
            }

            btnBuyPro.setOnClickListener {
                dialog.dismiss()
                val url =
                    "https://play.google.com/store/apps/details?id=org.devmiyax.yabasanshioro2.pro"
                val intent = Intent(Intent.ACTION_VIEW)
                intent.data = Uri.parse(url)
                intent.setPackage("com.android.vending")
                ctx.startActivity(intent)
            }

            btnCancel.setOnClickListener {
                dialog.dismiss()
            }

            // Gamepad/D-pad focus: set initial focus on primary CTA after dialog is shown
            dialog.setOnShowListener {
                val focusTarget = if (isSubscriptionDisabledByRemoteConfig) btnBuyPro else btnSubscribe
                focusTarget.post {
                    focusTarget.isFocusable = true
                    focusTarget.isFocusableInTouchMode = true
                    focusTarget.requestFocus()
                }
            }

            dialog.show()
        }

        fun getVersionName(): String? {
            val pm = appContext.packageManager
            var versionName = ""
            try {
                val packageInfo = pm.getPackageInfo(appContext.packageName, 0)
                versionName = packageInfo.versionName ?: ""
            } catch (e: PackageManager.NameNotFoundException) {
                e.printStackTrace()
            }
            return versionName
        }

        fun getVersionCode(): Int {
            val pm = appContext.packageManager
            var versionCode = 0
            try {
                val packageInfo = pm.getPackageInfo(appContext.packageName, 0)
                versionCode = PackageInfoCompat.getLongVersionCode(packageInfo).toInt()
            } catch (e: PackageManager.NameNotFoundException) {
                e.printStackTrace()
            }
            return versionCode
        }
    }
}
