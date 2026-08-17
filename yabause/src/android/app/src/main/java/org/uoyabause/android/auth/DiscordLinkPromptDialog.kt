package org.uoyabause.android.auth

import android.content.Context
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.TextView
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.FragmentActivity
import org.devmiyax.yabasanshiro.R

/**
 * Dialog to prompt the user to link their Discord account
 */
class DiscordLinkPromptDialog : DialogFragment() {
    /**
     * Interface for handling Discord link prompt actions
     */
    interface DiscordLinkPromptListener {
        /**
         * Called when the user accepts to link their Discord account
         */
        fun onDiscordLinkAccepted()

        /**
         * Called when the user declines to link their Discord account
         */
        fun onDiscordLinkDeclined()
    }

    private var listener: DiscordLinkPromptListener? = null

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View? = inflater.inflate(R.layout.dialog_discord_link_prompt, container, false)

    override fun onViewCreated(
        view: View,
        savedInstanceState: Bundle?,
    ) {
        super.onViewCreated(view, savedInstanceState)

        // Set dialog title and description
        view.findViewById<TextView>(R.id.tv_discord_link_title).text =
            getString(R.string.discord_link_prompt_title)
        view.findViewById<TextView>(R.id.tv_discord_link_description).text =
            getString(R.string.discord_link_prompt_message)

        // Set up link button
        view.findViewById<Button>(R.id.btn_link_discord).apply {
            text = getString(R.string.discord_link_prompt_yes)
            setOnClickListener {
                listener?.onDiscordLinkAccepted()
                dismiss()
            }
        }

        // Set up skip button
        view.findViewById<Button>(R.id.btn_skip_discord_link).apply {
            text = getString(R.string.discord_link_prompt_no)
            setOnClickListener {
                listener?.onDiscordLinkDeclined()
                dismiss()
            }
        }
    }

    override fun onAttach(context: Context) {
        super.onAttach(context)
        if (context is DiscordLinkPromptListener) {
            listener = context
        } else if (parentFragment is DiscordLinkPromptListener) {
            listener = parentFragment as DiscordLinkPromptListener
        } else {
            throw RuntimeException("$context must implement DiscordLinkPromptListener")
        }
    }

    override fun onDetach() {
        super.onDetach()
        listener = null
    }

    companion object {
        const val TAG = "DiscordLinkPromptDialog"

        /**
         * Show the Discord link prompt dialog
         * @param activity The activity to show the dialog in
         */
        fun show(activity: FragmentActivity) {
            val dialog = DiscordLinkPromptDialog()
            dialog.show(activity.supportFragmentManager, TAG)
        }
    }
}
