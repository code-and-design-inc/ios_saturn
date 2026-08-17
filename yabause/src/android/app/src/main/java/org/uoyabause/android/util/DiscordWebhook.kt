package org.uoyabause.android.util

import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.io.OutputStreamWriter
import java.net.HttpURLConnection
import java.net.URL

/**
 * Discord Webhook を使用してメッセージを送信するユーティリティクラス
 */
class DiscordWebhook {
    companion object {
        private const val TAG = "DiscordWebhook"

        /**
         * Discordのウェブフックにメッセージを送信する
         *
         * @param webhookUrl Discordのウェブフックのエンドポイント
         * @param username 表示するユーザー名
         * @param content メッセージの内容
         * @param embedTitle 埋め込みのタイトル（オプション）
         * @param embedDescription 埋め込みの説明（オプション）
         * @param embedColor 埋め込みの色（オプション、16進数のカラーコード）
         * @param avatarUrl アバターのURL（オプション）
         * @return 送信が成功したかどうか
         */
        suspend fun sendMessage(
            webhookUrl: String,
            username: String,
            content: String,
            embedTitle: String? = null,
            embedDescription: String? = null,
            embedColor: Int? = null,
            avatarUrl: String? = null,
        ): Boolean =
            withContext(Dispatchers.IO) {
                try {
                    val url = URL(webhookUrl)
                    val connection = url.openConnection() as HttpURLConnection
                    connection.requestMethod = "POST"
                    connection.setRequestProperty("Content-Type", "application/json")
                    connection.doOutput = true

                    Log.d(TAG, "Creating JSONObject...")
                    val json = JSONObject()
                    Log.d(TAG, "JSONObject created: $json")
                    json.put("username", username)
                    json.put("content", content)

                    json.put("tts", false)

                    // アバターURLが指定されている場合は設定
                    if (avatarUrl != null) {
                        json.put("avatar_url", avatarUrl)
                    }

                    // 埋め込みが指定されている場合は追加
                    if (embedTitle != null || embedDescription != null) {
                        Log.d(TAG, "Creating embed JSONObject...")
                        val embed = JSONObject()
                        Log.d(TAG, "Embed JSONObject created: $embed")
                        if (embedTitle != null) {
                            embed.put("title", embedTitle)
                        }
                        if (embedDescription != null) {
                            embed.put("description", embedDescription)
                        }
                        if (embedColor != null) {
                            embed.put("color", embedColor)
                        }

                        // JSONArrayを正しく作成
                        val embedsArray = JSONArray()
                        embedsArray.put(embed)
                        json.put("embeds", embedsArray)
                    }

                    val outputStreamWriter = OutputStreamWriter(connection.outputStream)
                    outputStreamWriter.write(json.toString())
                    outputStreamWriter.flush()

                    val responseCode = connection.responseCode
                    Log.d(TAG, "Discord webhook response code: $responseCode")

                    outputStreamWriter.close()
                    connection.disconnect()

                    return@withContext responseCode == 204 // Discordのウェブフックは成功時に204を返す
                } catch (e: Exception) {
                    Log.e(TAG, "Error sending Discord webhook", e)
                    return@withContext false
                }
            }

        /**
         * 新記録（1位）をDiscordに投稿する
         *
         * @param webhookUrl Discordのウェブフックのエンドポイント
         * @param gameId ゲームID
         * @param leaderboardName リーダーボード名
         * @param userName ユーザー名
         * @param score スコア（タイム）
         * @param avatarUrl アバターのURL（オプション）
         * @return 送信が成功したかどうか
         */
        suspend fun sendNewRecordMessage(
            webhookUrl: String,
            gameId: String,
            leaderboardName: String,
            userName: String,
            score: Long,
            avatarUrl: String? = null,
        ): Boolean {
            // タイムをフォーマット（ミリ秒をMM:SS.MMMの形式に変換）
            val formattedTime = formatTime(score)

            // Build message content
            val content = "🏆 **NEW RECORD!** 🏆"
            val embedTitle = "$leaderboardName - New Record"
            val embedDescription = "**$userName** achieved 1st place with a time of **$formattedTime**!"

            // Embed color (gold)
            val embedColor = 0xFFD700 // Gold color hex code

            return sendMessage(
                webhookUrl = webhookUrl,
                username = "Leaderboard Bot",
                content = content,
                embedTitle = embedTitle,
                embedDescription = embedDescription,
                embedColor = embedColor,
                avatarUrl = avatarUrl,
            )
        }

        /**
         * タイムをフォーマットする（ミリ秒をMM:SS.MMMの形式に変換）
         */
        private fun formatTime(msec: Long): String {
            val min = msec / 60000
            val sec = (msec % 60000) / 1000
            val ms = msec % 1000
            return String.format("%d:%02d.%03d", min, sec, ms)
        }
    }
}
