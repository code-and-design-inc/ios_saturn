/*
    Copyright 2024 devMiyax(smiyaxdev@gmail.com)

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
import android.net.Uri
import android.provider.OpenableColumns
import android.util.Log
import java.io.File
import java.io.FileOutputStream
import java.security.MessageDigest
import java.util.zip.ZipInputStream

/**
 * Manages BIOS file operations for the emulator.
 * Handles copying BIOS files from SAF URIs to app-private storage,
 * MD5 checksum validation, and preference key constants.
 */

/**
 * Data class representing a BIOS file with its metadata.
 */
data class BiosFileInfo(
    val filename: String,
    val file: File,
    val checksum: String,
    val isKnown: Boolean,
)

object BiosManager {
    private const val TAG = "BiosManager"
    private const val BIOS_DIR = "bios"

    // SharedPreferences keys
    const val KEY_BIOS_TYPE = "pref_bios_type"
    const val KEY_BIOS_FILENAME = "pref_bios_filename" // Display name
    const val KEY_BIOS_SELECTED_FILE = "pref_bios_selected" // Actual file in bios dir
    const val KEY_BIOS_CHECKSUM = "pref_bios_checksum"
    const val KEY_BIOS_VALID = "pref_bios_valid"

    // BIOS type values
    const val BIOS_TYPE_BUILTIN = "builtin"
    const val BIOS_TYPE_FILE = "file"

    // Known valid BIOS MD5 checksums
    private val KNOWN_CHECKSUMS =
        setOf(
            "85ec9ca47d8f6807718151cbcca8b964", // sega_101.bin
            "3240872c70984b6cbfda1586cab68dbe", // mpr-17933.bin
            "255113ba943c92a54facd25a10fd780c", // mpr-18811-mx.ic1
            "1cd19988d1d72a3e7caa0b73234c96b4", // mpr-19367-mx.ic1
            "af5828fdff51384f99b3c4926be27762", // saturn_bios.bin, sega_100.bin
            "3ea3202e2634cb47cb90f3a05c015010", // hisaturn.bin
            "cb2cebc1b6e573b7c44523d037edcd45", // mpr-18100.bin
            "f273555d7d91e8a5a6bfd9bcf066331c", // sega_100a.bin
            "ac4e4b6522e200c0d23d371a8cecbfd3", // vsaturn.bin
        )

    /**
     * Returns the app-private directory for BIOS files.
     * Creates the directory if it doesn't exist.
     */
    fun getBiosDir(context: Context): File = File(context.filesDir, BIOS_DIR).apply { mkdirs() }

    /**
     * Returns the currently selected BIOS file based on preferences.
     * @param context Application context
     * @return The selected BIOS file, or null if not set
     */
    fun getBiosFile(context: Context): File {
        val prefs = androidx.preference.PreferenceManager.getDefaultSharedPreferences(context)
        val selectedFile = prefs.getString(KEY_BIOS_SELECTED_FILE, null)
        return if (selectedFile != null) {
            File(getBiosDir(context), selectedFile)
        } else {
            // Fallback to first available BIOS file
            getBiosFiles(context).firstOrNull()?.file ?: File(getBiosDir(context), "user_bios.bin")
        }
    }

    /**
     * Returns list of all BIOS files in the app-private bios directory.
     */
    fun getBiosFiles(context: Context): List<BiosFileInfo> {
        val biosDir = getBiosDir(context)
        val files = biosDir.listFiles()?.filter { it.isFile } ?: emptyList()
        return files
            .map { file ->
                val checksum =
                    try {
                        calculateMD5(file)
                    } catch (e: Exception) {
                        ""
                    }
                BiosFileInfo(
                    filename = file.name,
                    file = file,
                    checksum = checksum,
                    isKnown = isKnownChecksum(checksum),
                )
            }.sortedBy { it.filename }
    }

    /**
     * Result class for BIOS import operation.
     */
    data class ImportResult(
        val success: Boolean,
        val filenames: List<String> = emptyList(),
        val error: String? = null,
    )

    /**
     * Copies a BIOS file from a SAF content URI to app-private storage.
     * If the file is a ZIP archive, extracts all files from it.
     * @param context Application context
     * @param uri Content URI from SAF file picker
     * @return ImportResult with list of imported filenames
     */
    fun copyBiosFromUri(
        context: Context,
        uri: Uri,
    ): String? {
        val result = importBiosFromUri(context, uri)
        return if (result.success && result.filenames.isNotEmpty()) {
            result.filenames.first()
        } else {
            null
        }
    }

