/*
 * Copyright 2024 devMiyax
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.uoyabause.android.backup.ui

import android.os.Bundle
import android.view.KeyEvent
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import com.bumptech.glide.Glide
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import com.google.android.material.button.MaterialButton
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.backup.model.SharedBackupItem
import java.util.Date

class SharedBackupDetailBottomSheet : BottomSheetDialogFragment() {
    interface Listener {
        fun onImportClick(item: SharedBackupItem)

        fun onRateClick(item: SharedBackupItem)

        fun onShareClick(item: SharedBackupItem)

        fun onDeleteClick(item: SharedBackupItem)
    }

    private var listener: Listener? = null
    private var item: SharedBackupItem? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        arguments?.let { args ->
            item = SharedBackupItem(
                id = args.getString(ARG_ID, ""),
                ownerId = args.getString(ARG_OWNER_ID, ""),
                ownerName = args.getString(ARG_OWNER_NAME, ""),
                ownerPhotoUrl = args.getString(ARG_OWNER_PHOTO_URL),
                gameTitle = args.getString(ARG_GAME_TITLE, ""),
                productNumber = args.getString(ARG_PRODUCT_NUMBER, ""),
                filename = args.getString(ARG_FILENAME, ""),
                comment = args.getString(ARG_COMMENT, ""),
                description = args.getString(ARG_DESCRIPTION, ""),
                saveDate = args.getString(ARG_SAVE_DATE, ""),
                downloadUrl = args.getString(ARG_DOWNLOAD_URL, ""),
                sharedAt = args.getLong(ARG_SHARED_AT, 0L).let {
                    if (it > 0) Date(it) else null
                },
                downloadCount = args.getInt(ARG_DOWNLOAD_COUNT, 0),
                isPublic = args.getBoolean(ARG_IS_PUBLIC, true),
                averageRating = args.getFloat(ARG_AVERAGE_RATING, 0f),
                ratingCount = args.getInt(ARG_RATING_COUNT, 0),
                dataSize = args.getInt(ARG_DATA_SIZE, 0),
                blockSize = args.getInt(ARG_BLOCK_SIZE, 0),
                screenshotUrl = args.getString(ARG_SCREENSHOT_URL),
            )
        }
        listener = parentFragment as? Listener
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View? = inflater.inflate(R.layout.bottom_sheet_shared_backup_detail, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val currentItem = item ?: return

        bindData(view, currentItem)
        setupButtons(view, currentItem)
        setupGamepadSupport(view)
    }

    /**
     * Refresh the displayed download count after an import, without recreating
     * the sheet.
     */
    fun updateDownloadCount(count: Int) {
        item = item?.copy(downloadCount = count)
        view?.findViewById<TextView>(R.id.tv_downloads_detail)?.text = count.toString()
    }

    private fun bindData(view: View, item: SharedBackupItem) {
        // Screenshot
        val ivScreenshot = view.findViewById<ImageView>(R.id.iv_screenshot_detail)
        if (!item.screenshotUrl.isNullOrEmpty()) {
            ivScreenshot.imageTintList = null
            Glide
                .with(this)
                .load(item.screenshotUrl)
                .placeholder(R.drawable.ic_save)
                .fitCenter()
                .into(ivScreenshot)
        } else {
            ivScreenshot.setImageResource(R.drawable.ic_save)
        }

        // Game title
        view.findViewById<TextView>(R.id.tv_game_title_detail).text =
            item.gameTitle.ifEmpty { "Unknown Game" }

        // Filename
        view.findViewById<TextView>(R.id.tv_filename_detail).text = item.filename

        // Description (shared by the owner)
        val tvDescription = view.findViewById<TextView>(R.id.tv_description_detail)
        if (item.description.isNotEmpty()) {
            tvDescription.text = item.description
            tvDescription.visibility = View.VISIBLE
        } else {
            tvDescription.visibility = View.GONE
        }

        // Original backup comment
        val tvComment = view.findViewById<TextView>(R.id.tv_comment_detail)
        if (item.comment.isNotEmpty()) {
            tvComment.text = item.comment
            tvComment.visibility = View.VISIBLE
        } else {
            tvComment.visibility = View.GONE
        }

        // Owner photo
        val ivOwnerPhoto = view.findViewById<ImageView>(R.id.iv_owner_photo_detail)
        if (!item.ownerPhotoUrl.isNullOrEmpty()) {
            Glide
                .with(this)
                .load(item.ownerPhotoUrl)
                .placeholder(R.drawable.ic_account_circle_24)
                .circleCrop()
                .into(ivOwnerPhoto)
        } else {
            ivOwnerPhoto.setImageResource(R.drawable.ic_account_circle_24)
        }

        // Owner name
        view.findViewById<TextView>(R.id.tv_owner_name_detail).text = item.ownerName

        // Rating
        view.findViewById<TextView>(R.id.tv_rating_detail).text = item.displayRating

        // Downloads
        view.findViewById<TextView>(R.id.tv_downloads_detail).text =
            item.downloadCount.toString()

        // Shared date
        val tvSharedDate = view.findViewById<TextView>(R.id.tv_shared_date_detail)
        tvSharedDate.text = item.displaySharedDate
    }

    private fun setupButtons(view: View, item: SharedBackupItem) {
        val btnImport = view.findViewById<MaterialButton>(R.id.btn_import_detail)
        val btnRate = view.findViewById<MaterialButton>(R.id.btn_rate_detail)
        val btnShare = view.findViewById<MaterialButton>(R.id.btn_share_detail)
        val btnDelete = view.findViewById<MaterialButton>(R.id.btn_delete_detail)

        btnImport.setOnClickListener {
            listener?.onImportClick(item)
        }

        btnRate.setOnClickListener {
            listener?.onRateClick(item)
        }

        btnShare.setOnClickListener {
            listener?.onShareClick(item)
        }

        btnDelete.setOnClickListener {
            listener?.onDeleteClick(item)
            dismiss()
        }

        // Show delete button only for owner
        val isOwner = arguments?.getBoolean(ARG_IS_OWNER, false) ?: false
        btnDelete.visibility = if (isOwner) View.VISIBLE else View.GONE

        // Set initial focus to Import button
        btnImport.post { btnImport.requestFocus() }

        // Focus order
        btnImport.nextFocusLeftId = R.id.btn_rate_detail
        btnRate.nextFocusRightId = R.id.btn_import_detail
        btnRate.nextFocusLeftId = R.id.btn_share_detail
        btnShare.nextFocusRightId = R.id.btn_rate_detail
        if (isOwner) {
            btnShare.nextFocusLeftId = R.id.btn_delete_detail
            btnDelete.nextFocusRightId = R.id.btn_share_detail
        }
    }

    private fun setupGamepadSupport(view: View) {
        val buttons = listOf<View>(
            view.findViewById(R.id.btn_import_detail),
            view.findViewById(R.id.btn_rate_detail),
            view.findViewById(R.id.btn_share_detail),
            view.findViewById(R.id.btn_delete_detail),
        )

        buttons.forEach { button ->
            // A button support
            button.setOnKeyListener { v, keyCode, event ->
                if (event.action == KeyEvent.ACTION_DOWN) {
                    when (keyCode) {
                        KeyEvent.KEYCODE_BUTTON_A,
                        KeyEvent.KEYCODE_ENTER,
                        KeyEvent.KEYCODE_DPAD_CENTER,
                        -> {
                            v.performClick()
                            true
                        }
                        else -> false
                    }
                } else {
                    false
                }
            }

            // Focus animation
            button.setOnFocusChangeListener { v, hasFocus ->
                val scale = if (hasFocus) 1.05f else 1.0f
                val alpha = if (hasFocus) 1.0f else 0.9f
                v
                    .animate()
                    .scaleX(scale)
                    .scaleY(scale)
                    .alpha(alpha)
                    .setDuration(150)
                    .start()
            }
        }
    }

    companion object {
        private const val ARG_ID = "id"
        private const val ARG_OWNER_ID = "ownerId"
        private const val ARG_OWNER_NAME = "ownerName"
        private const val ARG_OWNER_PHOTO_URL = "ownerPhotoUrl"
        private const val ARG_GAME_TITLE = "gameTitle"
        private const val ARG_PRODUCT_NUMBER = "productNumber"
        private const val ARG_FILENAME = "filename"
        private const val ARG_COMMENT = "comment"
        private const val ARG_DESCRIPTION = "description"
        private const val ARG_SAVE_DATE = "saveDate"
        private const val ARG_DOWNLOAD_URL = "downloadUrl"
        private const val ARG_SHARED_AT = "sharedAt"
        private const val ARG_DOWNLOAD_COUNT = "downloadCount"
        private const val ARG_IS_PUBLIC = "isPublic"
        private const val ARG_AVERAGE_RATING = "averageRating"
        private const val ARG_RATING_COUNT = "ratingCount"
        private const val ARG_DATA_SIZE = "dataSize"
        private const val ARG_BLOCK_SIZE = "blockSize"
        private const val ARG_SCREENSHOT_URL = "screenshotUrl"
        private const val ARG_IS_OWNER = "isOwner"

        fun newInstance(item: SharedBackupItem, isOwner: Boolean): SharedBackupDetailBottomSheet =
            SharedBackupDetailBottomSheet().apply {
                arguments = Bundle().apply {
                    putString(ARG_ID, item.id)
                    putString(ARG_OWNER_ID, item.ownerId)
                    putString(ARG_OWNER_NAME, item.ownerName)
                    putString(ARG_OWNER_PHOTO_URL, item.ownerPhotoUrl)
                    putString(ARG_GAME_TITLE, item.gameTitle)
                    putString(ARG_PRODUCT_NUMBER, item.productNumber)
                    putString(ARG_FILENAME, item.filename)
                    putString(ARG_COMMENT, item.comment)
                    putString(ARG_DESCRIPTION, item.description)
                    putString(ARG_SAVE_DATE, item.saveDate)
                    putString(ARG_DOWNLOAD_URL, item.downloadUrl)
                    putLong(ARG_SHARED_AT, item.sharedAt?.time ?: 0L)
                    putInt(ARG_DOWNLOAD_COUNT, item.downloadCount)
                    putBoolean(ARG_IS_PUBLIC, item.isPublic)
                    putFloat(ARG_AVERAGE_RATING, item.averageRating)
                    putInt(ARG_RATING_COUNT, item.ratingCount)
                    putInt(ARG_DATA_SIZE, item.dataSize)
                    putInt(ARG_BLOCK_SIZE, item.blockSize)
                    putString(ARG_SCREENSHOT_URL, item.screenshotUrl)
                    putBoolean(ARG_IS_OWNER, isOwner)
                }
            }
    }
}
