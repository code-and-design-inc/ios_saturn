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
import android.net.Uri
import android.util.Base64
import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.flow
import kotlinx.coroutines.flow.flowOn
import kotlinx.coroutines.withContext
import org.json.JSONObject
import org.uoyabause.android.backup.datasource.LocalBackupDataSource
import org.uoyabause.android.backup.model.BackupItem
import org.uoyabause.android.backup.model.DeviceType
import org.uoyabause.android.backup.model.LocalBackupFile

/**
 * Repository for local backup operations (Internal and External storage).
 * Acts as a clean API layer between ViewModels and data sources.
 */
class BackupRepository(
    context: Context,
) {
    companion object {
        private const val TAG = "BackupRepository"
    }

    private val localDataSource = LocalBackupDataSource(context)

    /**
     * Get backup items from the specified device as a Flow.
     * @param deviceType The device type (INTERNAL or EXTERNAL)
     * @return Flow emitting the list of backup items
     */
    fun getBackupItems(deviceType: DeviceType): Flow<List<BackupItem>> = flow {
        val items = localDataSource.getBackupItems(deviceType)
        emit(items)
    }.flowOn(Dispatchers.IO)

    /**
     * Get backup items from the specified device (suspend function).
     * @param deviceType The device type
     * @return List of backup items
     */
    suspend fun getBackupItemsSync(deviceType: DeviceType): List<BackupItem> = localDataSource.getBackupItems(deviceType)

    /**
     * Get storage status for the specified device.
     * @param deviceType The device type
     * @param biosSize The logical size from GetDeviceStats
     * @param biosBlockSize The block size from GetDeviceStats
     * @return Pair of (totalSize, freeSize) in bytes
     */
    suspend fun getStorageStatus(deviceType: DeviceType, biosSize: Int, biosBlockSize: Int): StorageStatus =
        withContext(Dispatchers.IO) {
            val (total, free) = localDataSource.getStorageStatus(deviceType, biosSize, biosBlockSize)
            StorageStatus(
                totalSize = total,
                freeSize = free,
                usedSize = total - free,
            )
        }

    /**
     * Export a backup item to an external location.
     * @param item The backup item to export
     * @param destinationUri The destination URI (from SAF picker)
     * @return Result indicating success or failure
     */
    suspend fun exportBackup(item: BackupItem, destinationUri: Uri): Result<Unit> = withContext(Dispatchers.IO) {
        try {
            val success = localDataSource.exportBackup(item, destinationUri)
            if (success) {
                Result.success(Unit)
            } else {
                Result.failure(Exception("Failed to export backup"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * Import a backup from an external location.
     * @param sourceUri The source URI (from SAF picker)
     * @param targetDevice The target device type
     * @return Result indicating success or failure
     */
    suspend fun importBackup(sourceUri: Uri, targetDevice: DeviceType): Result<Unit> = withContext(Dispatchers.IO) {
        try {
            val success = localDataSource.importBackup(sourceUri, targetDevice)
            if (success) {
                Result.success(Unit)
            } else {
                Result.failure(Exception("Failed to import backup"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * Copy a backup item to another device.
     * @param item The backup item to copy
     * @param targetDevice The target device type
     * @return Result indicating success or failure
     */
    suspend fun copyBackup(item: BackupItem, targetDevice: DeviceType): Result<Unit> = withContext(Dispatchers.IO) {
        try {
            val success = localDataSource.copyBackup(item, targetDevice)
            if (success) {
                Result.success(Unit)
            } else {
                Result.failure(Exception("Failed to copy backup"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * Import backup data directly to a device (used for cloud-to-local copy).
     * @param targetDevice The target device type
     * @param filename The save filename
     * @param comment The save comment
     * @param language The language code
     * @param saveDate The save date string in "YYYY/MM/DD HH:mm:ss" format
     * @param data The save data bytes
     * @return Result indicating success or failure
     */
    suspend fun importBackupData(
        targetDevice: DeviceType,
        filename: String,
        comment: String,
        language: Int,
        saveDate: String,
        data: ByteArray,
    ): Result<Unit> = withContext(Dispatchers.IO) {
        try {
            val success = localDataSource.importBackupData(
                targetDevice = targetDevice,
                filename = filename,
                comment = comment,
                language = language,
                saveDate = saveDate,
                data = data,
            )
            if (success) {
                Result.success(Unit)
            } else {
                Result.failure(Exception("Failed to import backup"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * Delete a backup item.
     * @param item The backup item to delete
     * @return Result indicating success or failure
     */
    suspend fun deleteBackup(item: BackupItem): Result<Unit> = withContext(Dispatchers.IO) {
        try {
            val success = localDataSource.deleteBackup(item)
            if (success) {
                Result.success(Unit)
            } else {
                Result.failure(Exception("Failed to delete backup"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * Get raw backup data for cloud upload.
     * @param item The backup item
     * @return ByteArray of backup data, or null if not found
     */
    suspend fun getBackupData(item: BackupItem): ByteArray? = localDataSource.getBackupData(item)

    /**
     * Check if internal backup exists.
     */
    fun hasInternalBackup(): Boolean = localDataSource.hasInternalBackup()

    /**
     * Check if external backup exists.
     */
    fun hasExternalBackup(): Boolean = localDataSource.hasExternalBackup()

    /**
     * Get all available devices.
     * @return List of available device types
     */
    fun getAvailableDevices(): List<DeviceType> {
        val devices = mutableListOf<DeviceType>()

        if (hasInternalBackup()) {
            devices.add(DeviceType.INTERNAL)
        }

        if (hasExternalBackup()) {
            devices.add(DeviceType.EXTERNAL)
        }

        return devices
    }

    /**
     * Get backup items from a specific file path.
     * @param filePath The path to the backup RAM file
     * @param deviceType The device type to assign to items
     * @return List of backup items
     */
    suspend fun getBackupItemsFromFile(filePath: String, deviceType: DeviceType): List<BackupItem> =
        localDataSource.parseBackupFile(filePath, deviceType)

    /**
     * Get storage status from a specific file path.
     * @param filePath The path to the backup RAM file
     * @param biosSize The logical size from GetDeviceStats
     * @param biosBlockSize The block size from GetDeviceStats
     * @return StorageStatus for the file
     */
    suspend fun getStorageStatusFromFile(filePath: String, biosSize: Int, biosBlockSize: Int): StorageStatus =
        withContext(Dispatchers.IO) {
            val (total, free) = localDataSource.getStorageStatusFromFile(filePath, biosSize, biosBlockSize)
            StorageStatus(
                totalSize = total,
                freeSize = free,
                usedSize = total - free,
            )
        }

    /**
     * Delete a backup item from a specific file path.
     * @param filePath The path to the backup RAM file
     * @param item The backup item to delete
     * @return Result indicating success or failure
     */
    suspend fun deleteBackupFromFile(filePath: String, item: BackupItem): Result<Unit> = withContext(Dispatchers.IO) {
        try {
            val success = localDataSource.deleteBackupFromFile(filePath, item)
            if (success) {
                Result.success(Unit)
            } else {
                Result.failure(Exception("Failed to delete backup"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * Get raw backup data from a specific file path.
     * @param filePath The path to the backup RAM file
     * @param item The backup item
     * @return ByteArray of backup data, or null if not found
     */
    suspend fun getBackupDataFromFile(filePath: String, item: BackupItem): ByteArray? =
        localDataSource.extractBackupDataFromFile(filePath, item)

    /**
     * Export a backup item from a specific file path to an external URI.
     * @param filePath The path to the backup RAM file
     * @param item The backup item to export
     * @param uri The destination URI
     * @return Result indicating success or failure
     */
    suspend fun exportBackupFromFile(filePath: String, item: BackupItem, uri: Uri): Result<Unit> =
        withContext(Dispatchers.IO) {
            try {
                val success = localDataSource.exportBackupFromFile(filePath, item, uri)
                if (success) {
                    Result.success(Unit)
                } else {
                    Result.failure(Exception("Failed to export backup"))
                }
            } catch (e: Exception) {
                Result.failure(e)
            }
        }

    /**
     * Check if a backup with the given filename already exists in a backup file.
     * @param filePath The path to the backup RAM file
     * @param filename The save filename to check
     * @param deviceType The device type
     * @return true if a backup with the same filename exists
     */
    suspend fun backupExistsInFile(filePath: String, filename: String, deviceType: DeviceType): Boolean =
        localDataSource.backupExistsInFile(filePath, filename, deviceType)

    /**
     * Copy backup data to a specific local backup file.
     * @param item The backup item to copy
     * @param targetFile The target LocalBackupFile
     * @param backupData The raw backup data bytes
     * @return Result indicating success or failure
     */
    suspend fun copyBackupToFile(
        item: BackupItem,
        targetFile: LocalBackupFile,
        backupData: ByteArray,
    ): Result<Unit> = withContext(Dispatchers.IO) {
        try {
            val (actualData, actualItem, legacyDateRaw) = convertLegacyCloudData(backupData, item)
            val dateRaw = legacyDateRaw ?: encodeSaturnDate(actualItem.saveDate)

            val success = localDataSource.importBackupDataToFile(
                filePath = targetFile.getFilePath(),
                filename = actualItem.filename,
                comment = actualItem.comment,
                language = actualItem.language,
                dateRaw = dateRaw,
                data = actualData,
                deviceType = targetFile.deviceType,
            )
            if (success) {
                Result.success(Unit)
            } else {
                Result.failure(Exception("Failed to copy backup to ${targetFile.displayName}"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * If the data is in the legacy JSON/base64 format from the old backup system
     * (BackupItemFragment + YabauseRunnable.getFile()), parse it and extract the
     * actual deinterleaved save data. Otherwise return as-is.
     *
     * Legacy format: {"header":{...},"data":{"size":N,"content":"<base64>"}}
     * New format: raw deinterleaved bytes
     */
    private fun convertLegacyCloudData(
        rawBytes: ByteArray,
        item: BackupItem,
    ): Triple<ByteArray, BackupItem, Int?> {
        if (rawBytes.isEmpty() || rawBytes[0] != '{'.code.toByte()) {
            return Triple(rawBytes, item, null)
        }

        return try {
            val jsonStr = String(rawBytes, Charsets.UTF_8)
            val json = JSONObject(jsonStr)
            val headerObj = json.optJSONObject("header") ?: return Triple(rawBytes, item, null)
            val dataObj = json.optJSONObject("data") ?: return Triple(rawBytes, item, null)

            val base64Content = dataObj.optString("content", "")
            if (base64Content.isEmpty()) return Triple(rawBytes, item, null)

            val decodedData = Base64.decode(base64Content, Base64.DEFAULT)

            // Extract metadata from JSON header (more reliable than Firebase metadata
            // which may have field name mismatches for old data)
            val filenameB64 = headerObj.optString("filename", "")
            val commentB64 = headerObj.optString("comment", "")
            val filename = if (filenameB64.isNotEmpty()) {
                String(Base64.decode(filenameB64, Base64.DEFAULT), charset("MS932")).trimEnd('\u0000')
            } else {
                item.filename
            }
            val comment = if (commentB64.isNotEmpty()) {
                String(Base64.decode(commentB64, Base64.DEFAULT), charset("MS932")).trimEnd('\u0000')
            } else {
                item.comment
            }
            val language = headerObj.optInt("language", item.language)
            val dateRaw = headerObj.optInt("date", 0)

            val updatedItem = item.copy(
                filename = filename,
                comment = comment,
                language = language,
                dataSize = decodedData.size,
            )

            Log.i(TAG, "Converted legacy JSON backup: '$filename', size=${decodedData.size}")
            Triple(decodedData, updatedItem, if (dateRaw != 0) dateRaw else null)
        } catch (e: Exception) {
            Log.w(TAG, "Legacy format detection failed: ${e.message}")
            Triple(rawBytes, item, null)
        }
    }

    /**
     * Encode a Saturn date from a formatted string.
     */
    private fun encodeSaturnDate(saveDate: String): Int = try {
        val parts = saveDate.split(" ")
        val dateParts = parts.getOrElse(0) { "" }.split("/")
        val timeParts = if (parts.size > 1) parts[1].split(":") else listOf("0", "0", "0")

        val year = dateParts.getOrNull(0)?.toIntOrNull() ?: 1980
        val month = dateParts.getOrNull(1)?.toIntOrNull() ?: 1
        val day = dateParts.getOrNull(2)?.toIntOrNull() ?: 1
        val hour = timeParts.getOrNull(0)?.toIntOrNull() ?: 0
        val minute = timeParts.getOrNull(1)?.toIntOrNull() ?: 0

        val yearsSince1980 = (year - 1980).coerceAtLeast(0)
        val fourYearCycles = yearsSince1980 / 4
        val yearInCycle = yearsSince1980 % 4

        var totalDays = fourYearCycles * 0x5B5

        when (yearInCycle) {
            1 -> totalDays += 366
            2 -> totalDays += 366 + 365
            3 -> totalDays += 366 + 365 + 365
        }

        val isLeapYear = yearInCycle == 0
        val monthDays = intArrayOf(0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334)
        totalDays += monthDays[(month - 1).coerceIn(0, 11)]
        if (isLeapYear && month > 2) totalDays++

        totalDays += day
        totalDays * 0x5A0 + hour * 0x3C + minute
    } catch (e: Exception) {
        0
    }
}

/**
 * Data class representing storage status.
 */
data class StorageStatus(
    val totalSize: Int,
    val freeSize: Int,
    val usedSize: Int,
) {
    /**
     * Get formatted total size string.
     */
    val displayTotalSize: String
        get() = formatSize(totalSize)

    /**
     * Get formatted free size string.
     */
    val displayFreeSize: String
        get() = formatSize(freeSize)

    /**
     * Get formatted used size string.
     */
    val displayUsedSize: String
        get() = formatSize(usedSize)

    /**
     * Get usage percentage (0-100).
     */
    val usagePercent: Int
        get() = if (totalSize > 0) (usedSize * 100 / totalSize) else 0

    private fun formatSize(bytes: Int): String = when {
        bytes >= 1024 * 1024 -> String.format("%.1f MB", bytes / (1024.0 * 1024.0))
        bytes >= 1024 -> String.format("%.1f KB", bytes / 1024.0)
        else -> "$bytes B"
    }
}
