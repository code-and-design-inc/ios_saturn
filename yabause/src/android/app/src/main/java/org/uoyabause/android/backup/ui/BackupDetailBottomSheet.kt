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
import com.bumptech.glide.signature.ObjectKey
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import com.google.android.material.button.MaterialButton
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.backup.model.BackupItem
import org.uoyabause.android.backup.model.DeviceType
import java.io.File

class BackupDetailBottomSheet : BottomSheetDialogFragment() {
    interface Listener {
        fun onCopyToClick(item: BackupItem)

        fun onExportClick(item: BackupItem)

        fun onShareClick(item: BackupItem)

        fun onDeleteClick(item: BackupItem)
    }

    private var listener: Listener? = null
    private var item: BackupItem? = null
    private var isCloud: Boolean = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        arguments?.let { args ->
            isCloud = args.getBoolean(ARG_IS_CLOUD, false)
            item = BackupItem(
                id = args.getString(ARG_ID, ""),
                filename = args.getString(ARG_FILENAME, ""),
                comment = args.getString(ARG_COMMENT, ""),
                language = args.getInt(ARG_LANGUAGE, 0),
                saveDate = args.getString(ARG_SAVE_DATE, ""),
                dataSize = args.getInt(ARG_DATA_SIZE, 0),
                blockSize = args.getInt(ARG_BLOCK_SIZE, 0),
                deviceType = DeviceType.fromId(args.getInt(ARG_DEVICE_TYPE, DeviceType.INTERNAL.id)),
                downloadUrl = args.getString(ARG_DOWNLOAD_URL),
                firebaseKey = args.getString(ARG_FIREBASE_KEY),
                screenshotUrl = args.getString(ARG_SCREENSHOT_URL),
                gameTitle = args.getString(ARG_GAME_TITLE),
                productNumber = args.getString(ARG_PRODUCT_NUMBER),
                backupFileKey = args.getString(ARG_BACKUP_FILE_KEY, ""),
            )
        }
        listener = parentFragment as? Listener
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View? = inflater.inflate(R.layout.bottom_sheet_backup_detail, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val currentItem = item ?: return

        bindData(view, currentItem)
        setupButtons(view, currentItem)
        setupGamepadSupport(view)
    }

    private fun bindData(view: View, item: BackupItem) {
        // Screenshot
        val ivScreenshot = view.findViewById<ImageView>(R.id.iv_screenshot_detail)
        val screenshotPath = item.screenshotUrl
        if (!screenshotPath.isNullOrEmpty()) {
            if (screenshotPath.startsWith("https://")) {
                ivScreenshot.imageTintList = null
                Glide
                    .with(this)
                    .load(screenshotPath)
                    .placeholder(R.drawable.ic_save)
                    .fitCenter()
                    .into(ivScreenshot)
            } else {
                val file = File(screenshotPath)
                if (file.exists()) {
                    ivScreenshot.imageTintList = null
                    Glide
                        .with(this)
                        .load(file)
                        .signature(ObjectKey(file.lastModified()))
                        .fitCenter()
                        .into(ivScreenshot)
                } else {
                    ivScreenshot.setImageResource(R.drawable.ic_save)
                }
            }
        } else {
            ivScreenshot.setImageResource(R.drawable.ic_save)
        }

        // Game title
        val tvGameTitle = view.findViewById<TextView>(R.id.tv_game_title_detail)
        if (!item.gameTitle.isNullOrEmpty()) {
            tvGameTitle.text = item.gameTitle
            tvGameTitle.visibility = View.VISIBLE
        } else {
            tvGameTitle.visibility = View.GONE
        }

        // Filename
        view.findViewById<TextView>(R.id.tv_filename_detail).text = item.filename

        // Comment
        val tvComment = view.findViewById<TextView>(R.id.tv_comment_detail)
        if (item.comment.isNotEmpty()) {
            tvComment.text = item.comment
            tvComment.visibility = View.VISIBLE
        } else {
            tvComment.visibility = View.GONE
        }

        // Size
        view.findViewById<TextView>(R.id.tv_size_detail).text = item.displayBlocks

        // Date
        view.findViewById<TextView>(R.id.tv_date_detail).text = item.saveDate
    }

