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

import android.app.Notification
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.UiModeManager
import android.content.Intent
import android.content.res.Configuration
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.media.RingtoneManager
import android.net.Uri
import android.util.Log
import androidx.core.app.NotificationCompat
import com.google.firebase.messaging.FirebaseMessagingService
import com.google.firebase.messaging.RemoteMessage
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.phone.GameSelectActivityPhone
import org.uoyabause.android.tv.GameSelectActivity
import java.lang.Exception
import java.net.URL

/**
 * Created by devMiyax on 2016/05/28.
 */
@OptIn(DelicateCoroutinesApi::class)
class Notification : FirebaseMessagingService() {
    override fun onNewToken(token: String) {
        super.onNewToken(token)
        Log.d(TAG, "New FCM token generated: ${token.take(20)}...")

        // トークンをFirestoreに保存（Admin権限があれば）
        GlobalScope.launch(Dispatchers.IO) {
            try {
                FCMTokenManager(applicationContext).registerFcmToken()
            } catch (e: Exception) {
                Log.e(TAG, "Failed to register new FCM token", e)
            }
        }
    }

    fun showVersionUpNOtification(remoteMessage: RemoteMessage) {
        // https://play.google.com/store/apps/details?id=org.uoyabause.android
        val `val` = remoteMessage.data
        val googlePlayIntent = Intent(Intent.ACTION_VIEW)
        googlePlayIntent.data = Uri.parse("market://details?id=org.uoyabause.uranus")
        val pendingIntent =
            PendingIntent.getActivity(
                this,
                0,
                googlePlayIntent,
                PendingIntent.FLAG_ONE_SHOT or PendingIntent.FLAG_IMMUTABLE,
            )
        val message = `val`["message"]
        val defaultSoundUri = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_NOTIFICATION)
        val mBuilder =
            NotificationCompat
                .Builder(this, CHANNEL_ID)
                .setSmallIcon(R.drawable.ic_stat_ss_one)
                .setContentTitle(getString(R.string.new_version_available))
                .setStyle(NotificationCompat.BigTextStyle().bigText(message))
                .setContentText(message)
                .setSound(defaultSoundUri)
                .setContentIntent(pendingIntent)
                .setAutoCancel(false) // .setPriority(android.app.Notification.PRIORITY_MAX)
                .addAction(android.R.drawable.ic_media_play, "Install", pendingIntent)
        val uiModeManager = getSystemService(UI_MODE_SERVICE) as UiModeManager
        var notification =
            if (uiModeManager.currentModeType == Configuration.UI_MODE_TYPE_TELEVISION) {
                val r = resources
                val image = BitmapFactory.decodeResource(r, R.drawable.banner)
                mBuilder
                    .setCategory(Notification.CATEGORY_RECOMMENDATION)
                    .setLargeIcon(image)
                    .setLocalOnly(true)
                    .setOngoing(true)
                NotificationCompat.BigPictureStyle(mBuilder).build()
            } else {
                mBuilder.build()
            }
        val notificationManager = getSystemService(NOTIFICATION_SERVICE) as NotificationManager
        notificationManager.notify(0 /* ID of notification */, notification)
    }

    override fun onMessageReceived(remoteMessage: RemoteMessage) {
        super.onMessageReceived(remoteMessage)
        Log.d(TAG, "From: " + remoteMessage.from)
        // Log.d(TAG, "Notification Message Body: " + remoteMessage.getNotification().getBody());
        val `val` = remoteMessage.data
        var sentversion = `val`["version"]
        // Version up Information
        if (sentversion != null) {
            showVersionUpNOtification(remoteMessage)
            return
        }

        // Check for admin report notification
        val type = `val`["type"]
        if (type == "new_report") {
            showAdminReportNotification(remoteMessage)
            return
        }

        // Release announcement sent by the release skill (topic message).
        // This path only runs in foreground; background notification-messages
        // are shown by the system tray automatically.
        if (type == "release_notification") {
            showReleaseNotification(remoteMessage)
            return
        }

        val intent = Intent(this, GameSelectActivity::class.java)
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP)
        val pendingIntent =
            PendingIntent.getActivity(
                this,
                0 /* Request code */,
                intent,
                PendingIntent.FLAG_ONE_SHOT or PendingIntent.FLAG_IMMUTABLE,
            )
        val defaultSoundUri = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_NOTIFICATION)
        val mBuilder =
            NotificationCompat
                .Builder(this, CHANNEL_ID)
                .setSmallIcon(R.drawable.ic_stat_ss_one)
                .setContentTitle("uoYabause")
                .setContentText(remoteMessage.notification!!.body)
                .setAutoCancel(true)
                .setSound(defaultSoundUri)
                .setContentIntent(pendingIntent)
        val uiModeManager = getSystemService(UI_MODE_SERVICE) as UiModeManager
        var notification =
            if (uiModeManager.currentModeType == Configuration.UI_MODE_TYPE_TELEVISION) {
                val r = resources
                val image = BitmapFactory.decodeResource(r, R.drawable.banner)
                mBuilder
                    .setCategory("recommendation")
                    .setLargeIcon(image)
                    .setLocalOnly(true)
                    .setCategory(Notification.CATEGORY_RECOMMENDATION)
                    .setOngoing(true)
                NotificationCompat.BigPictureStyle(mBuilder).build()
            } else {
                mBuilder.build()
            }
        val notificationManager = getSystemService(NOTIFICATION_SERVICE) as NotificationManager
        notificationManager.notify(0 /* ID of notification */, notification)
    }

    private fun showReleaseNotification(remoteMessage: RemoteMessage) {
        // Android 13+ requires POST_NOTIFICATIONS to display notifications
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.TIRAMISU) {
            val permission = android.Manifest.permission.POST_NOTIFICATIONS
            if (checkSelfPermission(permission) != android.content.pm.PackageManager.PERMISSION_GRANTED) {
                Log.e(TAG, "POST_NOTIFICATIONS permission not granted, cannot show release notification")
                return
            }
        }

        val title = remoteMessage.notification?.title ?: getString(R.string.new_version_available)
        val body = remoteMessage.notification?.body ?: ""

        // Debug builds (.debug suffix) share the store page of the release package
        val storePackage = packageName.removeSuffix(".debug")
        val storeIntent = Intent(Intent.ACTION_VIEW, Uri.parse("market://details?id=$storePackage"))
        val pendingIntent =
            PendingIntent.getActivity(
                this,
                0,
                storeIntent,
                PendingIntent.FLAG_ONE_SHOT or PendingIntent.FLAG_IMMUTABLE,
            )
        val defaultSoundUri = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_NOTIFICATION)
        val builder =
            NotificationCompat
                .Builder(this, YabauseApplication.CHANNEL_ID_RELEASE)
                .setSmallIcon(R.drawable.ic_stat_ss_one)
                .setContentTitle(title)
                .setContentText(body)
                .setStyle(NotificationCompat.BigTextStyle().bigText(body))
                .setAutoCancel(true)
                .setSound(defaultSoundUri)
                .setContentIntent(pendingIntent)
        val notificationManager = getSystemService(NOTIFICATION_SERVICE) as NotificationManager
        notificationManager.notify(RELEASE_NOTIFICATION_ID, builder.build())
    }

    private fun showAdminReportNotification(remoteMessage: RemoteMessage) {
        val data = remoteMessage.data
        val title = data["title"] ?: "新しいレポート / New Report"
        val body = data["body"] ?: ""
        val productNumber = data["product_number"] ?: "Unknown Game"
        val reporterName = data["reporter_name"] ?: "Anonymous User"
        val gameId = data["game_id"] ?: ""
        val ratingId = data["rating_id"] ?: ""
        val comment = data["comment"] ?: ""
        val screenshotUrl = data["screenshot_url"] ?: ""
        val emulationRating = data["emulation_rating"] ?: "0"
        val gameRating = data["game_rating"] ?: "0"

        Log.d(TAG, "Received admin report notification: $title from $reporterName")
        Log.d(TAG, "gameId: $gameId, ratingId: $ratingId")
        Log.d(TAG, "comment: $comment, screenshotUrl: $screenshotUrl")
        Log.d(TAG, "Data-only notification: title=$title")

        // Check notification permission (Android 13+)
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.TIRAMISU) {
            val permission = android.Manifest.permission.POST_NOTIFICATIONS
            if (checkSelfPermission(permission) != android.content.pm.PackageManager.PERMISSION_GRANTED) {
                Log.e(TAG, "POST_NOTIFICATIONS permission not granted, cannot show notification")
                return
            }
            Log.d(TAG, "POST_NOTIFICATIONS permission granted")
        }

        // Create intent to open GameSelectActivityPhone first, then navigate to ReportListActivity
        // This ensures we can find the game's file path from product_number
        val intent =
            Intent(this, GameSelectActivityPhone::class.java).apply {
                addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP)
                putExtra(EXTRA_OPEN_REPORT_LIST, true)
                putExtra(EXTRA_PRODUCT_NUMBER, productNumber)
                putExtra(EXTRA_GAME_ID, gameId)
                putExtra(EXTRA_GAME_TITLE, title)
            }

        val pendingIntent =
            PendingIntent.getActivity(
                this,
                System.currentTimeMillis().toInt(), // Unique request code
                intent,
                PendingIntent.FLAG_ONE_SHOT or PendingIntent.FLAG_IMMUTABLE,
            )

        val defaultSoundUri = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_NOTIFICATION)

        Log.d(TAG, "Building notification with channel: admin_reports")

        // Build notification text with ratings and comment
        val notificationText =
            buildString {
                append(reporterName)
                append("\n評価: $gameRating/5 | エミュ: $emulationRating/5")
                if (comment.isNotEmpty()) {
                    append("\n💬 $comment")
                }
            }

        val notificationBuilder =
            NotificationCompat
                .Builder(this, "admin_reports")
                .setSmallIcon(R.drawable.ic_stat_ss_one)
                .setContentTitle(title)
                .setContentText(notificationText)
                .setStyle(NotificationCompat.BigTextStyle().bigText(notificationText))
                .setAutoCancel(true)
                .setSound(defaultSoundUri)
                .setPriority(NotificationCompat.PRIORITY_HIGH)
                .setContentIntent(pendingIntent)

        // Load screenshot asynchronously if available
        if (screenshotUrl.isNotEmpty()) {
            GlobalScope.launch(Dispatchers.IO) {
                try {
                    val bitmap = loadBitmapFromUrl(screenshotUrl)
                    if (bitmap != null) {
                        withContext(Dispatchers.Main) {
                            // Update notification with screenshot
                            val bigPictureStyle =
                                NotificationCompat
                                    .BigPictureStyle()
                                    .bigPicture(bitmap)
                                    .bigLargeIcon(null as Bitmap?) // Hide large icon when expanded
                                    .setBigContentTitle(title)
                                    .setSummaryText(reporterName)

                            notificationBuilder
                                .setStyle(bigPictureStyle)
                                .setLargeIcon(bitmap)

                            val notificationManager = getSystemService(NOTIFICATION_SERVICE) as NotificationManager
                            notificationManager.notify(ratingId.hashCode(), notificationBuilder.build())
                            Log.d(TAG, "Notification updated with screenshot")
                        }
                    } else {
                        // Show notification without screenshot
                        showNotificationWithoutScreenshot(notificationBuilder, ratingId)
                    }
                } catch (e: Exception) {
                    Log.e(TAG, "Failed to load screenshot, showing notification without image", e)
                    showNotificationWithoutScreenshot(notificationBuilder, ratingId)
                }
            }
        } else {
            // No screenshot, show notification immediately
            showNotificationWithoutScreenshot(notificationBuilder, ratingId)
        }
    }

    private fun showNotificationWithoutScreenshot(
        notificationBuilder: NotificationCompat.Builder,
        ratingId: String,
    ) {
        val notificationManager = getSystemService(NOTIFICATION_SERVICE) as NotificationManager

        // Check if channel exists (Android 8.0+)
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
            val channel = notificationManager.getNotificationChannel("admin_reports")
            if (channel == null) {
                Log.e(TAG, "Notification channel 'admin_reports' does not exist!")
            } else {
                Log.d(TAG, "Notification channel exists: ${channel.name}, importance: ${channel.importance}")
            }
        }

        val notificationId = ratingId.hashCode()
        Log.d(TAG, "Attempting to notify with ID: $notificationId")

        try {
            notificationManager.notify(notificationId, notificationBuilder.build())
            Log.d(TAG, "Notification sent successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to show notification", e)
        }
    }

    private fun loadBitmapFromUrl(urlString: String): Bitmap? = try {
        val url = URL(urlString)
        val connection = url.openConnection()
        connection.connectTimeout = 5000 // 5 seconds timeout
        connection.readTimeout = 5000
        connection.connect()
        val input = connection.getInputStream()
        BitmapFactory.decodeStream(input)
    } catch (e: Exception) {
        Log.e(TAG, "Error loading bitmap from URL: $urlString", e)
        null
    }

    companion object {
        private const val TAG = "uoyabause.Notification"
        private const val CHANNEL_ID = "admin_reports"
        private const val RELEASE_NOTIFICATION_ID = 3001
        const val EXTRA_OPEN_REPORT_LIST = "extra_open_report_list"
        const val EXTRA_PRODUCT_NUMBER = "extra_product_number"
        const val EXTRA_GAME_ID = "extra_game_id"
        const val EXTRA_GAME_TITLE = "extra_game_title"
    }
}
