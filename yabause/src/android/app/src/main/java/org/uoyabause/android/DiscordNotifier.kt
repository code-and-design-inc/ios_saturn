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
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaTypeOrNull
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.devmiyax.yabasanshiro.R
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.TimeUnit

/**
 * Utility class for sending notifications to Discord via Webhook
 */
class DiscordNotifier(
    private val context: Context,
) {
    private val client: OkHttpClient =
        OkHttpClient
            .Builder()
            .connectTimeout(10, TimeUnit.SECONDS)
            .writeTimeout(10, TimeUnit.SECONDS)
            .readTimeout(30, TimeUnit.SECONDS)
            .build()

    /**
     * Send a report notification to Discord
     *
     * @param reportData The report data containing user info, ratings, etc.
     * @return true if notification was sent successfully, false otherwise
     */
    suspend fun sendReportNotification(reportData: ReportData): Boolean {
        return withContext(Dispatchers.IO) {
            try {
                val webhookUrl = context.getString(R.string.discord_report_webhook_url)

                // Skip if webhook URL is not configured
                if (webhookUrl.isBlank() || webhookUrl == "YOUR_DISCORD_WEBHOOK_URL_HERE") {
                    Log.d(TAG, "Discord webhook URL not configured, skipping notification")
                    return@withContext true
                }

                // Build Discord embed message
                val embed =
                    JSONObject().apply {
                        put("title", "🎮 New Report Submitted")
                        put("color", getColorForRating(reportData.emulation_rating))
                        put(
                            "timestamp",
                            reportData.timestamp?.toDate()?.let { date ->
                                java.text
                                    .SimpleDateFormat(
                                        "yyyy-MM-dd'T'HH:mm:ss.SSS'Z'",
                                        java.util.Locale.US,
                                    ).apply { timeZone = java.util.TimeZone.getTimeZone("UTC") }
                                    .format(date)
                            },
                        )

                        // Add URL to game detail page if game_id is available
                        if (!reportData.game_id.isNullOrBlank()) {
                            put("url", "https://www.yabasanshiro.com/games/${reportData.game_id}")
                        }

                        // Add fields
                        val fields = JSONArray()

                        // User info
                        fields.put(
                            JSONObject().apply {
                                put("name", "👤 User")
                                put("value", reportData.display_name ?: "Unknown")
                                put("inline", true)
                            },
                        )

                        // Game title and product number
                        val gameInfo =
                            buildString {
                                if (!reportData.game_title.isNullOrBlank()) {
                                    append(reportData.game_title)
                                    if (!reportData.product_number.isNullOrBlank()) {
                                        append("\n`")
                                        append(reportData.product_number)
                                        append("`")
                                    }
                                } else if (!reportData.product_number.isNullOrBlank()) {
                                    append(reportData.product_number)
                                } else {
                                    append("Unknown")
                                }
                            }
                        fields.put(
                            JSONObject().apply {
                                put("name", "📀 Game")
                                put("value", gameInfo)
                                put("inline", false)
                            },
                        )

                        // Emulation rating
                        fields.put(
                            JSONObject().apply {
                                put("name", "⭐ Emulation Rating")
                                put("value", reportData.getRatingStars())
                                put("inline", true)
                            },
                        )

                        // Game rating
                        fields.put(
                            JSONObject().apply {
                                put("name", "🎯 Game Rating")
                                put("value", "${"★".repeat(reportData.rating)}")
                                put("inline", true)
                            },
                        )

                        // Version info
                        fields.put(
                            JSONObject().apply {
                                put("name", "📱 Version")
                                put("value", "${reportData.version} (${reportData.version_code})")
                                put("inline", true)
                            },
                        )

                        // Attachments info
                        if (reportData.has_attachments) {
                            val attachmentInfo = mutableListOf<String>()
                            if (!reportData.screenshot_url.isNullOrEmpty()) {
                                attachmentInfo.add("📸 Screenshot")
                            }
                            if (!reportData.savestate_url.isNullOrEmpty()) {
                                attachmentInfo.add("💾 Save State")
                            }
                            if (!reportData.memory_url.isNullOrEmpty()) {
                                attachmentInfo.add("🧠 Memory")
                            }

                            fields.put(
                                JSONObject().apply {
                                    put("name", "📎 Attachments")
                                    put("value", attachmentInfo.joinToString(", "))
                                    put("inline", false)
                                },
                            )

                            if (reportData.attachment_size > 0) {
                                fields.put(
                                    JSONObject().apply {
                                        put("name", "💽 Size")
                                        put("value", reportData.getFormattedAttachmentSize())
                                        put("inline", true)
                                    },
                                )
                            }
                        }

                        // Comment
                        if (reportData.comment.isNotBlank()) {
                            fields.put(
                                JSONObject().apply {
                                    put("name", "💬 Comment")
                                    put("value", reportData.comment.take(1024)) // Discord field value limit
                                    put("inline", false)
                                },
                            )
                        }

                        put("fields", fields)

                        // Add screenshot image if available
                        if (!reportData.screenshot_url.isNullOrEmpty()) {
                            put(
                                "image",
                                JSONObject().apply {
                                    put("url", reportData.screenshot_url)
                                },
                            )
                        }

                        // Footer
                        put(
                            "footer",
                            JSONObject().apply {
                                put("text", "YabaSanshiro Report System")
                            },
                        )
                    }

                // Build webhook payload
                val payload =
                    JSONObject().apply {
                        put("embeds", JSONArray().put(embed))
                    }

                // Send request
                val mediaType = "application/json; charset=utf-8".toMediaTypeOrNull()
                val body = payload.toString().toRequestBody(mediaType)

                val request =
                    Request
                        .Builder()
                        .url(webhookUrl)
                        .post(body)
                        .build()

                val response = client.newCall(request).execute()

                if (response.isSuccessful) {
                    Log.d(TAG, "Discord notification sent successfully")
                    true
                } else {
                    Log.e(TAG, "Failed to send Discord notification: ${response.code} ${response.message}")
                    false
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error sending Discord notification", e)
                false
            }
        }
    }

    /**
     * Get Discord embed color based on rating
     * Returns a decimal color value for Discord embeds
     */
    private fun getColorForRating(rating: Int): Int = when (rating) {
        5 -> 0x00FF00 // Green - Excellent
        4 -> 0x7FFF00 // Light Green - Good
        3 -> 0xFFFF00 // Yellow - Average
        2 -> 0xFF8C00 // Orange - Poor
        1 -> 0xFF0000 // Red - Very Poor
        else -> 0x808080 // Gray - Unknown
    }

    companion object {
        private const val TAG = "DiscordNotifier"
    }
}
