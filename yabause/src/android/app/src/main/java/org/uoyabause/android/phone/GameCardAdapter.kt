package org.uoyabause.android.phone

import android.view.KeyEvent
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.RatingBar
import android.widget.TextView
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.bumptech.glide.Glide
import com.bumptech.glide.request.RequestOptions
import com.google.android.material.card.MaterialCardView
import jp.wasabeef.glide.transformations.BlurTransformation
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.GameInfo
import org.uoyabause.android.GameInfo.Companion.sigin
import java.io.File

class GameCardAdapter(
    private val listener: OnItemClickListener,
) : ListAdapter<GameInfo, GameCardAdapter.ViewHolder>(GameDiffCallback()) {
    interface OnItemClickListener {
        fun onItemClick(position: Int, item: GameInfo)

        fun onItemLongClick(position: Int, item: GameInfo, anchor: View)

        fun onItemFocused(position: Int, item: GameInfo)

        fun onItemPlayGame(position: Int, item: GameInfo)

        fun onItemDPadRight(): Boolean
    }

    class ViewHolder(
        view: View,
    ) : RecyclerView.ViewHolder(view) {
        val card: MaterialCardView = view.findViewById(R.id.card_game)
        val imgBoxart: ImageView = view.findViewById(R.id.img_boxart)
        val tvTitle: TextView = view.findViewById(R.id.tv_game_title)
        val tvMeta: TextView = view.findViewById(R.id.tv_game_meta)
        val ratingBar: RatingBar = view.findViewById(R.id.rating_bar)
        val badgeCloud: ImageView = view.findViewById(R.id.badge_cloud)
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val view = LayoutInflater
            .from(parent.context)
            .inflate(R.layout.item_game_card, parent, false)
        return ViewHolder(view)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val game = getItem(position)
        val ctx = holder.itemView.context

        holder.tvTitle.text = game.game_title

        // Accessibility: box art and card contentDescription
        holder.imgBoxart.contentDescription = ctx.getString(R.string.box_art_description, game.game_title)
        holder.card.contentDescription = game.game_title

        // Cloud badge
        if (game.isCloudOnly) {
            holder.badgeCloud.visibility = View.VISIBLE
            holder.tvMeta.text = ctx.getString(R.string.cloud_only_game)
            holder.card.strokeWidth = holder.itemView.resources.getDimensionPixelSize(R.dimen.card_stroke_width_outlined)
        } else {
            holder.badgeCloud.visibility = View.GONE
            holder.tvMeta.text = if (game.device_infomation == "CD-1/1") "" else game.device_infomation
            holder.card.strokeWidth = 0
        }

        // Rating
        holder.ratingBar.rating = game.rating.toFloat()
        holder.ratingBar.contentDescription = ctx.getString(R.string.rating_description, game.rating)

        // Update rating asynchronously
        if (!game.isCloudOnly) {
            CoroutineScope(Dispatchers.IO).launch {
                game.updateState()
                withContext(Dispatchers.Main) {
                    holder.ratingBar.rating = game.rating.toFloat()
                    if (game.device_infomation != "CD-1/1") {
                        holder.tvMeta.text = game.device_infomation
                    }
                }
            }
        }

        // Load boxart image
        loadBoxart(holder, game)

        // D-pad focus animation + stroke management
        holder.card.setOnFocusChangeListener { v, hasFocus ->
            val scale = if (hasFocus) 1.02f else 1.0f
            val elevation = if (hasFocus) 4f * ctx.resources.displayMetrics.density else 0f
            v
                .animate()
                .scaleX(scale)
                .scaleY(scale)
                .translationZ(elevation)
                .setDuration(200)
                .start()

            if (hasFocus) {
                holder.card.strokeWidth = 2.dpToPx(ctx)
                holder.card.strokeColor = ctx.getColor(R.color.colorPrimary)
                val pos = holder.bindingAdapterPosition
                if (pos != RecyclerView.NO_POSITION) {
                    listener.onItemFocused(pos, getItem(pos))
                }
            } else if (!game.isCloudOnly) {
                holder.card.strokeWidth = 0
            }
        }

        // Click listeners
        holder.itemView.setOnClickListener {
            val pos = holder.bindingAdapterPosition
            if (pos != RecyclerView.NO_POSITION) {
                listener.onItemClick(pos, getItem(pos))
            }
        }

        holder.itemView.setOnLongClickListener {
            val pos = holder.bindingAdapterPosition
            if (pos != RecyclerView.NO_POSITION) {
                listener.onItemLongClick(pos, getItem(pos), it)
            }
            true
        }

        // Gamepad/Keyboard support for launching games directly
        holder.itemView.setOnKeyListener { _, keyCode, event ->
            if (event.action == KeyEvent.ACTION_DOWN) {
                when (keyCode) {
                    KeyEvent.KEYCODE_BUTTON_A,
                    KeyEvent.KEYCODE_ENTER,
                    KeyEvent.KEYCODE_DPAD_CENTER,
                    -> {
                        val pos = holder.bindingAdapterPosition
                        if (pos != RecyclerView.NO_POSITION) {
                            listener.onItemPlayGame(pos, getItem(pos))
                        }
                        true
                    }
                    KeyEvent.KEYCODE_DPAD_RIGHT -> {
                        // Move focus to detail panel button (landscape mode)
                        listener.onItemDPadRight()
                    }
                    else -> false
                }
            } else {
                false
            }
        }
    }

    private fun loadBoxart(holder: ViewHolder, game: GameInfo) {
        if (game.image_url.isNullOrEmpty()) {
            Glide
                .with(holder.imgBoxart)
                .load(R.drawable.missing)
                .into(holder.imgBoxart)
            return
        }

        if (game.image_url!!.startsWith("http")) {
            var url = game.image_url
            if (game.isCloudOnly) {
                url += "?" + sigin
            }
            val request = Glide
                .with(holder.imgBoxart)
                .load(url)
                .error(R.drawable.missing)

            if (game.isCloudOnly) {
                request.apply(RequestOptions.bitmapTransform(BlurTransformation(8)))
            }

            request.into(holder.imgBoxart)
        } else {
            Glide
                .with(holder.imgBoxart)
                .load(game.image_url?.let { File(it) })
                .error(R.drawable.missing)
                .into(holder.imgBoxart)
        }
    }

    private fun Int.dpToPx(ctx: android.content.Context): Int =
        (this * ctx.resources.displayMetrics.density).toInt()
}

class GameDiffCallback : DiffUtil.ItemCallback<GameInfo>() {
    override fun areItemsTheSame(oldItem: GameInfo, newItem: GameInfo): Boolean =
        oldItem.id == newItem.id && oldItem.file_path == newItem.file_path

    override fun areContentsTheSame(oldItem: GameInfo, newItem: GameInfo): Boolean =
        oldItem.game_title == newItem.game_title &&
            oldItem.rating == newItem.rating &&
            oldItem.isCloudOnly == newItem.isCloudOnly &&
            oldItem.device_infomation == newItem.device_infomation &&
            oldItem.raUnlocked == newItem.raUnlocked &&
            oldItem.raTotal == newItem.raTotal &&
            oldItem.raGameId == newItem.raGameId
}
