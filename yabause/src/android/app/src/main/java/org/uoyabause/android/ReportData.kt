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

import com.google.firebase.Timestamp
import com.google.firebase.firestore.DocumentId
import java.text.SimpleDateFormat
import java.util.Locale

/**
 * Data class representing a user report from Firestore
 */
data class ReportData(
    @DocumentId
    val id: String = "",
    val uid: String = "",
    val display_name: String? = null,
    val photo_url: String? = null,
    val rating: Int = 0,
    val emulation_rating: Int = 0,
    val comment: String = "",
    val platform: String = "",
    val version: String = "",
    val version_code: Int = 0,
    val device_model: String = "",
    val os_version: String = "",
    val gpu_renderer: String = "",
    val timestamp: Timestamp? = null,
    val isVisible: Boolean = true,
    val anonymous: Boolean = false,
    val has_attachments: Boolean = false,
    val screenshot_url: String? = null,
    val savestate_url: String? = null,
    val memory_url: String? = null,
    val attachment_size: Long = 0,
    val preferences: Map<String, String>? = null,
    val game_title: String? = null,
    val product_number: String? = null,
    val game_id: String? = null,
) {
    /**
     * Get formatted timestamp string
     */
    fun getFormattedTimestamp(): String = if (timestamp != null) {
        val date = timestamp.toDate()
        val formatter = SimpleDateFormat("yyyy/MM/dd HH:mm", Locale.getDefault())
        formatter.format(date)
    } else {
        "Unknown"
    }

    /**
     * Get rating stars string
     */
    fun getRatingStars(): String = "★".repeat(emulation_rating)

    /**
     * Check if report has any attachments
     */
    fun hasAnyAttachments(): Boolean = !screenshot_url.isNullOrEmpty() ||
        !savestate_url.isNullOrEmpty() ||
        !memory_url.isNullOrEmpty()

    /**
     * Get formatted attachment size
     */
    fun getFormattedAttachmentSize(): String = when {
        attachment_size < 1024 -> "$attachment_size B"
        attachment_size < 1024 * 1024 -> String.format(Locale.getDefault(), "%.1f KB", attachment_size / 1024.0)
        else -> String.format(Locale.getDefault(), "%.1f MB", attachment_size / (1024.0 * 1024.0))
    }

    /**
     * Check if report has reproducible data (savestate and/or memory)
     */
    fun isReproducible(): Boolean = !savestate_url.isNullOrEmpty() || !memory_url.isNullOrEmpty()
}
