/*  Copyright 2025 devMiyax(smiyaxdev@gmail.com)

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

import android.content.Context
import android.content.Intent
import android.util.Log
import com.google.firebase.storage.FirebaseStorage
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.tasks.await
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.util.zip.ZipInputStream

/**
 * Manager class for reproducing reported issues by downloading and setting up
 * savestate and memory files from Firebase Storage
 */
class ReportReproduceManager(
    private val context: Context,
) {
    companion object {
        private const val TAG = "ReportReproduceManager"
        private const val REPRODUCE_DIR = "reproduce"
        private const val MAX_DOWNLOAD_SIZE_BYTES = 50 * 1024 * 1024 // 50MB

        // Preference keys that need to be saved and restored for reproduction
        private val REPRODUCTION_PREFERENCE_KEYS =
            listOf(
                "pref_bios",
                "pref_cart",
                "pref_cpu",
                "pref_use_cpu_affinity",
                "pref_use_sh2_cache",
                "pref_video",
                "pref_frameskip",
                "pref_landscape",
                "pref_rotate_screen",
                "pref_frameLimit",
                "pref_filter",
                "pref_polygon_generation",
                "pref_resolution",
                "pref_use_compute_shader",
                "pref_fps",
                "pref_aspect_rate",
                "pref_rbg_resolution",
                "pref_sound_engine",
                "scsp_time_sync_mode",
                "pref_scsp_sync_per_frame",
            )

        // Default values for each preference (from res/xml/preferences.xml)
        private val PREFERENCE_DEFAULT_VALUES =
            mapOf(
                "pref_bios" to "",
                "pref_cart" to "7",
                "pref_cpu" to "3",
                "pref_use_cpu_affinity" to "false",
                "pref_use_sh2_cache" to "true",
                "pref_video" to "-1",
                "pref_frameskip" to "true",
                "pref_landscape" to "false",
                "pref_rotate_screen" to "false",
                "pref_frameLimit" to "0",
                "pref_filter" to "0",
                "pref_polygon_generation" to "0",
                "pref_resolution" to "0",
                "pref_use_compute_shader" to "false",
                "pref_fps" to "false",
                "pref_aspect_rate" to "0",
                "pref_rbg_resolution" to "0",
                "pref_sound_engine" to "1",
                "scsp_time_sync_mode" to "0",
                "pref_scsp_sync_per_frame" to "4",
            )

        // Preference keys that are stored as Boolean in SharedPreferences
        private val BOOLEAN_PREFERENCE_KEYS =
            setOf(
                "pref_use_cpu_affinity",
                "pref_use_sh2_cache",
                "pref_frameskip",
                "pref_landscape",
                "pref_rotate_screen",
                "pref_use_compute_shader",
                "pref_fps",
            )

        // Preference keys that are stored as Int in SharedPreferences
        // (e.g. SeekBarPreference). Map<String, String> values use the
        // decimal string representation on the wire.
        private val INT_PREFERENCE_KEYS =
            setOf(
                "pref_scsp_sync_per_frame",
            )
    }

    /**
     * Result of reproduction setup
     */
    data class ReproduceResult(
        val success: Boolean = false,
        val errorMessage: String? = null,
        val savestateFile: File? = null,
        val memoryFile: File? = null,
        val screenshotFile: File? = null,
        val preferences: Map<String, String>? = null,
    )

    /**
     * Progress callback for download operations
     */
    interface DownloadProgressListener {
        fun onProgress(
            bytesTransferred: Long,
            totalBytes: Long,
        )

        fun onComplete()

        fun onError(error: Exception)
    }

    /**
     * Downloads and prepares all necessary files to reproduce a reported issue
     *
     * @param reportData The report data containing URLs to attachments
     * @param productNumber The product number of the game
     * @param progressListener Optional progress listener
     * @return ReproduceResult containing file paths and success status
     */
    suspend fun prepareReproduction(
        reportData: ReportData,
        productNumber: String,
        progressListener: DownloadProgressListener? = null,
    ): ReproduceResult {
        return withContext(Dispatchers.IO) {
            try {
                // Validate report has reproducible data
                if (!reportData.isReproducible()) {
                    return@withContext ReproduceResult(
                        success = false,
                        errorMessage = "This report does not have savestate or memory data",
                    )
                }

                // Check available storage space
                val reproduceDir = getReproduceDirectory()
                val availableSpace = reproduceDir.usableSpace
                if (availableSpace < MAX_DOWNLOAD_SIZE_BYTES) {
                    return@withContext ReproduceResult(
                        success = false,
                        errorMessage = "Insufficient storage space (need at least 50MB)",
                    )
                }

                // Create reproduce directory for this report
                val reportDir = File(reproduceDir, reportData.id)
                if (!reportDir.exists() && !reportDir.mkdirs()) {
                    return@withContext ReproduceResult(
                        success = false,
                        errorMessage = "Failed to create reproduce directory",
                    )
                }

                var savestateFile: File? = null
                var memoryFile: File? = null
                var screenshotFile: File? = null

                // Download savestate if available
                if (!reportData.savestate_url.isNullOrEmpty()) {
                    val savestateZip = File(reportDir, "savestate.zip")
                    val downloadSuccess =
                        downloadFileFromUrl(
                            reportData.savestate_url,
                            savestateZip,
                            progressListener,
                        )

                    if (downloadSuccess) {
                        // Extract ZIP file
                        val extractedFiles = extractZipFile(savestateZip, reportDir)
                        if (extractedFiles.isNotEmpty()) {
                            // Find the savestate file (usually .yss extension)
                            savestateFile = extractedFiles.firstOrNull {
                                it.extension == "yss" || it.extension == "ysz"
                            } ?: extractedFiles.first()

                            // Move to proper location for loading
                            val targetSavestatePath = YabauseStorage.storage.stateSavePath
                            val targetSavestateDir = File(targetSavestatePath, productNumber)
                            if (!targetSavestateDir.exists()) {
                                targetSavestateDir.mkdirs()
                            }

                            val targetFile = File(targetSavestateDir, "reproduce_${reportData.id}.yss")
                            savestateFile.copyTo(targetFile, overwrite = true)
                            savestateFile = targetFile

                            Log.d(TAG, "Savestate prepared: ${savestateFile.absolutePath}")
                        }

                        // Clean up ZIP
                        savestateZip.delete()
                    } else {
                        return@withContext ReproduceResult(
                            success = false,
                            errorMessage = "Failed to download savestate",
                        )
                    }
                }

                // Download memory if available
                if (!reportData.memory_url.isNullOrEmpty()) {
                    val memoryZip = File(reportDir, "memory.zip")
                    val downloadSuccess =
                        downloadFileFromUrl(
                            reportData.memory_url,
                            memoryZip,
                            progressListener,
                        )

                    if (downloadSuccess) {
                        // Extract ZIP file
                        val extractedFiles = extractZipFile(memoryZip, reportDir)
                        if (extractedFiles.isNotEmpty()) {
                            // Find the memory file (usually .ram extension)
                            memoryFile = extractedFiles.firstOrNull {
                                it.extension == "ram"
                            } ?: extractedFiles.first()

                            // Keep the downloaded memory file as-is, don't copy to memory location
                            // It will be passed via intent to Yabause Activity
                            Log.d(TAG, "Memory prepared: ${memoryFile.absolutePath}")
                        }

                        // Clean up ZIP
                        memoryZip.delete()
                    } else {
                        Log.w(TAG, "Failed to download memory file, continuing without it")
                    }
                }

                // Download screenshot if available (optional, for reference)
                if (!reportData.screenshot_url.isNullOrEmpty()) {
                    val screenshotFileTemp = File(reportDir, "screenshot.png")
                    val downloadSuccess =
                        downloadFileFromUrl(
                            reportData.screenshot_url,
                            screenshotFileTemp,
                            null, // No progress for screenshot
                        )

                    if (downloadSuccess) {
                        screenshotFile = screenshotFileTemp
                        Log.d(TAG, "Screenshot downloaded: ${screenshotFile.absolutePath}")
                    }
                }

                progressListener?.onComplete()

                return@withContext ReproduceResult(
                    success = true,
                    savestateFile = savestateFile,
                    memoryFile = memoryFile,
                    screenshotFile = screenshotFile,
                    preferences = reportData.preferences,
                )
            } catch (e: Exception) {
                Log.e(TAG, "Error preparing reproduction", e)
                progressListener?.onError(e)
                return@withContext ReproduceResult(
                    success = false,
                    errorMessage = "Error: ${e.message}",
                )
            }
        }
    }

    /**
     * Downloads a file from Firebase Storage URL
     *
     * @param url Download URL from Firebase Storage
     * @param targetFile Local file to save to
     * @param progressListener Optional progress listener
     * @return true if download was successful
     */
    private suspend fun downloadFileFromUrl(
        url: String,
        targetFile: File,
        progressListener: DownloadProgressListener?,
    ): Boolean = try {
        val storage = FirebaseStorage.getInstance()
        val storageRef = storage.getReferenceFromUrl(url)

        // Add progress listener if provided
        val downloadTask = storageRef.getFile(targetFile)

        progressListener?.let { listener ->
            downloadTask.addOnProgressListener { taskSnapshot ->
                listener.onProgress(
                    taskSnapshot.bytesTransferred,
                    taskSnapshot.totalByteCount,
                )
            }
        }

        // Wait for download to complete
        downloadTask.await()

        // Verify file was downloaded
        if (targetFile.exists() && targetFile.length() > 0) {
            Log.d(TAG, "Downloaded: ${targetFile.absolutePath} (${targetFile.length()} bytes)")
            true
        } else {
            Log.e(TAG, "Downloaded file is empty or doesn't exist")
            false
        }
    } catch (e: Exception) {
        Log.e(TAG, "Download failed for $url", e)
        progressListener?.onError(e)
        false
    }

    /**
     * Extracts a ZIP file to the specified directory
     *
     * @param zipFile The ZIP file to extract
     * @param targetDir The directory to extract to
     * @return List of extracted files
     */
    private fun extractZipFile(
        zipFile: File,
        targetDir: File,
    ): List<File> {
        val extractedFiles = mutableListOf<File>()

        try {
            if (!targetDir.exists()) {
                targetDir.mkdirs()
            }

            ZipInputStream(FileInputStream(zipFile)).use { zipInput ->
                var entry = zipInput.nextEntry

                while (entry != null) {
                    val entryFile = File(targetDir, entry.name)

                    // Security check: prevent path traversal attacks
                    if (!entryFile.canonicalPath.startsWith(targetDir.canonicalPath)) {
                        Log.w(TAG, "Skipping entry with suspicious path: ${entry.name}")
                        entry = zipInput.nextEntry
                        continue
                    }

                    if (entry.isDirectory) {
                        entryFile.mkdirs()
                    } else {
                        // Create parent directories if needed
                        entryFile.parentFile?.mkdirs()

                        // Extract file
                        FileOutputStream(entryFile).use { output ->
                            val buffer = ByteArray(8192)
                            var bytesRead: Int
                            while (zipInput.read(buffer).also { bytesRead = it } != -1) {
                                output.write(buffer, 0, bytesRead)
                            }
                        }

                        extractedFiles.add(entryFile)
                        Log.d(TAG, "Extracted: ${entryFile.name} (${entryFile.length()} bytes)")
                    }

                    zipInput.closeEntry()
                    entry = zipInput.nextEntry
                }
            }

            return extractedFiles
        } catch (e: Exception) {
            Log.e(TAG, "Failed to extract ZIP file: ${zipFile.absolutePath}", e)
            return emptyList()
        }
    }

    /**
     * Collects current preferences that are needed for reproduction
     * Uses default values from preferences.xml if value is not set
     *
     * @param gameCode Game code to collect game-specific preferences
     * @return Map of preference keys to values (all values stored as strings)
     */
    fun collectCurrentPreferences(gameCode: String?): Map<String, String> {
        val preferences = mutableMapOf<String, String>()

        try {
            // Get game-specific preferences if gameCode is provided
            val sharedPref =
                if (gameCode != null) {
                    context.getSharedPreferences(gameCode, Context.MODE_PRIVATE)
                } else {
                    androidx.preference.PreferenceManager.getDefaultSharedPreferences(context)
                }

            REPRODUCTION_PREFERENCE_KEYS.forEach { key ->
                val value =
                    when {
                        BOOLEAN_PREFERENCE_KEYS.contains(key) -> {
                            val defaultValue = PREFERENCE_DEFAULT_VALUES[key]?.toBoolean() ?: false
                            sharedPref.getBoolean(key, defaultValue).toString()
                        }
                        INT_PREFERENCE_KEYS.contains(key) -> {
                            val defaultValue = PREFERENCE_DEFAULT_VALUES[key]?.toIntOrNull() ?: 0
                            sharedPref.getInt(key, defaultValue).toString()
                        }
                        else -> {
                            sharedPref.getString(key, null) ?: PREFERENCE_DEFAULT_VALUES[key]
                        }
                    }

                if (value != null) {
                    preferences[key] = value
                }
            }

            Log.d(TAG, "Collected ${preferences.size} preferences for reproduction (gameCode: $gameCode, with defaults)")
        } catch (e: Exception) {
            Log.e(TAG, "Error collecting preferences", e)
        }

        return preferences
    }

    /**
     * Applies reproduction preferences temporarily
     * Returns a map of original preferences for restoration
     *
     * @param gameCode Game code to apply game-specific preferences
     * @param preferences Preferences to apply (values as strings)
     * @return Map of original preferences (for restoration, values as strings)
     */
    fun applyReproductionPreferences(
        gameCode: String?,
        preferences: Map<String, String>?,
    ): Map<String, String> {
        val originalPreferences = mutableMapOf<String, String>()

        if (preferences == null) {
            return originalPreferences
        }

        try {
            // Get game-specific preferences if gameCode is provided
            val sharedPref =
                if (gameCode != null) {
                    context.getSharedPreferences(gameCode, Context.MODE_PRIVATE)
                } else {
                    androidx.preference.PreferenceManager.getDefaultSharedPreferences(context)
                }
            val editor = sharedPref.edit()

            // Save original values and apply new ones
            preferences.forEach { (key, value) ->
                when {
                    BOOLEAN_PREFERENCE_KEYS.contains(key) -> {
                        val defaultValue = PREFERENCE_DEFAULT_VALUES[key]?.toBoolean() ?: false
                        val originalBoolValue = sharedPref.getBoolean(key, defaultValue)
                        originalPreferences[key] = originalBoolValue.toString()

                        val newBoolValue = value.toBoolean()
                        editor.putBoolean(key, newBoolValue)
                        Log.d(TAG, "Applied Boolean preference: $key = $newBoolValue")
                    }
                    INT_PREFERENCE_KEYS.contains(key) -> {
                        val defaultValue = PREFERENCE_DEFAULT_VALUES[key]?.toIntOrNull() ?: 0
                        val originalIntValue = sharedPref.getInt(key, defaultValue)
                        originalPreferences[key] = originalIntValue.toString()

                        val newIntValue = value.toIntOrNull() ?: defaultValue
                        editor.putInt(key, newIntValue)
                        Log.d(TAG, "Applied Int preference: $key = $newIntValue")
                    }
                    else -> {
                        val originalValue = sharedPref.getString(key, null)
                        if (originalValue != null) {
                            originalPreferences[key] = originalValue
                        }

                        editor.putString(key, value)
                        Log.d(TAG, "Applied String preference: $key = $value")
                    }
                }
            }

            editor.apply()
            Log.d(TAG, "Applied ${preferences.size} reproduction preferences (gameCode: $gameCode)")
        } catch (e: Exception) {
            Log.e(TAG, "Error applying reproduction preferences", e)
        }

        return originalPreferences
    }

    /**
     * Restores original preferences after reproduction
     *
     * @param gameCode Game code to restore game-specific preferences
     * @param originalPreferences Original preferences to restore (values as strings)
     */
    fun restoreOriginalPreferences(
        gameCode: String?,
        originalPreferences: Map<String, String>,
    ) {
        if (originalPreferences.isEmpty()) {
            return
        }

        try {
            // Get game-specific preferences if gameCode is provided
            val sharedPref =
                if (gameCode != null) {
                    context.getSharedPreferences(gameCode, Context.MODE_PRIVATE)
                } else {
                    androidx.preference.PreferenceManager.getDefaultSharedPreferences(context)
                }
            val editor = sharedPref.edit()

            originalPreferences.forEach { (key, value) ->
                when {
                    BOOLEAN_PREFERENCE_KEYS.contains(key) -> {
                        val boolValue = value.toBoolean()
                        editor.putBoolean(key, boolValue)
                        Log.d(TAG, "Restored Boolean preference: $key = $boolValue")
                    }
                    INT_PREFERENCE_KEYS.contains(key) -> {
                        val intValue = value.toIntOrNull()
                            ?: PREFERENCE_DEFAULT_VALUES[key]?.toIntOrNull()
                            ?: 0
                        editor.putInt(key, intValue)
                        Log.d(TAG, "Restored Int preference: $key = $intValue")
                    }
                    else -> {
                        editor.putString(key, value)
                        Log.d(TAG, "Restored String preference: $key = $value")
                    }
                }
            }

            editor.apply()
            Log.d(TAG, "Restored ${originalPreferences.size} original preferences (gameCode: $gameCode)")
        } catch (e: Exception) {
            Log.e(TAG, "Error restoring original preferences", e)
        }
    }

    /**
     * Creates an Intent to launch Yabause with the reproduced state
     *
     * @param gameInfo The game info
     * @param savestateFile Optional savestate file to load
     * @param memoryFile Optional memory file to use
     * @return Intent to launch Yabause
     */
    fun createLaunchIntent(
        gameInfo: GameInfo,
        savestateFile: File?,
        memoryFile: File? = null,
    ): Intent {
        val intent =
            Intent(context, Yabause::class.java).apply {
                action = Intent.ACTION_VIEW

                // Set game path
                if (gameInfo.file_path.contains("content://")) {
                    putExtra("org.uoyabause.android.FileNameUri", gameInfo.file_path)
                    putExtra("org.uoyabause.android.FileDir", gameInfo.iso_file_path)
                } else {
                    putExtra("org.uoyabause.android.FileNameEx", gameInfo.file_path)
                }

                putExtra("org.uoyabause.android.gamecode", gameInfo.product_number)

                // Set savestate to load if available
                if (savestateFile != null && savestateFile.exists()) {
                    putExtra("org.uoyabause.android.LoadState", savestateFile.absolutePath)
                }

                // Set temporary memory file if available
                if (memoryFile != null && memoryFile.exists()) {
                    putExtra("org.uoyabause.android.tmpbackupfile", memoryFile.absolutePath)
                }

                flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP
            }

        return intent
    }

    /**
     * Gets or creates the reproduce directory
     */
    private fun getReproduceDirectory(): File {
        val baseDir = YabauseStorage.storage.screenshotPath
        val reproduceDir = File(baseDir, REPRODUCE_DIR)

        if (!reproduceDir.exists()) {
            reproduceDir.mkdirs()
        }

        return reproduceDir
    }

    /**
     * Cleans up old reproduction files
     *
     * @param olderThanDays Delete files older than this many days (default: 7)
     */
    fun cleanupOldReproductions(olderThanDays: Int = 7) {
        try {
            val reproduceDir = getReproduceDirectory()
            if (!reproduceDir.exists()) return

            val cutoffTime = System.currentTimeMillis() - (olderThanDays * 24 * 60 * 60 * 1000L)

            reproduceDir.listFiles()?.forEach { file ->
                if (file.lastModified() < cutoffTime) {
                    if (file.isDirectory) {
                        file.deleteRecursively()
                    } else {
                        file.delete()
                    }
                    Log.d(TAG, "Cleaned up old reproduction: ${file.name}")
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error cleaning up old reproductions", e)
        }
    }

    /**
     * Gets the total size of all reproduction files
     */
    fun getReproductionCacheSize(): Long {
        return try {
            val reproduceDir = getReproduceDirectory()
            if (!reproduceDir.exists()) return 0L

            var totalSize = 0L
            reproduceDir.walk().forEach { file ->
                if (file.isFile) {
                    totalSize += file.length()
                }
            }
            totalSize
        } catch (e: Exception) {
            Log.e(TAG, "Error calculating cache size", e)
            0L
        }
    }

    /**
     * Deletes all reproduction files
     */
    fun clearAllReproductions() {
        try {
            val reproduceDir = getReproduceDirectory()
            if (reproduceDir.exists()) {
                reproduceDir.deleteRecursively()
                reproduceDir.mkdirs()
                Log.d(TAG, "Cleared all reproduction files")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error clearing reproductions", e)
        }
    }
}
