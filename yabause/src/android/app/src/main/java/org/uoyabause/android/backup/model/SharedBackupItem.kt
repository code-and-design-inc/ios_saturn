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
package org.uoyabause.android.backup.model

import com.google.firebase.Timestamp
import com.google.firebase.firestore.DocumentSnapshot
import com.google.firebase.firestore.IgnoreExtraProperties
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Data class representing a shared backup item from other users.
 * These are public backups shared by the community that can be searched and imported.
 *
 * @property id Firestore document ID
 * @property ownerId Firebase UID of the owner
 * @property ownerName Display name of the owner
 * @property ownerPhotoUrl Profile photo URL of the owner
 * @property gameTitle Title of the game
 * @property productNumber Product number/code of the game
 * @property filename Backup file name
 * @property comment Original comment from the backup file
 * @property description User's description when sharing (e.g., "100% completion", "Before final boss")
 * @property saveDate Original save date from the backup
 * @property downloadUrl Firebase Storage download URL
 * @property sharedAt When this backup was shared
 * @property downloadCount Number of times this backup has been downloaded
 * @property isPublic Whether this backup is publicly visible
 * @property averageRating Average rating (1-5 stars)
 * @property ratingCount Number of ratings received
 * @property dataSize Size of the backup data in bytes
 * @property blockSize Number of blocks used
 * @property screenshotUrl URL of screenshot (optional)
 */
@IgnoreExtraProperties
data class SharedBackupItem(
    val id: String = "",
    val ownerId: String = "",
    val ownerName: String = "",
    val ownerPhotoUrl: String? = null,
    val gameTitle: String = "",
    val productNumber: String = "",
    val filename: String = "",
    val comment: String = "",
    val description: String = "",
    val saveDate: String = "",
    val downloadUrl: String = "",
    val sharedAt: Date? = null,
    val downloadCount: Int = 0,
    val isPublic: Boolean = true,
    val averageRating: Float = 0f,
    val ratingCount: Int = 0,
    val dataSize: Int = 0,
    val blockSize: Int = 0,
    val screenshotUrl: String? = null,
) {
    /**
     * Display-friendly size string
     */
    val displaySize: String
        get() = when {
            dataSize >= 1024 * 1024 -> String.format("%.1f MB", dataSize / (1024.0 * 1024.0))
            dataSize >= 1024 -> String.format("%.1f KB", dataSize / 1024.0)
            else -> "$dataSize B"
        }

    /**
     * Display-friendly shared date
     */
    val displaySharedDate: String
        get() = sharedAt?.let {
            SimpleDateFormat("yyyy/MM/dd", Locale.getDefault()).format(it)
        } ?: ""

    /**
     * Display-friendly rating string
     */
    val displayRating: String
        get() = if (ratingCount > 0) {
            String.format("%.1f (%d)", averageRating, ratingCount)
        } else {
            "No ratings"
        }

    /**
     * Check if current user is the owner
     */
    fun isOwnedBy(uid: String?): Boolean = uid != null && ownerId == uid

    companion object {
        /**
         * Create from Firestore DocumentSnapshot
         */
        fun fromDocument(document: DocumentSnapshot): SharedBackupItem? = try {
            val ratings = document.get("ratings") as? Map<*, *>
            SharedBackupItem(
                id = document.id,
                ownerId = document.getString("ownerId") ?: "",
                ownerName = document.getString("ownerName") ?: "Unknown",
                ownerPhotoUrl = document.getString("ownerPhotoUrl"),
                gameTitle = document.getString("gameTitle") ?: "",
                productNumber = document.getString("productNumber") ?: "",
                filename = document.getString("filename") ?: "",
                comment = document.getString("comment") ?: "",
                description = document.getString("description") ?: "",
                saveDate = document.getString("saveDate") ?: "",
                downloadUrl = document.getString("downloadUrl") ?: "",
                sharedAt = document.getTimestamp("sharedAt")?.toDate(),
                downloadCount = document.getLong("downloadCount")?.toInt() ?: 0,
                isPublic = document.getBoolean("isPublic") ?: true,
                averageRating = (ratings?.get("average") as? Number)?.toFloat() ?: 0f,
                ratingCount = (ratings?.get("count") as? Number)?.toInt() ?: 0,
                dataSize = document.getLong("dataSize")?.toInt() ?: 0,
                blockSize = document.getLong("blockSize")?.toInt() ?: 0,
                screenshotUrl = document.getString("screenshotUrl"),
            )
        } catch (e: Exception) {
            null
        }

        /**
         * Convert BackupItem to SharedBackupItem for sharing
         */
        fun fromBackupItem(
            backup: BackupItem,
            ownerId: String,
            ownerName: String,
            ownerPhotoUrl: String?,
            downloadUrl: String,
            description: String = "",
        ): SharedBackupItem = SharedBackupItem(
            ownerId = ownerId,
            ownerName = ownerName,
            ownerPhotoUrl = ownerPhotoUrl,
            gameTitle = backup.gameTitle ?: "",
            productNumber = backup.productNumber ?: "",
            filename = backup.filename,
            comment = backup.comment,
            description = description,
            saveDate = backup.saveDate,
            downloadUrl = downloadUrl,
            sharedAt = Date(),
            downloadCount = 0,
            isPublic = true,
            averageRating = 0f,
            ratingCount = 0,
            dataSize = backup.dataSize,
            blockSize = backup.blockSize,
            screenshotUrl = backup.screenshotUrl,
        )
    }

    /**
     * Convert to a map for Firestore upload
     */
    fun toMap(): Map<String, Any?> = mapOf(
        "ownerId" to ownerId,
        "ownerName" to ownerName,
        "ownerPhotoUrl" to ownerPhotoUrl,
        "gameTitle" to gameTitle,
        "productNumber" to productNumber,
        "filename" to filename,
        "comment" to comment,
        "description" to description,
        "saveDate" to saveDate,
        "downloadUrl" to downloadUrl,
        "sharedAt" to Timestamp.now(),
        "downloadCount" to downloadCount,
        "isPublic" to isPublic,
        "ratings" to mapOf(
            "average" to averageRating,
            "count" to ratingCount,
        ),
        "dataSize" to dataSize,
        "blockSize" to blockSize,
        "screenshotUrl" to screenshotUrl,
    )
}

/**
 * Data class representing a user's rating for a shared backup
 */
@IgnoreExtraProperties
data class BackupRating(
    val rating: Int = 0,
    val createdAt: Date? = null,
) {
    companion object {
        fun fromDocument(document: DocumentSnapshot): BackupRating? = try {
            BackupRating(
                rating = document.getLong("rating")?.toInt() ?: 0,
                createdAt = document.getTimestamp("createdAt")?.toDate(),
            )
        } catch (e: Exception) {
            null
        }
    }

    fun toMap(): Map<String, Any> = mapOf(
        "rating" to rating,
        "createdAt" to Timestamp.now(),
    )
}
