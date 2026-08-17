package org.devmiyax.yabasanshiro

import android.app.AlertDialog
import android.content.Context

/**
 * Discord連携を促すダイアログ
 */
class DiscordLinkPromptDialog {
    companion object {
        /**
         * ダイアログを表示する
         */
        fun show(context: Context): AlertDialog {
            val listener =
                if (context is DiscordLinkPromptListener) {
                    context
                } else {
                    null
                }

            return AlertDialog
                .Builder(context)
                .setTitle(R.string.discord_link_prompt_title)
                .setMessage(R.string.discord_link_prompt_message)
                .setIcon(R.drawable.ic_discord_logo)
                .setPositiveButton(R.string.discord_link_prompt_yes) { _, _ ->
                    listener?.onDiscordLinkAccepted()
                }.setNegativeButton(R.string.discord_link_prompt_no) { _, _ ->
                    listener?.onDiscordLinkDeclined()
                }.setCancelable(false)
                .show()
        }
    }

    /**
     * Discord連携プロンプトの結果を受け取るリスナーインターフェース
     */
    interface DiscordLinkPromptListener {
        /**
         * ユーザーがDiscord連携を承諾した場合に呼ばれる
         */
        fun onDiscordLinkAccepted()

        /**
         * ユーザーがDiscord連携を拒否した場合に呼ばれる
         */
        fun onDiscordLinkDeclined()
    }
}
