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
package org.uoyabause.android.util

import android.content.Context
import android.net.Uri
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileInputStream
import java.io.InputStream
import java.security.MessageDigest

/**
 * Utility object for calculating file hashes.
 * Provides SHA256 calculation for files and content URIs.
 */
object FileHashUtil {
    private const val BUFFER_SIZE = 8192
    private val HEX_CHARS = "0123456789ABCDEF"

    /**
     * Calculate SHA256 hash of a file
     * @param file The file to calculate hash for
     * @return The SHA256 hash as a hex string
     */
    suspend fun calculateSHA256(file: File): String =
        withContext(Dispatchers.IO) {
            FileInputStream(file).use { inputStream ->
                calculateSHA256FromStream(inputStream)
            }
        }

    /**
     * Calculate SHA256 hash for a content URI
     * @param context Context for content resolver
     * @param uri The content URI to calculate hash for
     * @return The SHA256 hash as a hex string
     * @throws IllegalArgumentException if the URI cannot be opened
     */
    suspend fun calculateSHA256(
        context: Context,
        uri: Uri,
    ): String =
        withContext(Dispatchers.IO) {
            context.contentResolver.openInputStream(uri)?.use { inputStream ->
                calculateSHA256FromStream(inputStream)
            } ?: throw IllegalArgumentException("Cannot open input stream for URI: $uri")
        }

    /**
     * Calculate SHA256 hash from an input stream
     * @param inputStream The input stream to read from
     * @return The SHA256 hash as a hex string
     */
    private fun calculateSHA256FromStream(inputStream: InputStream): String {
        val md = MessageDigest.getInstance("SHA-256")
        val buffer = ByteArray(BUFFER_SIZE)
        var bytesRead: Int

        while (inputStream.read(buffer).also { bytesRead = it } != -1) {
            md.update(buffer, 0, bytesRead)
        }

        return bytesToHex(md.digest())
    }

    /**
     * Convert a byte array to a hexadecimal string
     * @param bytes The byte array to convert
     * @return The hexadecimal string representation
     */
    private fun bytesToHex(bytes: ByteArray): String {
        val hexChars = CharArray(bytes.size * 2)
        for (i in bytes.indices) {
            val v = bytes[i].toInt() and 0xFF
            hexChars[i * 2] = HEX_CHARS[v ushr 4]
            hexChars[i * 2 + 1] = HEX_CHARS[v and 0x0F]
        }
        return String(hexChars)
    }
}
