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

import android.content.res.ColorStateList
import android.view.KeyEvent
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import androidx.activity.ComponentActivity
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.bumptech.glide.Glide
import com.bumptech.glide.signature.ObjectKey
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.backup.model.BackupItem
import org.uoyabause.android.backup.model.DeviceType
import java.io.File

/**
 * RecyclerView adapter for displaying backup items.
 * Uses ListAdapter with DiffUtil for efficient updates.
 */
class BackupListAdapter(
    private val listener: OnItemClickListener,
) : ListAdapter<BackupItem, BackupListAdapter.BackupViewHolder>(BackupDiffCallback()) {
    interface OnItemClickListener {
        fun onItemClick(item: BackupItem)
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): BackupViewHolder {
        val view = LayoutInflater
            .from(parent.context)
            .inflate(R.layout.item_backup, parent, false)
        return BackupViewHolder(view)
    }

    override fun onBindViewHolder(holder: BackupViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    inner class BackupViewHolder(
        itemView: View,
    ) : RecyclerView.ViewHolder(itemView) {
        private val ivBackupIcon: ImageView = itemView.findViewById(R.id.iv_backup_icon)
        private val tvFilename: TextView = itemView.findViewById(R.id.tv_backup_filename)
        private val tvComment: TextView = itemView.findViewById(R.id.tv_backup_comment)
        private val tvSize: TextView = itemView.findViewById(R.id.tv_backup_size)
        private val tvDate: TextView = itemView.findViewById(R.id.tv_backup_date)
        private val ivCloudIndicator: ImageView = itemView.findViewById(R.id.iv_cloud_indicator)

        fun bind(item: BackupItem) {
            tvFilename.text = item.filename
            tvComment.text = item.comment.ifEmpty { "-" }
            tvSize.text = item.displayBlocks
            tvDate.text = item.saveDate

            // Show cloud indicator for cloud backups
            ivCloudIndicator.visibility = if (item.isCloudBackup) {
                View.VISIBLE
            } else {
                View.GONE
            }

            // Set icon or screenshot thumbnail
            val screenshotPath = item.screenshotUrl
            if (screenshotPath != null) {
                if (screenshotPath.startsWith("https://")) {
                    // Cloud screenshot: load from Firebase Storage URL
                    ivBackupIcon.imageTintList = null
                    Glide
                        .with(itemView.context)
                        .load(screenshotPath)
                        .centerCrop()
                        .error(R.drawable.ic_cloud)
                        .into(ivBackupIcon)
                } else {
                    // Local screenshot: load from file
                    val file = File(screenshotPath)
                    if (file.exists()) {
                        ivBackupIcon.imageTintList = null
                        Glide
                            .with(itemView.context)
                            .load(file)
                            .signature(ObjectKey(file.lastModified()))
                            .centerCrop()
                            .into(ivBackupIcon)
                    } else {
                        showDefaultIcon(item.deviceType)
                    }
                }
            } else {
                showDefaultIcon(item.deviceType)
            }

            // Click listener - opens BottomSheet
            itemView.setOnClickListener {
                listener.onItemClick(item)
            }

            // Gamepad button support
            itemView.setOnKeyListener { _, keyCode, event ->
                if (event.action == KeyEvent.ACTION_DOWN) {
                    when (keyCode) {
                        KeyEvent.KEYCODE_BUTTON_A -> {
                            listener.onItemClick(item)
                            true
                        }
                        KeyEvent.KEYCODE_BUTTON_B -> {
                            (itemView.context as? ComponentActivity)
                                ?.onBackPressedDispatcher
                                ?.onBackPressed()
                            true
                        }
                        else -> false
                    }
                } else {
                    false
                }
            }
        }

        private fun showDefaultIcon(deviceType: DeviceType) {
            Glide.with(itemView.context).clear(ivBackupIcon)
            ivBackupIcon.imageTintList = ColorStateList.valueOf(
                ContextCompat.getColor(itemView.context, R.color.colorAccent),
            )
            when (deviceType) {
                DeviceType.CLOUD -> ivBackupIcon.setImageResource(R.drawable.ic_cloud)
                else -> ivBackupIcon.setImageResource(R.drawable.ic_save)
            }
        }
    }

    /**
     * DiffUtil callback for efficient list updates.
     */
    class BackupDiffCallback : DiffUtil.ItemCallback<BackupItem>() {
        override fun areItemsTheSame(oldItem: BackupItem, newItem: BackupItem): Boolean = oldItem.id == newItem.id && oldItem.deviceType == newItem.deviceType

        override fun areContentsTheSame(oldItem: BackupItem, newItem: BackupItem): Boolean = oldItem == newItem
    }
}
