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
package org.uoyabause.android.game

import android.net.Uri

data class ScanFolderItem(
    val path: String,
    val fileCount: Int = 0,
    val lastScanTimestamp: Long = 0L,
) {
    val displayName: String
        get() = extractDisplayName(path)

    fun getRelativeTimeText(currentTimeMillis: Long = System.currentTimeMillis()): String {
        if (lastScanTimestamp <= 0L) return ""
        val diffMs = currentTimeMillis - lastScanTimestamp
        if (diffMs < 0) return ""
        val diffMinutes = diffMs / (1000 * 60)
        val diffHours = diffMs / (1000 * 60 * 60)
        val diffDays = diffMs / (1000 * 60 * 60 * 24)
        return when {
            diffMinutes < 1 -> "just now"
            diffMinutes < 60 -> "${diffMinutes}m ago"
            diffHours < 24 -> "${diffHours}h ago"
            else -> "${diffDays}d ago"
        }
    }

    companion object {
        fun extractDisplayName(path: String): String {
            if (path.isBlank()) return ""
            return try {
                val uri = Uri.parse(path)
                val lastSegment = uri.lastPathSegment ?: return path
                // Content URIs often have paths like "primary:Saturn Games"
                // or "primary%3ASaturn%20Games"
                val decoded = Uri.decode(lastSegment)
                val colonIndex = decoded.lastIndexOf(':')
                if (colonIndex >= 0 && colonIndex < decoded.length - 1) {
                    decoded.substring(colonIndex + 1)
                } else {
                    val slashIndex = decoded.lastIndexOf('/')
                    if (slashIndex >= 0 && slashIndex < decoded.length - 1) {
                        decoded.substring(slashIndex + 1)
                    } else {
                        decoded
                    }
                }
            } catch (e: Exception) {
                path
            }
        }
    }
}
