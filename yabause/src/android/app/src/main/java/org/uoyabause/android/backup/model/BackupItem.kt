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

import com.google.firebase.firestore.IgnoreExtraProperties
import java.util.Date

/**
 * Data class representing a backup item in the standalone backup manager.
 * This class is used for both local and cloud backup items.
 *
 * @property id Unique identifier (index for local, document ID for cloud)
 * @property filename The backup file name
 * @property comment User-defined comment/description for the save
 * @property language Language code of the save data
 * @property saveDate The date when the save was created
 * @property dataSize Size of the actual save data in bytes
 * @property blockSize Number of blocks used by the save
 * @property deviceType The device type where this backup is stored
 * @property downloadUrl Firebase Storage download URL (for cloud items)
 * @property firebaseKey Firebase database key (for cloud items)
 * @property screenshotUrl URL of screenshot taken at BUP_Write (optional)
 * @property gameTitle Title of the game (if available)
 * @property productNumber Product number of the game (if available)
 * @property backupFileKey Backup RAM file name without extension (e.g., "memory", "memory_filebios", "backup4")
 */
@IgnoreExtraProperties
data class BackupItem(
    val id: String = "",
    val filename: String = "",
    val comment: String = "",
    val language: Int = 0,
    val saveDate: String = "",
    val dataSize: Int = 0,
    val blockSize: Int = 0,
    val deviceType: DeviceType = DeviceType.INTERNAL,
    val downloadUrl: String? = null,
    val firebaseKey: String? = null,
    val screenshotUrl: String? = null,
    val gameTitle: String? = null,
    val productNumber: String? = null,
    val backupFileKey: String = "",
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
     * Display-friendly block count
     */
    val displayBlocks: String
        get() = "$blockSize blocks"

    /**
     * Check if this is a cloud backup
     */
    val isCloudBackup: Boolean
        get() = deviceType == DeviceType.CLOUD || downloadUrl != null

    companion object {
        /**
         * Create a BackupItem from JSON data returned by JNI
         * @param index The index in the device's file list
         * @param json The JSON object containing backup data
         * @param deviceType The device type
         * @return A new BackupItem instance
         */
        fun fromJson(
            index: Int,
            json: org.json.JSONObject,
            deviceType: DeviceType,
        ): BackupItem = BackupItem(
            id = index.toString(),
            filename = decodeBase64String(json.optString("filename", "")),
            comment = decodeBase64String(json.optString("comment", "")),
            language = json.optInt("language", 0),
            saveDate = json.optString("savedate", ""),
            dataSize = json.optInt("datasize", 0),
            blockSize = json.optInt("blocksize", 0),
            deviceType = deviceType,
        )

        /**
         * Decode a Base64 encoded string with MS932 charset
         */
        private fun decodeBase64String(encoded: String): String = try {
            if (encoded.isEmpty()) {
                ""
            } else {
                val decoded = android.util.Base64.decode(encoded, android.util.Base64.DEFAULT)
                String(decoded, charset("MS932"))
            }
        } catch (e: Exception) {
            android.util.Log.w("BackupItem", "Failed to decode Base64 string", e)
            ""
        }
    }
}

/**
 * Data class for backup file metadata stored in Firebase
 * Used for cloud synchronization
 */
@IgnoreExtraProperties
data class CloudBackupMetadata(
    val filename: String = "",
    val comment: String = "",
    val language: Int = 0,
    val saveDate: String = "",
    val dataSize: Int = 0,
    val blockSize: Int = 0,
    val downloadUrl: String = "",
    val url: String = "",
    val uploadedAt: Date? = null,
    val gameTitle: String? = null,
    val productNumber: String? = null,
    val screenshotUrl: String? = null,
) {
    /**
     * Convert to BackupItem
     */
    fun toBackupItem(key: String): BackupItem = BackupItem(
        id = key,
        filename = filename,
        comment = comment,
        language = language,
        saveDate = saveDate,
        dataSize = dataSize,
        blockSize = blockSize,
        deviceType = DeviceType.CLOUD,
        downloadUrl = downloadUrl.ifEmpty { url }.ifEmpty { null },
        firebaseKey = key,
        screenshotUrl = screenshotUrl,
        gameTitle = gameTitle,
        productNumber = productNumber,
    )
}
