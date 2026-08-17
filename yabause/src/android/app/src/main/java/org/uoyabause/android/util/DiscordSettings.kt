package org.uoyabause.android.util

import android.content.Context
import android.content.SharedPreferences

/**
 * Discord関連の設定を管理するユーティリティクラス
 */
class DiscordSettings {
    companion object {
        private const val PREFS_NAME = "discord_settings"
        private const val KEY_WEBHOOK_ENABLED = "webhook_enabled"
        private const val KEY_WEBHOOK_URL = "webhook_url"

        /**
         * Discord Webhookが有効かどうかを取得する
         *
         * @param context コンテキスト
         * @return Webhookが有効な場合はtrue、それ以外はfalse
         */
        fun isWebhookEnabled(context: Context): Boolean {
            val prefs = getPreferences(context)
            return prefs.getBoolean(KEY_WEBHOOK_ENABLED, false)
        }

        /**
         * Discord Webhookの有効/無効を設定する
         *
         * @param context コンテキスト
         * @param enabled 有効にする場合はtrue、無効にする場合はfalse
         */
        fun setWebhookEnabled(
            context: Context,
            enabled: Boolean,
        ) {
            val prefs = getPreferences(context)
            prefs.edit().putBoolean(KEY_WEBHOOK_ENABLED, enabled).apply()
        }

        /**
         * Discord WebhookのURLを取得する
         *
         * @param context コンテキスト
         * @return WebhookのURL、設定されていない場合は空文字列
         */
        fun getWebhookUrl(context: Context): String {
            val prefs = getPreferences(context)
            return prefs.getString(KEY_WEBHOOK_URL, "") ?: ""
        }

        /**
         * Discord WebhookのURLを設定する
         *
         * @param context コンテキスト
         * @param url WebhookのURL
         */
        fun setWebhookUrl(
            context: Context,
            url: String,
        ) {
            val prefs = getPreferences(context)
            prefs.edit().putString(KEY_WEBHOOK_URL, url).apply()
        }

        /**
         * SharedPreferencesを取得する
         */
        private fun getPreferences(context: Context): SharedPreferences = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
    }
}
