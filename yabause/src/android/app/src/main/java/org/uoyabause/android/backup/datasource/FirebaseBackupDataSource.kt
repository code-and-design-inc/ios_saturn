/*
 * Copyright 2024 devMiyax
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.uoyabause.android.backup.datasource

import android.content.Context
import android.util.Log
import com.google.firebase.database.DataSnapshot
import com.google.firebase.database.DatabaseError
import com.google.firebase.database.FirebaseDatabase
import com.google.firebase.database.ValueEventListener
import com.google.firebase.firestore.FieldValue
import com.google.firebase.firestore.FirebaseFirestore
import com.google.firebase.firestore.Query
import com.google.firebase.storage.FirebaseStorage
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.channels.awaitClose
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.callbackFlow
import kotlinx.coroutines.tasks.await
import kotlinx.coroutines.withContext
import org.uoyabause.android.auth.AuthState
import org.uoyabause.android.backup.model.BackupItem
import org.uoyabause.android.backup.model.BackupRating
import org.uoyabause.android.backup.model.CloudBackupMetadata
import org.uoyabause.android.backup.model.SharedBackupItem
import org.uoyabause.android.backup.viewmodel.SharedBackupSortOrder
import java.util.UUID

/**
 * Data source for Firebase operations related to backups.
 * Handles both personal cloud backups and shared community backups.
 */