    private fun setupButtons(view: View, item: BackupItem) {
        val btnCopyTo = view.findViewById<MaterialButton>(R.id.btn_copy_to_detail)
        val btnShare = view.findViewById<MaterialButton>(R.id.btn_share_detail)
        val btnDelete = view.findViewById<MaterialButton>(R.id.btn_delete_detail)

        btnCopyTo.setOnClickListener {
            listener?.onCopyToClick(item)
        }

        btnShare.setOnClickListener {
            listener?.onShareClick(item)
        }

        btnDelete.setOnClickListener {
            listener?.onDeleteClick(item)
            dismiss()
        }

        // Set initial focus to CopyTo button
        btnCopyTo.post { btnCopyTo.requestFocus() }

        // Focus order (Share is available for both local and cloud items)
        btnDelete.nextFocusRightId = R.id.btn_share_detail
        btnShare.nextFocusLeftId = R.id.btn_delete_detail
        btnShare.nextFocusRightId = R.id.btn_copy_to_detail
        btnCopyTo.nextFocusLeftId = R.id.btn_share_detail
    }

    private fun setupGamepadSupport(view: View) {
        val buttons = listOf<View>(
            view.findViewById(R.id.btn_delete_detail),
            view.findViewById(R.id.btn_share_detail),
            view.findViewById(R.id.btn_copy_to_detail),
        )

        buttons.forEach { button ->
            // A button / Enter / DpadCenter support
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
        private const val ARG_FILENAME = "filename"
        private const val ARG_COMMENT = "comment"
        private const val ARG_LANGUAGE = "language"
        private const val ARG_SAVE_DATE = "saveDate"
        private const val ARG_DATA_SIZE = "dataSize"
        private const val ARG_BLOCK_SIZE = "blockSize"
        private const val ARG_DEVICE_TYPE = "deviceType"
        private const val ARG_DOWNLOAD_URL = "downloadUrl"
        private const val ARG_FIREBASE_KEY = "firebaseKey"
        private const val ARG_SCREENSHOT_URL = "screenshotUrl"
        private const val ARG_GAME_TITLE = "gameTitle"
        private const val ARG_PRODUCT_NUMBER = "productNumber"
        private const val ARG_BACKUP_FILE_KEY = "backupFileKey"
        private const val ARG_IS_CLOUD = "isCloud"

        fun newInstance(item: BackupItem, isCloud: Boolean): BackupDetailBottomSheet =
            BackupDetailBottomSheet().apply {
                arguments = Bundle().apply {
                    putString(ARG_ID, item.id)
                    putString(ARG_FILENAME, item.filename)
                    putString(ARG_COMMENT, item.comment)
                    putInt(ARG_LANGUAGE, item.language)
                    putString(ARG_SAVE_DATE, item.saveDate)
                    putInt(ARG_DATA_SIZE, item.dataSize)
                    putInt(ARG_BLOCK_SIZE, item.blockSize)
                    putInt(ARG_DEVICE_TYPE, item.deviceType.id)
                    putString(ARG_DOWNLOAD_URL, item.downloadUrl)
                    putString(ARG_FIREBASE_KEY, item.firebaseKey)
                    putString(ARG_SCREENSHOT_URL, item.screenshotUrl)
                    putString(ARG_GAME_TITLE, item.gameTitle)
                    putString(ARG_PRODUCT_NUMBER, item.productNumber)
                    putString(ARG_BACKUP_FILE_KEY, item.backupFileKey)
                    putBoolean(ARG_IS_CLOUD, isCloud)
                }
            }
    }
}