    /**
     * Imports BIOS file(s) from a SAF content URI to app-private storage.
     * Supports both regular files and ZIP archives.
     * @param context Application context
     * @param uri Content URI from SAF file picker
     * @return ImportResult with list of imported filenames and their verification status
     */
    fun importBiosFromUri(
        context: Context,
        uri: Uri,
    ): ImportResult {
        val originalName = getFilenameFromUri(context, uri)

        return try {
            if (originalName.lowercase().endsWith(".zip")) {
                // Handle ZIP file
                extractZipBios(context, uri)
            } else {
                // Handle regular file
                val destFile = File(getBiosDir(context), originalName)
                context.contentResolver.openInputStream(uri)?.use { input ->
                    FileOutputStream(destFile).use { output ->
                        input.copyTo(output)
                    }
                }
                Log.d(TAG, "BIOS file copied successfully to ${destFile.absolutePath}")
                ImportResult(success = true, filenames = listOf(originalName))
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to import BIOS file", e)
            ImportResult(success = false, error = e.message)
        }
    }

    /**
     * Extracts BIOS files from a ZIP archive.
     * Only extracts files that could be BIOS files (by size and extension).
     * @param context Application context
     * @param uri Content URI of the ZIP file
     * @return ImportResult with list of extracted filenames
     */
    private fun extractZipBios(
        context: Context,
        uri: Uri,
    ): ImportResult {
        val extractedFiles = mutableListOf<String>()
        val biosDir = getBiosDir(context)

        context.contentResolver.openInputStream(uri)?.use { inputStream ->
            ZipInputStream(inputStream).use { zipInput ->
                var entry = zipInput.nextEntry
                while (entry != null) {
                    // Skip directories and hidden files
                    if (!entry.isDirectory && !entry.name.startsWith(".") && !entry.name.contains("/")) {
                        val filename = entry.name
                        // Extract files that look like BIOS files (reasonable size range: 256KB - 2MB)
                        val size = entry.size
                        if (size in 256 * 1024..2 * 1024 * 1024 || size == -1L) {
                            val destFile = File(biosDir, filename)
                            FileOutputStream(destFile).use { output ->
                                zipInput.copyTo(output)
                            }
                            Log.d(TAG, "Extracted BIOS file: $filename")
                            extractedFiles.add(filename)
                        } else {
                            Log.d(TAG, "Skipped file (size=$size): $filename")
                        }
                    } else if (entry.isDirectory) {
                        // Handle files inside directories - extract with just the filename
                        Log.d(TAG, "Skipped directory entry: ${entry.name}")
                    } else if (entry.name.contains("/")) {
                        // File inside a subdirectory - extract with just the base filename
                        val filename = entry.name.substringAfterLast("/")
                        if (!filename.startsWith(".")) {
                            val size = entry.size
                            if (size in 256 * 1024..2 * 1024 * 1024 || size == -1L) {
                                val destFile = File(biosDir, filename)
                                FileOutputStream(destFile).use { output ->
                                    zipInput.copyTo(output)
                                }
                                Log.d(TAG, "Extracted BIOS file from subdirectory: $filename")
                                extractedFiles.add(filename)
                            }
                        }
                    }
                    zipInput.closeEntry()
                    entry = zipInput.nextEntry
                }
            }
        }

        return if (extractedFiles.isNotEmpty()) {
            ImportResult(success = true, filenames = extractedFiles)
        } else {
            ImportResult(success = false, error = "No valid BIOS files found in ZIP")
        }
    }

    /**
     * Deletes a BIOS file from the app-private storage.
     */
    fun deleteBiosFile(
        context: Context,
        filename: String,
    ): Boolean {
        val file = File(getBiosDir(context), filename)
        return if (file.exists()) {
            file.delete()
        } else {
            false
        }
    }

    /**
     * Calculates the MD5 checksum of a file using streaming to handle large files.
     * @param file The file to calculate checksum for
     * @return MD5 checksum as lowercase hex string
     */
    fun calculateMD5(file: File): String {
        val md = MessageDigest.getInstance("MD5")
        file.inputStream().use { input ->
            val buffer = ByteArray(8192)
            var bytesRead: Int
            while (input.read(buffer).also { bytesRead = it } != -1) {
                md.update(buffer, 0, bytesRead)
            }
        }
        return md.digest().joinToString("") { "%02x".format(it) }
    }

    /**
     * Checks if the given MD5 checksum matches a known valid BIOS file.
     * @param checksum MD5 checksum to validate
     * @return true if checksum is in the known valid list
     */
    fun isKnownChecksum(checksum: String): Boolean = checksum.lowercase() in KNOWN_CHECKSUMS

    /**
     * Extracts the display filename from a content URI.
     * @param context Application context
     * @param uri Content URI to get filename from
     * @return Display name of the file, or "unknown" if unavailable
     */
    fun getFilenameFromUri(
        context: Context,
        uri: Uri,
    ): String {
        var name = "unknown"
        try {
            context.contentResolver.query(uri, null, null, null, null)?.use { cursor ->
                if (cursor.moveToFirst()) {
                    val idx = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                    if (idx >= 0) {
                        name = cursor.getString(idx) ?: "unknown"
                    }
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to get filename from URI", e)
        }
        return name
    }
}
