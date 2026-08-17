/*  Copyright 2025 devMiyax(smiyaxdev@gmail.com)

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
package org.uoyabause.android

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import androidx.cardview.widget.CardView
import androidx.recyclerview.widget.RecyclerView
import com.bumptech.glide.Glide
import com.google.android.material.card.MaterialCardView
import org.devmiyax.yabasanshiro.R

/**
 * Adapter for displaying report list in RecyclerView
 */
class ReportListAdapter(
    private val reports: List<ReportData>,
    private val onReportClick: (ReportData) -> Unit,
) : RecyclerView.Adapter<ReportListAdapter.ReportViewHolder>() {
    inner class ReportViewHolder(
        itemView: View,
    ) : RecyclerView.ViewHolder(itemView) {
        val reportCard: MaterialCardView = itemView.findViewById(R.id.report_card)
        val userNameText: TextView = itemView.findViewById(R.id.user_name_text)
        val timestampText: TextView = itemView.findViewById(R.id.timestamp_text)
        val ratingText: TextView = itemView.findViewById(R.id.rating_text)
        val commentText: TextView = itemView.findViewById(R.id.comment_text)
        val screenshotCard: CardView = itemView.findViewById(R.id.screenshot_card)
        val screenshotImage: ImageView = itemView.findViewById(R.id.screenshot_image)
        val attachmentInfoLayout: View = itemView.findViewById(R.id.attachment_info_layout)
        val attachmentInfoText: TextView = itemView.findViewById(R.id.attachment_info_text)
        val deviceInfoText: TextView = itemView.findViewById(R.id.device_info_text)

        init {
            reportCard.setOnClickListener {
                val position = bindingAdapterPosition
                if (position != RecyclerView.NO_POSITION) {
                    onReportClick(reports[position])
                }
            }
        }
    }

    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int,
    ): ReportViewHolder {
        val view =
            LayoutInflater
                .from(parent.context)
                .inflate(R.layout.item_report, parent, false)
        return ReportViewHolder(view)
    }

    override fun onBindViewHolder(
        holder: ReportViewHolder,
        position: Int,
    ) {
        val report = reports[position]
        val context = holder.itemView.context

        // Set user name
        holder.userNameText.text = report.display_name ?: "Unknown User"

        // Set timestamp
        holder.timestampText.text = report.getFormattedTimestamp()

        // Set rating stars
        holder.ratingText.text = report.getRatingStars()

        // Set comment
        if (report.comment.isNotEmpty()) {
            holder.commentText.text = report.comment
            holder.commentText.visibility = View.VISIBLE
        } else {
            holder.commentText.visibility = View.GONE
        }

        // Handle screenshot
        if (!report.screenshot_url.isNullOrEmpty()) {
            holder.screenshotCard.visibility = View.VISIBLE
            Glide
                .with(holder.screenshotImage)
                .load(report.screenshot_url)
                .placeholder(R.drawable.missing)
                .error(R.drawable.missing)
                .centerCrop()
                .into(holder.screenshotImage)
        } else {
            holder.screenshotCard.visibility = View.GONE
        }

        // Build attachment info text
        if (report.hasAnyAttachments()) {
            holder.attachmentInfoLayout.visibility = View.VISIBLE

            val attachments = mutableListOf<String>()
            if (!report.screenshot_url.isNullOrEmpty()) {
                attachments.add(context.getString(R.string.screenshot))
            }
            if (!report.savestate_url.isNullOrEmpty()) {
                attachments.add(context.getString(R.string.savestate))
            }
            if (!report.memory_url.isNullOrEmpty()) {
                attachments.add(context.getString(R.string.memory))
            }

            val attachmentText =
                if (report.attachment_size > 0) {
                    "${attachments.joinToString(", ")} (${report.getFormattedAttachmentSize()})"
                } else {
                    attachments.joinToString(", ")
                }

            holder.attachmentInfoText.text = attachmentText
        } else {
            holder.attachmentInfoLayout.visibility = View.GONE
        }

        // Device info (model / OS / GPU) - shown only when present
        val deviceParts =
            listOf(report.device_model, report.os_version, report.gpu_renderer)
                .filter { it.isNotBlank() }
        if (deviceParts.isNotEmpty()) {
            holder.deviceInfoText.text = deviceParts.joinToString(" / ")
            holder.deviceInfoText.visibility = View.VISIBLE
        } else {
            holder.deviceInfoText.visibility = View.GONE
        }
    }

    override fun getItemCount(): Int = reports.size
}
