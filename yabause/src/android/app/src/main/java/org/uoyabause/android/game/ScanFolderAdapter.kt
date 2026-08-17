/*  Copyright 2019 devMiyax(smiyaxdev@gmail.com)

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
package org.uoyabause.android.game

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageButton
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import org.devmiyax.yabasanshiro.R

class ScanFolderAdapter(
    private var mItems: List<ScanFolderItem> = emptyList(),
    private var mListener: OnFolderActionListener? = null,
) : RecyclerView.Adapter<ScanFolderAdapter.ViewHolder>() {
    interface OnFolderActionListener {
        fun onRescanClick(position: Int, item: ScanFolderItem)

        fun onDeleteClick(position: Int, item: ScanFolderItem)
    }

    fun setOnFolderActionListener(listener: OnFolderActionListener?) {
        mListener = listener
    }

    fun updateItems(items: List<ScanFolderItem>) {
        mItems = items
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val view = LayoutInflater
            .from(parent.context)
            .inflate(R.layout.item_scan_folder, parent, false)
        return ViewHolder(view)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val item = mItems[position]
        holder.mItem = item
        holder.mFolderName.text = item.displayName
        val infoText = buildInfoText(item)
        holder.mFolderInfo.text = infoText
        holder.mFolderInfo.visibility = if (infoText.isEmpty()) View.GONE else View.VISIBLE
        holder.mButtonRescan.setOnClickListener {
            mListener?.onRescanClick(holder.adapterPosition, item)
        }
        holder.mButtonDelete.setOnClickListener {
            mListener?.onDeleteClick(holder.adapterPosition, item)
        }
    }

    override fun getItemCount(): Int = mItems.size

    private fun buildInfoText(item: ScanFolderItem): String {
        val parts = mutableListOf<String>()
        if (item.fileCount > 0) {
            parts.add("${item.fileCount} files")
        }
        val timeText = item.getRelativeTimeText()
        if (timeText.isNotEmpty()) {
            parts.add(timeText)
        }
        return parts.joinToString(" · ")
    }

    inner class ViewHolder(
        val mView: View,
    ) : RecyclerView.ViewHolder(mView) {
        val mFolderName: TextView = mView.findViewById(R.id.folder_name)
        val mFolderInfo: TextView = mView.findViewById(R.id.folder_info)
        val mButtonRescan: ImageButton = mView.findViewById(R.id.button_rescan)
        val mButtonDelete: ImageButton = mView.findViewById(R.id.button_delete)
        var mItem: ScanFolderItem? = null
    }
}
