package org.uoyabause.android

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.util.Log
import androidx.core.content.ContextCompat
import com.google.android.gms.tasks.Tasks
import com.google.firebase.firestore.FirebaseFirestore
import com.google.firebase.messaging.FirebaseMessaging
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.uoyabause.android.auth.AuthState
import java.util.Date

/**
 * FCMトークン管理クラス
 * Firebase Cloud Messagingのデバイストークンを取得し、
 * Adminユーザーの場合にFirestoreに保存する
 */
class FCMTokenManager(
    private val context: Context,
) {
    companion object {
        private const val TAG = "FCMTokenManager"
    }

    private val firestore: FirebaseFirestore = FirebaseFirestore.getInstance()

    /**
     * FCMトークンを取得し、Admin権限があればFirestoreに保存
     */
    suspend fun registerFcmToken() {
        withContext(Dispatchers.IO) {
            try {
                // 1. Login check - anonymous sessions must NOT register an FCM token
                val currentUser = AuthState.realUser()
                if (currentUser == null) {
                    Log.d(TAG, "User not logged in, skipping FCM token registration")
                    return@withContext
                }

                // 2. Admin権限チェック
                val isAdmin = checkIsAdmin(currentUser.uid)
                if (!isAdmin) {
                    Log.d(TAG, "User is not admin, skipping FCM token registration")
                    return@withContext
                }

                // 3. 通知権限チェック（Android 13+）
                if (!hasNotificationPermission()) {
                    Log.d(TAG, "Notification permission not granted, skipping FCM token registration")
                    return@withContext
                }

                // 4. FCMトークン取得
                val token = Tasks.await(FirebaseMessaging.getInstance().token)
                Log.d(TAG, "FCM Token obtained: ${token.take(20)}...")

                // 5. Firestoreに保存
                saveFcmTokenToFirestore(token, currentUser.uid, isAdmin)
            } catch (e: Exception) {
                Log.e(TAG, "Failed to register FCM token", e)
            }
        }
    }

    /**
     * 通知権限があるかチェック（Android 13以降）
     */
    fun hasNotificationPermission(): Boolean = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        ContextCompat.checkSelfPermission(
            context,
            Manifest.permission.POST_NOTIFICATIONS,
        ) == PackageManager.PERMISSION_GRANTED
    } else {
        // Android 12以前は権限不要
        true
    }

    /**
     * 通知権限が必要かチェック（Android 13以降かつAdmin権限あり）
     */
    suspend fun shouldRequestNotificationPermission(): Boolean {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            return false
        }

        if (hasNotificationPermission()) {
            return false
        }

        // Adminユーザーのみ権限をリクエスト
        val currentUser = AuthState.realUser() ?: return false
        return checkIsAdmin(currentUser.uid)
    }

    /**
     * FCMトークンをFirestoreに保存
     */
    private suspend fun saveFcmTokenToFirestore(
        token: String,
        uid: String,
        isAdmin: Boolean,
    ) {
        withContext(Dispatchers.IO) {
            try {
                if (!isAdmin) return@withContext

                val tokenData =
                    hashMapOf(
                        "token" to token,
                        "createdAt" to Date(),
                        "updatedAt" to Date(),
                        "deviceInfo" to
                            mapOf(
                                "platform" to "android",
                                "appVersion" to
                                    context.packageManager
                                        .getPackageInfo(context.packageName, 0)
                                        .versionName,
                            ),
                    )

                // Firestoreに保存: admins/{uid}/fcm_tokens/{tokenId}
                Tasks.await(
                    firestore
                        .collection("admins")
                        .document(uid)
                        .collection("fcm_tokens")
                        .document(token)
                        .set(tokenData),
                )

                Log.d(TAG, "FCM token saved to Firestore for admin: $uid")
            } catch (e: Exception) {
                Log.e(TAG, "Failed to save FCM token to Firestore", e)
            }
        }
    }

    /**
     * ユーザーがAdmin権限を持つかチェック
     */
    private suspend fun checkIsAdmin(uid: String): Boolean = withContext(Dispatchers.IO) {
        try {
            val adminDoc =
                Tasks.await(
                    firestore
                        .collection("admins")
                        .document(uid)
                        .get(),
                )
            adminDoc.exists()
        } catch (e: Exception) {
            Log.e(TAG, "Failed to check admin status", e)
            false
        }
    }
}