class FirebaseBackupDataSource(
    private val context: Context,
) {
    companion object {
        private const val TAG = "FirebaseBackupDataSource"

        // Firebase Realtime Database paths (for personal cloud backups - legacy)
        private const val USER_POSTS_PATH = "user-posts"
        private const val BACKUP_PATH = "backup"
        private const val MAX_BACKUP_COUNT_PATH = "max_backup_count"

        // Firestore collections (for shared backups)
        private const val SHARED_BACKUPS_COLLECTION = "shared_backups"
        private const val RATINGS_SUBCOLLECTION = "ratings"

        // Firebase Storage paths
        private const val CLOUD_BACKUP_STORAGE_PATH = "backups"
        private const val SHARED_BACKUP_STORAGE_PATH = "shared_backups"

        // Limits
        private const val DEFAULT_MAX_BACKUP_COUNT = 0
        private const val PRO_MAX_BACKUP_COUNT = 256
        private const val SHARED_BACKUPS_PAGE_SIZE = 20L
    }

    private val database = FirebaseDatabase.getInstance()
    private val firestore = FirebaseFirestore.getInstance()
    private val storage = FirebaseStorage.getInstance()

    /**
     * Check if user is authenticated.
     */
    fun isAuthenticated(): Boolean = AuthState.isSignedIn()

    /**
     * Get current user ID.
     */
    fun getCurrentUserId(): String? = AuthState.realUser()?.uid

    /**
     * Get current user display name.
     */
    fun getCurrentUserName(): String = AuthState.realUser()?.displayName ?: "Unknown"

    /**
     * Get current user photo URL.
     */
    fun getCurrentUserPhotoUrl(): String? = AuthState.realUser()?.photoUrl?.toString()

    // ============================================================
    // Personal Cloud Backups (Firebase Realtime Database)
    // ============================================================

    /**
     * Get user's personal cloud backup items as a Flow.
     * Uses Firebase Realtime Database for real-time updates.
     */
    fun getCloudBackupItems(): Flow<List<BackupItem>> = callbackFlow {
        val uid = AuthState.realUser()?.uid
        if (uid == null) {
            trySend(emptyList())
            close()
            return@callbackFlow
        }

        val backupRef = database.reference
            .child(USER_POSTS_PATH)
            .child(uid)
            .child(BACKUP_PATH)

        val listener = object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                val items = mutableListOf<BackupItem>()
                var index = 0

                Log.d(TAG, "Cloud backup snapshot childrenCount=${snapshot.childrenCount}")
                for (child in snapshot.children) {
                    val key = child.key ?: index.toString()
                    try {
                        Log.d(TAG, "Parsing cloud backup key=$key, rawValue=${child.value}")
                        val metadata = child.getValue(CloudBackupMetadata::class.java)
                        if (metadata != null) {
                            Log.d(TAG, "Parsed OK key=$key filename=${metadata.filename} uploadedAt=${metadata.uploadedAt} dataSize=${metadata.dataSize}")
                            items.add(metadata.toBackupItem(key))
                        } else {
                            Log.w(TAG, "Cloud backup metadata is null for key=$key")
                        }
                    } catch (e: Exception) {
                        Log.e(TAG, "Error parsing cloud backup key=$key: ${e.javaClass.simpleName}: ${e.message}", e)
                        // Log raw field types to identify type mismatch
                        try {
                            val raw = child.value as? Map<*, *>
                            raw?.forEach { (k, v) ->
                                Log.e(TAG, "  field '$k' type=${v?.javaClass?.simpleName} value=$v")
                            }
                        } catch (e2: Exception) {
                            Log.e(TAG, "  Could not dump raw fields: ${e2.message}")
                        }
                    }
                    index++
                }

                Log.d(TAG, "Cloud backup total parsed: ${items.size} / ${snapshot.childrenCount}")
                trySend(items)
            }

            override fun onCancelled(error: DatabaseError) {
                Log.e(TAG, "Cloud backup listener cancelled: ${error.message}")
                trySend(emptyList())
            }
        }

        backupRef.addValueEventListener(listener)

        awaitClose {
            backupRef.removeEventListener(listener)
        }
    }

    /**
     * Get max backup count for the current user.
     */
    suspend fun getMaxBackupCount(): Int = withContext(Dispatchers.IO) {
        val uid = AuthState.realUser()?.uid ?: return@withContext DEFAULT_MAX_BACKUP_COUNT

        try {
            val snapshot = database.reference
                .child(USER_POSTS_PATH)
                .child(uid)
                .child(MAX_BACKUP_COUNT_PATH)
                .get()
                .await()

            (snapshot.getValue(Long::class.java) ?: DEFAULT_MAX_BACKUP_COUNT.toLong()).toInt()
        } catch (e: Exception) {
            Log.e(TAG, "Error getting max backup count: ${e.message}", e)
            DEFAULT_MAX_BACKUP_COUNT
        }
    }

    /**
     * Set max backup count for the current user.
     * Pro/donated users get PRO_MAX_BACKUP_COUNT (256), free users get DEFAULT_MAX_BACKUP_COUNT (0).
     */
    suspend fun updateMaxBackupCount(hasDonated: Boolean): Unit = withContext(Dispatchers.IO) {
        val uid = AuthState.realUser()?.uid ?: return@withContext
        val maxCount = if (hasDonated) PRO_MAX_BACKUP_COUNT else DEFAULT_MAX_BACKUP_COUNT

        try {
            database.reference
                .child(USER_POSTS_PATH)
                .child(uid)
                .child(MAX_BACKUP_COUNT_PATH)
                .setValue(maxCount)
                .await()
        } catch (e: Exception) {
            Log.e(TAG, "Error updating max backup count: ${e.message}", e)
        }
    }

    /**
     * Upload a backup to personal cloud storage.
     * @param backupData The raw backup data
     * @param metadata The backup metadata
     * @param screenshotData Optional screenshot PNG data to upload alongside the backup
     * @return Download URL on success, null on failure
     */
    suspend fun uploadCloudBackup(
        backupData: ByteArray,
        metadata: CloudBackupMetadata,
        screenshotData: ByteArray? = null,
    ): String? = withContext(Dispatchers.IO) {
        val uid = AuthState.realUser()?.uid ?: return@withContext null

        try {
            // Generate unique key
            val key = UUID.randomUUID().toString()

            // Upload file to Storage
            val storageRef = storage.reference
                .child(uid)
                .child(CLOUD_BACKUP_STORAGE_PATH)
                .child(key)

            storageRef.putBytes(backupData).await()
            val downloadUrl = storageRef.downloadUrl.await().toString()

            // Upload screenshot if available
            var screenshotUrl = metadata.screenshotUrl
            if (screenshotData != null) {
                val screenshotRef = storage.reference
                    .child(uid)
                    .child(CLOUD_BACKUP_STORAGE_PATH)
                    .child("${key}_screenshot.png")
                screenshotRef.putBytes(screenshotData).await()
                screenshotUrl = screenshotRef.downloadUrl.await().toString()
            }

            // Save metadata to Realtime Database
            val metadataWithUrl = metadata.copy(
                downloadUrl = downloadUrl,
                screenshotUrl = screenshotUrl,
            )
            database.reference
                .child(USER_POSTS_PATH)
                .child(uid)
                .child(BACKUP_PATH)
                .child(key)
                .setValue(metadataWithUrl)
                .await()

            downloadUrl
        } catch (e: Exception) {
            Log.e(TAG, "Error uploading cloud backup: ${e.message}", e)
            null
        }
    }

    /**
     * Download a backup from personal cloud storage.
     * @param item The backup item to download
     * @return ByteArray of backup data on success, null on failure
     */
    suspend fun downloadCloudBackup(item: BackupItem): ByteArray? = withContext(Dispatchers.IO) {
        val downloadUrl = item.downloadUrl
        if (downloadUrl.isNullOrEmpty()) {
            Log.e(TAG, "Cannot download: no downloadUrl for item ${item.filename}")
            return@withContext null
        }

        try {
            val storageRef = storage.getReferenceFromUrl(downloadUrl)
            val maxSize = 10L * 1024 * 1024 // 10MB max
            storageRef.getBytes(maxSize).await()
        } catch (e: Exception) {
            Log.e(TAG, "Error downloading cloud backup: ${e.message}", e)
            null
        }
    }

    /**
     * Download a screenshot from Firebase Storage.
     * @param screenshotUrl The Firebase Storage URL of the screenshot
     * @return ByteArray of screenshot PNG data on success, null on failure
     */
    suspend fun downloadScreenshot(screenshotUrl: String): ByteArray? = withContext(Dispatchers.IO) {
        if (!screenshotUrl.startsWith("https://")) {
            return@withContext null
        }

        try {
            val storageRef = storage.getReferenceFromUrl(screenshotUrl)
            val maxSize = 5L * 1024 * 1024 // 5MB max for screenshots
            storageRef.getBytes(maxSize).await()
        } catch (e: Exception) {
            Log.e(TAG, "Error downloading screenshot: ${e.message}", e)
            null
        }
    }

    /**
     * Delete a backup from personal cloud storage.
     * @param item The backup item to delete
     * @return true on success, false on failure
     */
    suspend fun deleteCloudBackup(item: BackupItem): Boolean = withContext(Dispatchers.IO) {
        val uid = AuthState.realUser()?.uid ?: return@withContext false
        val key = item.firebaseKey ?: return@withContext false

        try {
            // Delete from Storage if URL exists
            item.downloadUrl?.let { url ->
                try {
                    storage.getReferenceFromUrl(url).delete().await()
                } catch (e: Exception) {
                    Log.w(TAG, "Storage file already deleted or not found: ${e.message}")
                }
            }

            // Delete screenshot from Storage if it's a Firebase URL
            item.screenshotUrl?.let { url ->
                if (url.startsWith("https://")) {
                    try {
                        storage.getReferenceFromUrl(url).delete().await()
                    } catch (e: Exception) {
                        Log.w(TAG, "Screenshot file already deleted or not found: ${e.message}")
                    }
                }
            }

            // Delete from Realtime Database
            database.reference
                .child(USER_POSTS_PATH)
                .child(uid)
                .child(BACKUP_PATH)
                .child(key)
                .removeValue()
                .await()

            true
        } catch (e: Exception) {
            Log.e(TAG, "Error deleting cloud backup: ${e.message}", e)
            false
        }
    }

    // ============================================================
    // Shared Community Backups (Firestore)
    // ============================================================

    /**
     * Share a backup publicly.
     * @param backup The backup item to share
     * @param backupData The raw backup data
     * @param gameTitle Title of the game
     * @param productNumber Product number of the game
     * @param description User's description about the save
     * @return The shared backup ID on success, null on failure
     */
    suspend fun shareBackup(
        backup: BackupItem,
        backupData: ByteArray,
        gameTitle: String,
        productNumber: String,
        description: String,
        screenshotData: ByteArray? = null,
    ): String? = withContext(Dispatchers.IO) {
        val uid = AuthState.realUser()?.uid ?: return@withContext null
        val userName = getCurrentUserName()
        val userPhotoUrl = getCurrentUserPhotoUrl()

        try {
            // Generate unique ID
            val backupId = UUID.randomUUID().toString()

            // Upload to shared storage
            val storageRef = storage.reference
                .child(SHARED_BACKUP_STORAGE_PATH)
                .child(backupId)
                .child(backup.filename)

            storageRef.putBytes(backupData).await()
            val downloadUrl = storageRef.downloadUrl.await().toString()

            // Upload screenshot if provided
            var screenshotUrl = backup.screenshotUrl
            if (screenshotData != null) {
                val screenshotRef = storage.reference
                    .child(SHARED_BACKUP_STORAGE_PATH)
                    .child(backupId)
                    .child("screenshot.png")
                screenshotRef.putBytes(screenshotData).await()
                screenshotUrl = screenshotRef.downloadUrl.await().toString()
            }

            // Create shared backup item
            val sharedItem = SharedBackupItem.fromBackupItem(
                backup = backup.copy(
                    gameTitle = gameTitle,
                    productNumber = productNumber,
                    screenshotUrl = screenshotUrl,
                ),
                ownerId = uid,
                ownerName = userName,
                ownerPhotoUrl = userPhotoUrl,
                downloadUrl = downloadUrl,
                description = description,
            )

            // Save to Firestore
            firestore
                .collection(SHARED_BACKUPS_COLLECTION)
                .document(backupId)
                .set(sharedItem.toMap())
                .await()

            backupId
        } catch (e: Exception) {
            Log.e(TAG, "Error sharing backup: ${e.message}", e)
            null
        }
    }

    /**
     * Search for shared backups.
     * @param query Search query (matches game title or product number)
     * @param gameTitle Optional exact game title filter
     * @param productNumber Optional exact product number filter
     * @param productNumbers Optional list of product numbers for library filter
     * @param sortOrder Sort order for results
     * @return Flow of matching shared backup items
     */
    fun searchSharedBackups(
        query: String? = null,
        gameTitle: String? = null,
        productNumber: String? = null,
        productNumbers: List<String>? = null,
        sortOrder: SharedBackupSortOrder = SharedBackupSortOrder.DATE_DESC,
    ): Flow<List<SharedBackupItem>> = callbackFlow {
        val fetchLimit = if (!productNumbers.isNullOrEmpty()) {
            100L
        } else {
            SHARED_BACKUPS_PAGE_SIZE
        }

        var firestoreQuery: Query = firestore
            .collection(SHARED_BACKUPS_COLLECTION)
            .whereEqualTo("isPublic", true)
            .orderBy("sharedAt", Query.Direction.DESCENDING)
            .limit(fetchLimit)

        // Apply exact product number filter
        if (!productNumber.isNullOrBlank()) {
            firestoreQuery = firestoreQuery.whereEqualTo("productNumber", productNumber)
        }

        val listener = firestoreQuery.addSnapshotListener { snapshot, error ->
            if (error != null) {
                Log.e(TAG, "Error searching shared backups: ${error.message}", error)
                trySend(emptyList())
                return@addSnapshotListener
            }

            var items = snapshot
                ?.documents
                ?.mapNotNull { doc ->
                    SharedBackupItem.fromDocument(doc)
                }?.filter { item ->
                    // Client-side filtering for text search
                    if (query.isNullOrBlank()) {
                        true
                    } else {
                        item.gameTitle.contains(query, ignoreCase = true) ||
                            item.productNumber.contains(query, ignoreCase = true) ||
                            item.comment.contains(query, ignoreCase = true) ||
                            item.description.contains(query, ignoreCase = true)
                    }
                }?.filter { item ->
                    // Client-side library filter
                    if (productNumbers.isNullOrEmpty()) {
                        true
                    } else {
                        productNumbers.contains(item.productNumber)
                    }
                } ?: emptyList()

            // Client-side sorting
            items = when (sortOrder) {
                SharedBackupSortOrder.RATING_DESC ->
                    items.sortedByDescending { it.averageRating }
                SharedBackupSortOrder.DATE_DESC ->
                    items // Already sorted by sharedAt DESC from Firestore
            }

            trySend(items)
        }

        awaitClose {
            listener.remove()
        }
    }

    /**
     * Fetch a single shared backup by its Firestore document id.
     * @return the item, or null if not found / parse failed.
     */
    suspend fun getSharedBackupById(id: String): SharedBackupItem? = withContext(Dispatchers.IO) {
        try {
            val doc = firestore
                .collection(SHARED_BACKUPS_COLLECTION)
                .document(id)
                .get()
                .await()
            if (!doc.exists()) return@withContext null
            SharedBackupItem.fromDocument(doc)
        } catch (e: Exception) {
            Log.e(TAG, "Error fetching shared backup $id: ${e.message}", e)
            null
        }
    }

    /**
     * Download a shared backup.
     * @param item The shared backup item
     * @return ByteArray of backup data on success, null on failure
     */
    suspend fun downloadSharedBackup(item: SharedBackupItem): ByteArray? = withContext(Dispatchers.IO) {
        try {
            val storageRef = storage.getReferenceFromUrl(item.downloadUrl)
            val maxSize = 10L * 1024 * 1024 // 10MB max
            storageRef.getBytes(maxSize).await()
        } catch (e: Exception) {
            Log.e(TAG, "Error downloading shared backup: ${e.message}", e)
            null
        }
    }

    /**
     * Increment the public download counter for a shared backup. This is a
     * best-effort side effect that should be called only when an import has
     * actually completed (not merely when the bytes were fetched), so that a
     * cancelled overwrite or a failed local write does not inflate the count.
     *
     * A consumer is usually not the document owner, so this write can be
     * rejected by Firestore rules (PERMISSION_DENIED); that is logged and
     * swallowed so it never breaks the caller.
     * @return true if the counter was incremented, false otherwise.
     */
    suspend fun incrementSharedBackupDownloadCount(id: String): Boolean = withContext(Dispatchers.IO) {
        try {
            firestore
                .collection(SHARED_BACKUPS_COLLECTION)
                .document(id)
                .update("downloadCount", FieldValue.increment(1))
                .await()
            true
        } catch (e: Exception) {
            Log.w(TAG, "Could not update downloadCount (non-fatal): ${e.message}")
            false
        }
    }

    /**
     * Rate a shared backup.
     * @param backupId The backup ID
     * @param rating Rating value (1-5)
     * @return true on success, false on failure
     */
    suspend fun rateBackup(backupId: String, rating: Int): Boolean = withContext(Dispatchers.IO) {
        val uid = AuthState.realUser()?.uid ?: return@withContext false

        if (rating < 1 || rating > 5) {
            Log.e(TAG, "Invalid rating value: $rating")
            return@withContext false
        }

        try {
            val backupRef = firestore.collection(SHARED_BACKUPS_COLLECTION).document(backupId)
            val ratingRef = backupRef.collection(RATINGS_SUBCOLLECTION).document(uid)

            // Save user's rating
            val ratingData = BackupRating(rating = rating)
            ratingRef.set(ratingData.toMap()).await()

            true
        } catch (e: Exception) {
            Log.e(TAG, "Error rating backup: ${e.message}", e)
            false
        }
    }

    /**
     * Get user's rating for a shared backup.
     * @param backupId The backup ID
     * @return The rating (1-5), or null if not rated
     */
    suspend fun getUserRating(backupId: String): Int? = withContext(Dispatchers.IO) {
        val uid = AuthState.realUser()?.uid ?: return@withContext null

        try {
            val doc = firestore
                .collection(SHARED_BACKUPS_COLLECTION)
                .document(backupId)
                .collection(RATINGS_SUBCOLLECTION)
                .document(uid)
                .get()
                .await()

            BackupRating.fromDocument(doc)?.rating
        } catch (e: Exception) {
            Log.e(TAG, "Error getting user rating: ${e.message}", e)
            null
        }
    }

    /**
     * Delete a shared backup (owner only).
     * @param item The shared backup item
     * @return true on success, false on failure
     */
    suspend fun deleteSharedBackup(item: SharedBackupItem): Boolean = withContext(Dispatchers.IO) {
        val uid = AuthState.realUser()?.uid ?: return@withContext false

        if (item.ownerId != uid) {
            Log.e(TAG, "Cannot delete shared backup: not the owner")
            return@withContext false
        }

        try {
            // Delete from Storage
            try {
                storage.getReferenceFromUrl(item.downloadUrl).delete().await()
            } catch (e: Exception) {
                Log.w(TAG, "Storage file already deleted: ${e.message}")
            }

            // Delete from Firestore
            firestore
                .collection(SHARED_BACKUPS_COLLECTION)
                .document(item.id)
                .delete()
                .await()

            true
        } catch (e: Exception) {
            Log.e(TAG, "Error deleting shared backup: ${e.message}", e)
            false
        }
    }
}
