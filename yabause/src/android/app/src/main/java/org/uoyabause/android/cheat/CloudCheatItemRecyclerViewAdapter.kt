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
package org.uoyabause.android.cheat

import android.annotation.SuppressLint
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.button.MaterialButton
import com.google.android.material.card.MaterialCardView
import com.google.android.material.color.MaterialColors
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.materialswitch.MaterialSwitch
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.auth.AuthState

/**
 * [RecyclerView.Adapter] that can display a [CheatItem] and makes a call to the
 * specified [OnListFragmentInteractionListener].
 */
class CloudCheatItemRecyclerViewAdapter(
    private val values: List<CheatItem?>?,
    private var listener: OnItemClickListener?,
) : RecyclerView.Adapter<CloudCheatItemRecyclerViewAdapter.ViewHolder>() {
    private var focusedItem = 0
    private var attachedRecyclerView: RecyclerView? = null

    override fun onAttachedToRecyclerView(recyclerView: RecyclerView) {
        super.onAttachedToRecyclerView(recyclerView)
        attachedRecyclerView = recyclerView
    }

    override fun onDetachedFromRecyclerView(recyclerView: RecyclerView) {
        super.onDetachedFromRecyclerView(recyclerView)
        attachedRecyclerView = null
    }

    fun moveCursor(direction: Int) {
        val rv = attachedRecyclerView ?: return
        tryMoveSelection(rv.layoutManager, direction)
    }

    fun selectFocused() {
        val rv = attachedRecyclerView ?: return
        val holder = rv.findViewHolderForAdapterPosition(focusedItem) as? ViewHolder ?: return
        holder.view.performClick()
    }

    private fun tryMoveSelection(
        lm: RecyclerView.LayoutManager?,
        direction: Int,
    ): Boolean {
        val next = focusedItem + direction
        if (next in 0 until itemCount) {
            notifyItemChanged(focusedItem)
            focusedItem = next
            notifyItemChanged(focusedItem)
            lm?.scrollToPosition(focusedItem)
            return true
        }
        return false
    }

    fun setOnItemClickListener(itemClickListener: OnItemClickListener?) {
        listener = itemClickListener
    }

    interface OnItemClickListener {
        fun onItemClick(
            position: Int,
            item: CheatItem?,
            v: View?,
            isLikeButton: Boolean,
        )

        fun onItemDelete(
            position: Int,
            item: CheatItem?,
        )
    }

    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int,
    ): ViewHolder {
        val view =
            LayoutInflater
                .from(parent.context)
                .inflate(R.layout.fragment_cloudcheatitem, parent, false)
        return ViewHolder(view)
    }

    override fun onBindViewHolder(
        holder: ViewHolder,
        @SuppressLint("RecyclerView") position: Int,
    ) {
        holder.item = values?.get(position)
        holder.idView.text = values?.get(position)?.description
        holder.contentView.text = values?.get(position)?.cheat_code
        holder.itemView.isSelected = focusedItem == position

        val card = holder.itemView as MaterialCardView
        if (focusedItem == position) {
            card.strokeColor = MaterialColors.getColor(card, androidx.appcompat.R.attr.colorPrimary, 0)
            card.strokeWidth = card.resources.getDimensionPixelSize(R.dimen.card_stroke_width_focused)
            card.setCardBackgroundColor(
                MaterialColors.getColor(card, com.google.android.material.R.attr.colorSurfaceContainerHigh, 0),
            )
        } else {
            card.strokeColor = 0
            card.strokeWidth = 0
            card.setCardBackgroundColor(
                MaterialColors.getColor(card, com.google.android.material.R.attr.colorSurface, 0),
            )
        }

        holder.likeCount.text = holder.item?.star_count?.toString() ?: "0"

        // Set enable state
        holder.switchEnable.isChecked = holder.item?.enable == true

        holder.switchEnable.setOnCheckedChangeListener { _, isChecked ->
            if (null != listener) {
                if (isChecked != holder.item?.enable) {
                    listener!!.onItemClick(position, holder.item, holder.view, false)
                }
            }
        }

        holder.likeButton.setOnClickListener {
            if (null != listener) {
                listener!!.onItemClick(position, holder.item, holder.view, true)
            }
        }

        holder.view.setOnClickListener {
            focusedItem = position

            val context = holder.view.context
            val enableText = if (holder.switchEnable.isChecked) {
                context.getString(R.string.disable)
            } else {
                context.getString(R.string.enable)
            }

            val currentUserId = AuthState.realUser()?.uid
            val isOwner = currentUserId != null && currentUserId == holder.item?.user_id

            val options = if (isOwner) {
                arrayOf(
                    context.getString(R.string.toggle_like),
                    enableText,
                    context.getString(R.string.delete),
                )
            } else {
                arrayOf(
                    context.getString(R.string.toggle_like),
                    enableText,
                )
            }

            MaterialAlertDialogBuilder(context)
                .setTitle(holder.item?.description)
                .setItems(options) { _, which ->
                    when (which) {
                        0 -> {
                            if (null != listener) {
                                listener!!.onItemClick(position, holder.item, holder.view, true)
                            }
                        }
                        1 -> {
                            if (null != listener) {
                                listener!!.onItemClick(position, holder.item, holder.view, false)
                            }
                        }
                        2 -> {
                            if (isOwner && null != listener) {
                                listener!!.onItemDelete(position, holder.item)
                            }
                        }
                    }
                }.show()
        }
    }

    override fun getItemCount(): Int = if (values == null) 0 else values.size

    inner class ViewHolder(
        val view: View,
    ) : RecyclerView.ViewHolder(view) {
        val idView: TextView = view.findViewById(R.id.id)
        val contentView: TextView = view.findViewById(R.id.content)
        val likeButton: MaterialButton = view.findViewById(R.id.button_like)
        val likeCount: TextView = view.findViewById(R.id.text_like_count)
        val switchEnable: MaterialSwitch = view.findViewById(R.id.switch_enable)
        var item: CheatItem? = null

        override fun toString(): String = super.toString() + " '" + contentView.text + "'"
    }
}
