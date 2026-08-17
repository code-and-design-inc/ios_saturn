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

import android.view.KeyEvent
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import androidx.activity.ComponentActivity
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.bumptech.glide.Glide
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.backup.model.SharedBackupItem

/**
 * RecyclerView adapter for displaying shared backup items.
 * Uses ListAdapter with DiffUtil for efficient updates.
 */
class SharedBackupListAdapter(
    private val listener: OnItemClickListener,
) : ListAdapter<SharedBackupItem, SharedBackupListAdapter.SharedBackupViewHolder>(SharedBackupDiffCallback()) {
    interface OnItemClickListener {
        fun onItemClick(item: SharedBackupItem)
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): SharedBackupViewHolder {
        val view = LayoutInflater
            .from(parent.context)
            .inflate(R.layout.item_shared_backup, parent, false)
        return SharedBackupViewHolder(view)
    }

    override fun onBindViewHolder(holder: SharedBackupViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    inner class SharedBackupViewHolder(
        itemView: View,
    ) : RecyclerView.ViewHolder(itemView) {
        private val ivScreenshot: ImageView = itemView.findViewById(R.id.iv_screenshot)
        private val tvGameTitle: TextView = itemView.findViewById(R.id.tv_game_title)
        private val tvFilename: TextView = itemView.findViewById(R.id.tv_filename)
        private val ivOwnerPhoto: ImageView = itemView.findViewById(R.id.iv_owner_photo)
        private val tvOwnerName: TextView = itemView.findViewById(R.id.tv_owner_name)
        private val tvRating: TextView = itemView.findViewById(R.id.tv_rating)
        private val tvDownloads: TextView = itemView.findViewById(R.id.tv_downloads)

        fun bind(item: SharedBackupItem) {
            tvGameTitle.text = item.gameTitle.ifEmpty { "Unknown Game" }
            tvFilename.text = item.filename

            // Owner info
            tvOwnerName.text = item.ownerName
            if (!item.ownerPhotoUrl.isNullOrEmpty()) {
                Glide
                    .with(itemView.context)
                    .load(item.ownerPhotoUrl)
                    .placeholder(R.drawable.ic_account_circle_24)
                    .circleCrop()
                    .into(ivOwnerPhoto)
            } else {
                ivOwnerPhoto.setImageResource(R.drawable.ic_account_circle_24)
            }

            // Screenshot
            if (!item.screenshotUrl.isNullOrEmpty()) {
                ivScreenshot.imageTintList = null
                Glide
                    .with(itemView.context)
                    .load(item.screenshotUrl)
                    .placeholder(R.drawable.ic_save)
                    .centerCrop()
                    .into(ivScreenshot)
            } else {
                ivScreenshot.setImageResource(R.drawable.ic_save)
            }

            // Rating and downloads
            tvRating.text = item.displayRating
            tvDownloads.text = item.downloadCount.toString()

            // Click listener
            itemView.setOnClickListener {
                listener.onItemClick(item)
            }

            // Gamepad button support
            itemView.setOnKeyListener { _, keyCode, event ->
                if (event.action == KeyEvent.ACTION_DOWN) {
                    when (keyCode) {
                        KeyEvent.KEYCODE_BUTTON_A,
                        KeyEvent.KEYCODE_DPAD_CENTER,
                        KeyEvent.KEYCODE_ENTER,
                        -> {
                            itemView.performClick()
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
    }

    /**
     * DiffUtil callback for efficient list updates.
     */
    class SharedBackupDiffCallback : DiffUtil.ItemCallback<SharedBackupItem>() {
        override fun areItemsTheSame(oldItem: SharedBackupItem, newItem: SharedBackupItem): Boolean = oldItem.id == newItem.id

        override fun areContentsTheSame(oldItem: SharedBackupItem, newItem: SharedBackupItem): Boolean = oldItem == newItem
    }
}
