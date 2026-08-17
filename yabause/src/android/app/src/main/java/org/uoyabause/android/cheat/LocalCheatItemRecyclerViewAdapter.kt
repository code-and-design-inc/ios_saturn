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
import com.google.android.material.card.MaterialCardView
import com.google.android.material.color.MaterialColors
import com.google.android.material.materialswitch.MaterialSwitch
import org.devmiyax.yabasanshiro.R

class LocalCheatItemRecyclerViewAdapter(
    private val values: List<CheatItem?>?,
    private var listener: OnItemClickListener?,
) : RecyclerView.Adapter<LocalCheatItemRecyclerViewAdapter.ViewHolder>() {
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
        listener?.onItemClick(focusedItem, holder.item, holder.itemView)
    }

    private fun tryMoveSelection(
        lm: RecyclerView.LayoutManager?,
        direction: Int,
    ): Boolean {
        val tryFocusItem = focusedItem + direction

        if (tryFocusItem >= 0 && tryFocusItem < itemCount) {
            notifyItemChanged(focusedItem)
            focusedItem = tryFocusItem
            notifyItemChanged(focusedItem)
            lm!!.scrollToPosition(focusedItem)
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
        )
    }

    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int,
    ): ViewHolder {
        val view =
            LayoutInflater
                .from(parent.context)
                .inflate(R.layout.fragment_localcheatitem, parent, false)
        return ViewHolder(view)
    }

    override fun onBindViewHolder(
        holder: ViewHolder,
        @SuppressLint("RecyclerView") position: Int,
    ) {
        if (values == null) return

        holder.item = values.get(position)
        holder.idView.setText(values.get(position)?.description)
        holder.contentView.setText(values.get(position)?.cheat_code)
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

        holder.switchEnable.isChecked = holder.item?.enable == true

        holder.itemView.setOnClickListener {
            notifyItemChanged(focusedItem)
            focusedItem = position
            notifyItemChanged(focusedItem)
            if (null != listener) {
                listener!!.onItemClick(position, holder.item, holder.itemView)
            }
        }
    }

    override fun getItemCount(): Int = values?.size ?: 0

    inner class ViewHolder(
        val view: View,
    ) : RecyclerView.ViewHolder(
            view,
        ) {
        val idView: TextView
        val contentView: TextView
        var switchEnable: MaterialSwitch
        var item: CheatItem? = null

        override fun toString(): String = super.toString() + " '" + contentView.text + "'"

        init {
            idView = view.findViewById(R.id.id)
            contentView = view.findViewById(R.id.content)
            switchEnable = view.findViewById(R.id.checkBox_enable)
            switchEnable.isEnabled = false
            switchEnable.isFocusable = false
        }
    }
}
