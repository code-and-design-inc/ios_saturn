package org.uoyabause.android.news

import android.app.Dialog
import android.content.DialogInterface
import android.content.Intent
import android.graphics.drawable.Drawable
import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.widget.TextView
import androidx.fragment.app.DialogFragment
import com.bumptech.glide.Glide
import com.bumptech.glide.load.DataSource
import com.bumptech.glide.load.engine.GlideException
import com.bumptech.glide.request.RequestListener
import com.bumptech.glide.request.target.Target
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.imageview.ShapeableImageView
import org.devmiyax.yabasanshiro.R

// News popup as a DialogFragment so it survives configuration changes (e.g. rotation).
// Content is passed via arguments; the FragmentManager rebuilds it after recreation.
class NewsDialogFragment : DialogFragment() {
    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val activity = requireActivity()
        val args = requireArguments()
        val titleText = args.getString(ARG_TITLE)
        val bodyText = args.getString(ARG_BODY)
        val imageUrl = args.getString(ARG_IMAGE_URL)
        val linkUrl = args.getString(ARG_LINK_URL)
        val showUpdate = args.getBoolean(ARG_SHOW_UPDATE)
        val playStoreUrl = args.getString(ARG_PLAY_URL).orEmpty()

        val view = LayoutInflater.from(activity).inflate(R.layout.dialog_app_news, null)
        val image = view.findViewById<ShapeableImageView>(R.id.news_image)
        val title = view.findViewById<TextView>(R.id.news_title)
        val body = view.findViewById<TextView>(R.id.news_body)

        title.text = titleText
        body.text = bodyText

        if (imageUrl.isNullOrEmpty()) {
            image.visibility = View.GONE
        } else {
            image.visibility = View.VISIBLE
            Glide
                .with(this)
                .load(imageUrl)
                .listener(
                    object : RequestListener<Drawable> {
                        override fun onLoadFailed(
                            e: GlideException?,
                            model: Any?,
                            target: Target<Drawable>,
                            isFirstResource: Boolean,
                        ): Boolean {
                            // Spec 7: image load failure -> hide frame, show text only.
                            image.visibility = View.GONE
                            return false
                        }

                        override fun onResourceReady(
                            resource: Drawable,
                            model: Any,
                            target: Target<Drawable>?,
                            dataSource: DataSource,
                            isFirstResource: Boolean,
                        ): Boolean = false
                    },
                ).into(image)
        }

        val builder =
            MaterialAlertDialogBuilder(activity)
                .setView(view)
                .setNegativeButton(R.string.news_close, null)

        if (!linkUrl.isNullOrEmpty()) {
            builder.setNeutralButton(R.string.news_read_more) { _, _ ->
                activity.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(linkUrl)))
            }
        }
        if (showUpdate) {
            builder.setPositiveButton(R.string.news_update) { _, _ ->
                activity.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(playStoreUrl)))
            }
        }

        val dialog = builder.create()
        // Gamepad/D-pad focus (app/CLAUDE.md required pattern).
        dialog.setOnShowListener {
            val focusButton =
                dialog.getButton(
                    if (showUpdate) DialogInterface.BUTTON_POSITIVE else DialogInterface.BUTTON_NEGATIVE,
                )
            focusButton?.post {
                focusButton.isFocusable = true
                focusButton.isFocusableInTouchMode = true
                focusButton.requestFocus()
            }
        }
        return dialog
    }

    companion object {
        const val TAG = "app_news_dialog"
        private const val ARG_TITLE = "title"
        private const val ARG_BODY = "body"
        private const val ARG_IMAGE_URL = "image_url"
        private const val ARG_LINK_URL = "link_url"
        private const val ARG_SHOW_UPDATE = "show_update"
        private const val ARG_PLAY_URL = "play_url"

        fun newInstance(
            title: String,
            body: String,
            imageUrl: String?,
            linkUrl: String?,
            showUpdate: Boolean,
            playStoreUrl: String,
        ): NewsDialogFragment =
            NewsDialogFragment().apply {
                arguments =
                    Bundle().apply {
                        putString(ARG_TITLE, title)
                        putString(ARG_BODY, body)
                        putString(ARG_IMAGE_URL, imageUrl)
                        putString(ARG_LINK_URL, linkUrl)
                        putBoolean(ARG_SHOW_UPDATE, showUpdate)
                        putString(ARG_PLAY_URL, playStoreUrl)
                    }
            }
    }
}
