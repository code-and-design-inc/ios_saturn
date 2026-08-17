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
package org.uoyabause.android.backup.repository

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.withContext
import org.uoyabause.android.backup.datasource.FirebaseBackupDataSource
import org.uoyabause.android.backup.model.BackupItem
import org.uoyabause.android.backup.model.CloudBackupMetadata
import org.uoyabause.android.backup.model.SharedBackupItem
import org.uoyabause.android.backup.viewmodel.SharedBackupSortOrder
import java.util.Date

/**
 * Repository for cloud backup operations.
 * Handles both personal cloud backups and shared community backups.
 */
class CloudBackupRepository(
    context: Context,
) {
    private val firebaseDataSource = FirebaseBackupDataSource(context)

    // ============================================================
    // Authentication
    // ============================================================

    /**
     * Check if user is authenticated.
     */
    fun isAuthenticated(): Boolean = firebaseDataSource.isAuthenticated()

    /**
     * Get current user info.
     */
    fun getCurrentUser(): UserInfo? {
        if (!isAuthenticated()) return null
        return UserInfo(
            uid = firebaseDataSource.getCurrentUserId() ?: "",
            displayName = firebaseDataSource.getCurrentUserName(),
            photoUrl = firebaseDataSource.getCurrentUserPhotoUrl(),
        )
    }

    // ============================================================
    // Personal Cloud Backups
    // ============================================================

    /**
     * Get user's personal cloud backup items.
     * @return Flow of backup items
     */
    fun getCloudBackupItems(): Flow<List<BackupItem>> = firebaseDataSource.getCloudBackupItems()

    /**
     * Get backup limits for the current user.
     * @return BackupLimits object
     */
    suspend fun getBackupLimits(): BackupLimits = withContext(Dispatchers.IO) {
        val maxCount = firebaseDataSource.getMaxBackupCount()
        BackupLimits(maxCount = maxCount)
    }

    /**
     * Update backup limits based on donation status.
     * @param hasDonated Whether the user has donated
     */
    suspend fun updateBackupLimits(hasDonated: Boolean) {
        firebaseDataSource.updateMaxBackupCount(hasDonated)
    }

    /**
     * Upload a backup to personal cloud storage.
     * @param backupData The raw backup data
     * @param item The backup item metadata
     * @param screenshotData Optional screenshot PNG data to upload
     * @return Result with download URL on success
     */
    suspend fun uploadBackup(
        backupData: ByteArray,
        item: BackupItem,
        screenshotData: ByteArray? = null,
    ): Result<String> = withContext(Dispatchers.IO) {
        try {
            val metadata = CloudBackupMetadata(
                filename = item.filename,
                comment = item.comment,
                language = item.language,
                saveDate = item.saveDate,
                dataSize = item.dataSize,
                blockSize = item.blockSize,
                uploadedAt = Date(),
                gameTitle = item.gameTitle,
                productNumber = item.productNumber,
                screenshotUrl = item.screenshotUrl,
            )

            val downloadUrl = firebaseDataSource.uploadCloudBackup(backupData, metadata, screenshotData)
            if (downloadUrl != null) {
                Result.success(downloadUrl)
            } else {
                Result.failure(Exception("Failed to upload backup"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * Download a backup from personal cloud storage.
     * @param item The backup item to download
     * @return Result with backup data on success
     */
    suspend fun downloadBackup(item: BackupItem): Result<ByteArray> = withContext(Dispatchers.IO) {
        try {
            val data = firebaseDataSource.downloadCloudBackup(item)
            if (data != null) {
                Result.success(data)
            } else {
                Result.failure(Exception("Failed to download backup"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * Download a screenshot from cloud storage.
     * @param screenshotUrl The Firebase Storage URL of the screenshot
     * @return Result with screenshot PNG data on success
     */
    suspend fun downloadScreenshot(screenshotUrl: String): Result<ByteArray> = withContext(Dispatchers.IO) {
        try {
            val data = firebaseDataSource.downloadScreenshot(screenshotUrl)
            if (data != null) {
                Result.success(data)
            } else {
                Result.failure(Exception("Failed to download screenshot"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * Delete a backup from personal cloud storage.
     * @param item The backup item to delete
     * @return Result indicating success or failure
     */
    suspend fun deleteBackup(item: BackupItem): Result<Unit> = withContext(Dispatchers.IO) {
        try {
            val success = firebaseDataSource.deleteCloudBackup(item)
            if (success) {
                Result.success(Unit)
            } else {
                Result.failure(Exception("Failed to delete backup"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    // ============================================================
    // Shared Community Backups
    // ============================================================

    /**
     * Share a backup publicly.
     * @param backup The backup item to share
     * @param backupData The raw backup data
     * @param shareInfo Additional sharing information
     * @return Result with shared backup ID on success
     */
    suspend fun shareBackup(
        backup: BackupItem,
        backupData: ByteArray,
        shareInfo: ShareInfo,
        screenshotData: ByteArray? = null,
    ): Result<String> = withContext(Dispatchers.IO) {
        try {
            val backupId = firebaseDataSource.shareBackup(
                backup = backup,
                backupData = backupData,
                gameTitle = shareInfo.gameTitle,
                productNumber = shareInfo.productNumber,
                description = shareInfo.description,
                screenshotData = screenshotData,
            )

            if (backupId != null) {
                Result.success(backupId)
            } else {
                Result.failure(Exception("Failed to share backup"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * Search for shared backups.
     * @param query Optional text search query
     * @param gameTitle Optional game title filter
     * @param productNumber Optional product number filter
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
    ): Flow<List<SharedBackupItem>> = firebaseDataSource.searchSharedBackups(
        query = query,
        gameTitle = gameTitle,
        productNumber = productNumber,
        productNumbers = productNumbers,
        sortOrder = sortOrder,
    )

    /**
     * Fetch a single shared backup by id (used by deep-link import).
     * @return Result with the item, or failure if not found.
     */
    suspend fun getSharedBackupById(id: String): Result<SharedBackupItem> = withContext(Dispatchers.IO) {
        val item = firebaseDataSource.getSharedBackupById(id)
        if (item != null) Result.success(item) else Result.failure(Exception("Shared backup not found"))
    }

    /**
     * Download a shared backup.
     * @param item The shared backup item
     * @return Result with backup data on success
     */
    suspend fun downloadSharedBackup(item: SharedBackupItem): Result<ByteArray> = withContext(Dispatchers.IO) {
        try {
            val data = firebaseDataSource.downloadSharedBackup(item)
            if (data != null) {
                Result.success(data)
            } else {
                Result.failure(Exception("Failed to download shared backup"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * Increment the public download counter for a shared backup. Call this only
     * after an import has actually completed. Best-effort: never throws.
     * @return true if the counter was incremented.
     */
    suspend fun incrementSharedBackupDownloadCount(id: String): Boolean = withContext(Dispatchers.IO) {
        firebaseDataSource.incrementSharedBackupDownloadCount(id)
    }

    /**
     * Rate a shared backup.
     * @param backupId The backup ID
     * @param rating Rating value (1-5)
     * @return Result indicating success or failure
     */
    suspend fun rateBackup(backupId: String, rating: Int): Result<Unit> = withContext(Dispatchers.IO) {
        try {
            if (rating < 1 || rating > 5) {
                return@withContext Result.failure(IllegalArgumentException("Rating must be between 1 and 5"))
            }

            val success = firebaseDataSource.rateBackup(backupId, rating)
            if (success) {
                Result.success(Unit)
            } else {
                Result.failure(Exception("Failed to rate backup"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * Get user's rating for a shared backup.
     * @param backupId The backup ID
     * @return The rating (1-5), or null if not rated
     */
    suspend fun getUserRating(backupId: String): Int? = firebaseDataSource.getUserRating(backupId)

    /**
     * Delete a shared backup (owner only).
     * @param item The shared backup item
     * @return Result indicating success or failure
     */
    suspend fun deleteSharedBackup(item: SharedBackupItem): Result<Unit> = withContext(Dispatchers.IO) {
        try {
            val success = firebaseDataSource.deleteSharedBackup(item)
            if (success) {
                Result.success(Unit)
            } else {
                Result.failure(Exception("Failed to delete shared backup"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * Check if current user owns a shared backup.
     */
    fun isOwner(item: SharedBackupItem): Boolean {
        val uid = firebaseDataSource.getCurrentUserId()
        return item.isOwnedBy(uid)
    }
}

/**
 * Data class representing current user info.
 */
data class UserInfo(
    val uid: String,
    val displayName: String,
    val photoUrl: String?,
)

/**
 * Data class representing backup limits.
 */
data class BackupLimits(
    val maxCount: Int,
    val currentCount: Int = 0,
) {
    val isLimitReached: Boolean
        get() = currentCount >= maxCount

    val remainingCount: Int
        get() = (maxCount - currentCount).coerceAtLeast(0)

    val displayString: String
        get() = "$currentCount / $maxCount"
}

/**
 * Data class for sharing information.
 */
data class ShareInfo(
    val gameTitle: String,
    val productNumber: String,
    val description: String,
)
