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
import android.util.Log
import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.storage.FirebaseStorage
import com.google.firebase.storage.StorageReference
import kotlinx.coroutines.tasks.await
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.text.DateFormat
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

/**
 * Manages attachments for bug reports including screenshots and save states.
 */
class ReportAttachmentManager(
    private val context: Context,
) {
    companion object {
        private const val TAG = "ReportAttachmentManager"
        private const val SCREENSHOT_FILENAME = "screenshot.png"
        private const val MAX_FILE_SIZE_BYTES = 20 * 1024 * 1024 // 10MB
    }

    /**
     * Result of attachment creation
     */
    data class AttachmentResult(
        val screenshotFile: File? = null,
        val stateSaveFile: File? = null,
        val memoryFile: File? = null,
        val totalSize: Long = 0,
        val success: Boolean = true,
        val errorMessage: String? = null,
    )

    /**
     * Result of file upload to Firebase Storage
     */
    data class UploadResult(
        val screenshotUrl: String? = null,
        val stateSaveUrl: String? = null,
        val memoryUrl: String? = null,
        val success: Boolean = true,
        val errorMessage: String? = null,
    )

    /**
     * Progress callback for upload operations
     */
    interface UploadProgressListener {
        fun onProgress(
            bytesTransferred: Long,
            totalBytes: Long,
        )

        fun onComplete()

        fun onError(error: Exception)
    }

    /**
     * Captures a screenshot of the current game screen
     *
     * @return File object pointing to the screenshot, or null if failed
     */
    fun captureScreenshot(): File? {
        try {
            val currentGameCode =
                YabauseRunnable.getCurrentGameCode() ?: run {
                    Log.w(TAG, "Cannot capture screenshot: no game code available")
                    return null
                }

            val screenshotPath = YabauseStorage.storage.screenshotPath
            val dateFormat: DateFormat = SimpleDateFormat("_yyyy_MM_dd_HH_mm_ss", Locale.getDefault())
            val date = Date()
            val filename = "$screenshotPath${currentGameCode}${dateFormat.format(date)}.png"

            val result = YabauseRunnable.screenshot(filename)
            if (result == 0) {
                val file = File(filename)
                if (file.exists() && file.length() > 0) {
                    Log.d(TAG, "Screenshot captured successfully: $filename (${file.length()} bytes)")
                    return file
                } else {
                    Log.e(TAG, "Screenshot file not found or empty: $filename")
                    return null
                }
            } else {
                Log.e(TAG, "Screenshot capture failed with code: $result")
                return null
            }
        } catch (e: Exception) {
            Log.e(TAG, "Exception while capturing screenshot", e)
            return null
        }
    }

    /**
     * Creates a save state file for the current game and compresses it
     *
     * @param compressed Whether to create a compressed save state (always true, parameter kept for compatibility)
     * @return File object pointing to the compressed save state, or null if failed
     */
    fun createStateSave(compressed: Boolean = true): File? {
        var tempFile: File? = null
        try {
            val currentGameCode =
                YabauseRunnable.getCurrentGameCode() ?: run {
                    Log.w(TAG, "Cannot create save state: no game code available")
                    return null
                }

            val savePath = YabauseStorage.storage.stateSavePath
            val saveRoot = File(savePath, currentGameCode)

            // Create directory if it doesn't exist
            if (!saveRoot.exists()) {
                if (!saveRoot.mkdirs()) {
                    Log.e(TAG, "Failed to create save state directory: ${saveRoot.absolutePath}")
                    return null
                }
            }

            val saveFilePath = "$savePath$currentGameCode"
            // Always use uncompressed savestate and compress manually
            val savedFile = YabauseRunnable.savestate(saveFilePath)

            if (savedFile == null) {
                Log.e(TAG, "Save state creation failed")
                return null
            }

            tempFile = File(savedFile)
            if (!tempFile.exists() || tempFile.length() == 0L) {
                Log.e(TAG, "Save state file not found or empty: $savedFile")
                return null
            }

            Log.d(TAG, "Save state created successfully: $savedFile (${tempFile.length()} bytes)")

            // Compress the save state file into a ZIP
            val dateFormat: DateFormat = SimpleDateFormat("_yyyy_MM_dd_HH_mm_ss", Locale.getDefault())
            val date = Date()
            val zipFileName = "${currentGameCode}${dateFormat.format(date)}.zip"
            val compressedFile = compressFiles(listOf(tempFile), zipFileName, savePath)

            // Clean up the temporary uncompressed file
            if (tempFile.exists()) {
                tempFile.delete()
                Log.d(TAG, "Cleaned up temporary uncompressed save state: $savedFile")
            }

            if (compressedFile != null && compressedFile.exists() && compressedFile.length() > 0) {
                Log.d(TAG, "Save state compressed successfully: ${compressedFile.absolutePath} (${compressedFile.length()} bytes)")
                return compressedFile
            } else {
                Log.e(TAG, "Save state compression failed")
                return null
            }
        } catch (e: Exception) {
            Log.e(TAG, "Exception while creating and compressing save state", e)
            // Clean up temporary file if it exists
            tempFile?.let {
                if (it.exists()) {
                    it.delete()
                }
            }
            return null
        }
    }

    /**
     * Copies the current memory (backup RAM) file for attachment and compresses it
     *
     * @return File object pointing to the compressed memory file, or null if failed
     */
    fun copyMemoryFile(): File? {
        var tempFile: File? = null
        try {
            val memoryPath = YabauseStorage.storage.getMemoryPath("memory.ram")
            val sourceFile = File(memoryPath)

            if (!sourceFile.exists()) {
                Log.w(TAG, "Memory file does not exist: $memoryPath")
                return null
            }

            if (sourceFile.length() == 0L) {
                Log.w(TAG, "Memory file is empty: $memoryPath")
                return null
            }

            // Create a temporary copy in the screenshots directory with timestamp
            val screenshotPath = YabauseStorage.storage.screenshotPath
            val dateFormat: DateFormat = SimpleDateFormat("_yyyy_MM_dd_HH_mm_ss", Locale.getDefault())
            val date = Date()
            val tempFileName = "${screenshotPath}memory${dateFormat.format(date)}.ram"
            tempFile = File(tempFileName)

            // Copy the file
            sourceFile.copyTo(tempFile, overwrite = true)

            if (!tempFile.exists() || tempFile.length() == 0L) {
                Log.e(TAG, "Memory file copy failed or is empty: $tempFileName")
                return null
            }

            Log.d(TAG, "Memory file copied successfully: $tempFileName (${tempFile.length()} bytes)")

            // Compress the copied file into a ZIP
            val zipFileName = "memory${dateFormat.format(date)}.zip"
            val compressedFile = compressFiles(listOf(tempFile), zipFileName)

            // Clean up the temporary uncompressed file
            if (tempFile.exists()) {
                tempFile.delete()
                Log.d(TAG, "Cleaned up temporary uncompressed file: $tempFileName")
            }

            if (compressedFile != null && compressedFile.exists() && compressedFile.length() > 0) {
                Log.d(TAG, "Memory file compressed successfully: ${compressedFile.absolutePath} (${compressedFile.length()} bytes)")
                return compressedFile
            } else {
                Log.e(TAG, "Memory file compression failed")
                return null
            }
        } catch (e: Exception) {
            Log.e(TAG, "Exception while copying and compressing memory file", e)
            // Clean up temporary file if it exists
            tempFile?.let {
                if (it.exists()) {
                    it.delete()
                }
            }
            return null
        }
    }

    /**
     * Creates attachments based on user preferences
     *
     * @param includeScreenshot Whether to include screenshot
     * @param includeStateSave Whether to include save state
     * @param includeMemory Whether to include memory (backup RAM) file
     * @param compressStateSave Whether to compress the save state
     * @return AttachmentResult containing the created files and metadata
     */
    fun createAttachments(
        includeScreenshot: Boolean,
        includeStateSave: Boolean,
        includeMemory: Boolean = false,
        compressStateSave: Boolean = true,
    ): AttachmentResult {
        var screenshotFile: File? = null
        var stateSaveFile: File? = null
        var memoryFile: File? = null
        var totalSize: Long = 0

        try {
            // Capture screenshot if requested
            if (includeScreenshot) {
                screenshotFile = captureScreenshot()
                if (screenshotFile != null) {
                    totalSize += screenshotFile.length()
                }
            }

            // Create save state if requested
            if (includeStateSave) {
                stateSaveFile = createStateSave(compressStateSave)
                if (stateSaveFile != null) {
                    totalSize += stateSaveFile.length()
                }
            }

            // Copy memory file if requested
            if (includeMemory) {
                memoryFile = copyMemoryFile()
                if (memoryFile != null) {
                    totalSize += memoryFile.length()
                }
            }

            // Check total file size
            if (totalSize > MAX_FILE_SIZE_BYTES) {
                Log.w(TAG, "Total attachment size ($totalSize bytes) exceeds maximum ($MAX_FILE_SIZE_BYTES bytes)")
                // Clean up created files
                cleanupFiles(screenshotFile, stateSaveFile, memoryFile)
                return AttachmentResult(
                    success = false,
                    errorMessage = "Total file size exceeds 10MB limit. Please try with compression enabled.",
                )
            }

            return AttachmentResult(
                screenshotFile = screenshotFile,
                stateSaveFile = stateSaveFile,
                memoryFile = memoryFile,
                totalSize = totalSize,
                success = true,
            )
        } catch (e: Exception) {
            Log.e(TAG, "Exception while creating attachments", e)
            // Clean up any files that were created
            cleanupFiles(screenshotFile, stateSaveFile, memoryFile)
            return AttachmentResult(
                success = false,
                errorMessage = "Failed to create attachments: ${e.message}",
            )
        }
    }

    /**
     * Calculates the estimated size of attachments before creating them
     * This is an approximation based on typical file sizes
     *
     * @param includeScreenshot Whether screenshot will be included
     * @param includeStateSave Whether save state will be included
     * @param includeMemory Whether memory file will be included
     * @return Estimated size in bytes
     */
    fun estimateAttachmentSize(
        includeScreenshot: Boolean,
        includeStateSave: Boolean,
        includeMemory: Boolean = false,
    ): Long {
        var estimatedSize: Long = 0

        // Screenshot is typically around 1-3MB for 1920x1080 PNG
        if (includeScreenshot) {
            estimatedSize += 132 * 1024 // 132kb
        }

        // Save state size varies greatly depending on game and compression
        // Compressed: ~500KB - 2MB, Uncompressed: ~2-5MB
        if (includeStateSave) {
            estimatedSize += (1.5 * 1024 * 1024).toInt() // 1MB estimate
        }

        // Memory file is typically 512KB (Saturn backup RAM size)
        if (includeMemory) {
            estimatedSize += 20 * 1024 // 512KB estimate
        }

        return estimatedSize
    }

    /**
     * Formats file size in human-readable format
     *
     * @param bytes Size in bytes
     * @return Formatted string (e.g., "2.5 MB")
     */
    fun formatFileSize(bytes: Long): String = when {
        bytes < 1024 -> "$bytes B"
        bytes < 1024 * 1024 -> String.format(Locale.getDefault(), "%.1f KB", bytes / 1024.0)
        else -> String.format(Locale.getDefault(), "%.1f MB", bytes / (1024.0 * 1024.0))
    }

    /**
     * Cleans up temporary attachment files
     *
     * @param files Variable number of files to delete
     */
    fun cleanupFiles(vararg files: File?) {
        files.filterNotNull().forEach { file ->
            try {
                if (file.exists()) {
                    if (file.delete()) {
                        Log.d(TAG, "Cleaned up file: ${file.absolutePath}")
                    } else {
                        Log.w(TAG, "Failed to delete file: ${file.absolutePath}")
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Exception while cleaning up file: ${file.absolutePath}", e)
            }
        }
    }

    /**
     * Validates if attachments can be created (e.g., game is running, sufficient storage)
     *
     * @return Pair of (canCreate, errorMessage)
     */
    fun validateAttachmentCreation(): Pair<Boolean, String?> {
        // Check if a game is running
        val currentGameCode = YabauseRunnable.getCurrentGameCode()
        if (currentGameCode == null) {
            return Pair(false, "No game is currently running")
        }

        // Check available storage space
        val screenshotPath = File(YabauseStorage.storage.screenshotPath)
        if (!screenshotPath.exists()) {
            screenshotPath.mkdirs()
        }

        val usableSpace = screenshotPath.usableSpace
        if (usableSpace < MAX_FILE_SIZE_BYTES * 2) {
            return Pair(false, "Insufficient storage space available")
        }

        return Pair(true, null)
    }

    /**
     * Compresses multiple files into a single ZIP file
     *
     * @param files List of files to compress
     * @param outputFileName Output ZIP filename (without path)
     * @param outputDir Output directory (defaults to screenshot path if null)
     * @return Compressed ZIP file, or null if failed
     */
    fun compressFiles(
        files: List<File>,
        outputFileName: String = "attachments.zip",
        outputDir: String? = null,
    ): File? {
        if (files.isEmpty()) {
            Log.w(TAG, "No files to compress")
            return null
        }

        val outputPath = outputDir ?: YabauseStorage.storage.screenshotPath
        val zipFile = File(outputPath, outputFileName)

        try {
            ZipOutputStream(FileOutputStream(zipFile)).use { zipOut ->
                for (file in files) {
                    if (!file.exists()) {
                        Log.w(TAG, "File does not exist, skipping: ${file.absolutePath}")
                        continue
                    }

                    FileInputStream(file).use { fileIn ->
                        val zipEntry = ZipEntry(file.name)
                        zipOut.putNextEntry(zipEntry)

                        val buffer = ByteArray(8192)
                        var bytesRead: Int
                        while (fileIn.read(buffer).also { bytesRead = it } != -1) {
                            zipOut.write(buffer, 0, bytesRead)
                        }

                        zipOut.closeEntry()
                        Log.d(TAG, "Added to ZIP: ${file.name}")
                    }
                }
            }

            Log.d(TAG, "Compression successful: ${zipFile.absolutePath} (${zipFile.length()} bytes)")
            return zipFile
        } catch (e: Exception) {
            Log.e(TAG, "Compression failed", e)
            // Clean up partial ZIP file
            if (zipFile.exists()) {
                zipFile.delete()
            }
            return null
        }
    }

    /**
     * Checks if compression would be beneficial (i.e., total size exceeds threshold)
     *
     * @param files List of files to check
     * @param threshold Size threshold in bytes (default: 5MB)
     * @return True if compression is recommended
     */
    fun shouldCompress(
        files: List<File>,
        threshold: Long = 5 * 1024 * 1024,
    ): Boolean {
        val totalSize = files.filter { it.exists() }.sumOf { it.length() }
        return totalSize > threshold
    }

    /**
     * Uploads a file to Firebase Storage
     *
     * @param file File to upload
     * @param remotePath Remote path in Firebase Storage (e.g., "reports/{userId}/{timestamp}/screenshot.png")
     * @param maxRetries Maximum number of retry attempts (default: 3)
     * @param progressListener Optional progress listener
     * @return Download URL if successful, null otherwise
     */
    suspend fun uploadFileToStorage(
        file: File,
        remotePath: String,
        maxRetries: Int = 3,
        progressListener: UploadProgressListener? = null,
    ): String? {
        if (!file.exists() || file.length() == 0L) {
            Log.e(TAG, "File does not exist or is empty: ${file.absolutePath}")
            progressListener?.onError(IllegalArgumentException("File does not exist or is empty"))
            return null
        }

        val storage = FirebaseStorage.getInstance()
        val storageRef: StorageReference = storage.reference.child(remotePath)

        var lastException: Exception? = null

        // Retry logic
        for (attempt in 1..maxRetries) {
            try {
                Log.d(TAG, "Uploading file (attempt $attempt/$maxRetries): ${file.absolutePath} -> $remotePath")

                val uploadTask = storageRef.putFile(android.net.Uri.fromFile(file))

                // Add progress listener if provided
                progressListener?.let { listener ->
                    uploadTask.addOnProgressListener { taskSnapshot ->
                        listener.onProgress(
                            taskSnapshot.bytesTransferred,
                            taskSnapshot.totalByteCount,
                        )
                    }
                }

                // Wait for upload to complete
                val uploadSnapshot = uploadTask.await()

                // Get download URL
                val downloadUrl = storageRef.downloadUrl.await()
                val urlString = downloadUrl.toString()

                Log.d(TAG, "File uploaded successfully: $urlString")
                progressListener?.onComplete()

                return urlString
            } catch (e: Exception) {
                lastException = e
                val errorMsg =
                    when {
                        e is java.net.UnknownHostException -> "Network error: Unable to reach Firebase Storage"
                        e is java.net.SocketTimeoutException -> "Upload timeout: Please check your internet connection"
                        e is java.io.IOException -> "Network error: ${e.message}"
                        e.message?.contains("storage quota", ignoreCase = true) == true -> "Storage quota exceeded"
                        e.message?.contains("permission", ignoreCase = true) == true -> "Permission denied: Check Firebase Storage rules"
                        else -> "Upload failed: ${e.message}"
                    }
                Log.e(TAG, "Upload attempt $attempt failed: $errorMsg", e)

                if (attempt == maxRetries) {
                    Log.e(TAG, "All upload attempts failed for: ${file.absolutePath}")
                    progressListener?.onError(Exception(errorMsg, e))
                } else {
                    // Wait before retrying (exponential backoff)
                    val delayMs = 1000L * attempt
                    Log.d(TAG, "Retrying upload in ${delayMs}ms...")
                    kotlinx.coroutines.delay(delayMs)
                }
            }
        }

        return null
    }

    /**
     * Checks if device has internet connectivity
     */
    private fun hasInternetConnection(): Boolean {
        val connectivityManager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as android.net.ConnectivityManager
        val network = connectivityManager.activeNetwork ?: return false
        val capabilities = connectivityManager.getNetworkCapabilities(network) ?: return false
        return capabilities.hasCapability(android.net.NetworkCapabilities.NET_CAPABILITY_INTERNET) &&
            capabilities.hasCapability(android.net.NetworkCapabilities.NET_CAPABILITY_VALIDATED)
    }

    /**
     * Uploads attachment files to Firebase Storage
     *
     * @param screenshotFile Screenshot file to upload (optional)
     * @param stateSaveFile Save state file to upload (optional)
     * @param memoryFile Memory (backup RAM) file to upload (optional)
     * @param progressListener Optional progress listener
     * @return UploadResult containing download URLs
     */
    suspend fun uploadAttachments(
        screenshotFile: File?,
        stateSaveFile: File?,
        memoryFile: File? = null,
        progressListener: UploadProgressListener? = null,
    ): UploadResult {
        try {
            // Check internet connection
            if (!hasInternetConnection()) {
                return UploadResult(
                    success = false,
                    errorMessage = "No internet connection. Please check your network and try again.",
                )
            }

            // Check authentication
            val currentUser = FirebaseAuth.getInstance().currentUser
            if (currentUser == null) {
                return UploadResult(
                    success = false,
                    errorMessage = "User not authenticated. Please sign in and try again.",
                )
            }

            val userId = currentUser.uid
            val timestamp = System.currentTimeMillis()

            var screenshotUrl: String? = null
            var stateSaveUrl: String? = null
            var memoryUrl: String? = null

            // Upload screenshot if provided
            if (screenshotFile != null && screenshotFile.exists()) {
                val screenshotPath = "reports/$userId/$timestamp/screenshot.png"
                screenshotUrl = uploadFileToStorage(screenshotFile, screenshotPath, progressListener = progressListener)

                if (screenshotUrl == null) {
                    return UploadResult(
                        success = false,
                        errorMessage = "Failed to upload screenshot",
                    )
                }
            }

            // Upload save state if provided
            if (stateSaveFile != null && stateSaveFile.exists()) {
                val extension = stateSaveFile.extension.ifEmpty { "yss" }
                val stateSavePath = "reports/$userId/$timestamp/savestate.$extension"
                stateSaveUrl = uploadFileToStorage(stateSaveFile, stateSavePath, progressListener = progressListener)

                if (stateSaveUrl == null) {
                    return UploadResult(
                        success = false,
                        errorMessage = "Failed to upload save state",
                    )
                }
            }

            // Upload memory file if provided
            if (memoryFile != null && memoryFile.exists()) {
                val extension = memoryFile.extension.ifEmpty { "ram" }
                val memoryPath = "reports/$userId/$timestamp/memory.$extension"
                memoryUrl = uploadFileToStorage(memoryFile, memoryPath, progressListener = progressListener)

                if (memoryUrl == null) {
                    return UploadResult(
                        success = false,
                        errorMessage = "Failed to upload memory file",
                    )
                }
            }

            return UploadResult(
                screenshotUrl = screenshotUrl,
                stateSaveUrl = stateSaveUrl,
                memoryUrl = memoryUrl,
                success = true,
            )
        } catch (e: Exception) {
            val errorMsg =
                when {
                    e is java.net.UnknownHostException -> "Network error: Unable to reach server. Please check your internet connection."
                    e is java.net.SocketTimeoutException -> "Upload timeout: The connection is too slow or unstable."
                    e is java.io.IOException -> "Network I/O error: ${e.message}"
                    e is com.google.firebase.FirebaseNetworkException -> "Firebase network error: Please check your connection."
                    e is com.google.firebase.FirebaseException -> "Firebase error: ${e.message}"
                    else -> "Upload failed: ${e.message ?: "Unknown error"}"
                }
            Log.e(TAG, "Exception during upload: $errorMsg", e)
            progressListener?.onError(Exception(errorMsg, e))
            return UploadResult(
                success = false,
                errorMessage = errorMsg,
            )
        }
    }
}
