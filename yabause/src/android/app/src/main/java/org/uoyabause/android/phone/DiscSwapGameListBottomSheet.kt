package org.uoyabause.android.phone

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import android.view.KeyEvent
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.TextView
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.bumptech.glide.Glide
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialog
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.GameInfo
import org.uoyabause.android.GameInfo.Companion.sigin
import org.uoyabause.android.YabauseStorage
import java.io.File

class DiscSwapGameListBottomSheet : BottomSheetDialogFragment() {

    interface Listener {
        fun onGameSelectedForDiscSwap(gameInfo: GameInfo)
        fun onDiscSwapCancelled()
    }

    private var listener: Listener? = null
    private var gameSelected = false

    fun setListener(l: Listener) {
        listener = l
    }

    override fun onDismiss(dialog: DialogInterface) {
        super.onDismiss(dialog)
        if (!gameSelected) {
            listener?.onDiscSwapCancelled()
        }
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val dialog = super.onCreateDialog(savedInstanceState) as BottomSheetDialog
        dialog.setOnShowListener {
            val bottomSheet = dialog.findViewById<FrameLayout>(
                com.google.android.material.R.id.design_bottom_sheet,
            )
            bottomSheet?.let {
                val behavior = BottomSheetBehavior.from(it)
                // Expand to full screen height
                val displayMetrics = resources.displayMetrics
                val screenHeight = displayMetrics.heightPixels
                behavior.peekHeight = screenHeight
                behavior.maxHeight = screenHeight
                behavior.state = BottomSheetBehavior.STATE_EXPANDED
                behavior.skipCollapsed = true
            }
        }
        return dialog
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View? = inflater.inflate(R.layout.bottom_sheet_disc_swap, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val recyclerView = view.findViewById<RecyclerView>(R.id.recycler_disc_swap)
        val emptyLayout = view.findViewById<View>(R.id.empty_layout)

        recyclerView.layoutManager = LinearLayoutManager(requireContext())

        lifecycleScope.launch(Dispatchers.IO) {
            val currentGameCode = arguments?.getString(ARG_CURRENT_GAME_CODE)
            val allGames = YabauseStorage.dao.getAllSortedByTitle()
            val games = if (currentGameCode.isNullOrBlank()) {
                allGames
            } else {
                val (sameCode, others) = allGames.partition { it.product_number == currentGameCode }
                sameCode.sortedBy { it.device_infomation } + others
            }
            withContext(Dispatchers.Main) {
                if (!isAdded) return@withContext
                if (games.isEmpty()) {
                    recyclerView.visibility = View.GONE
                    emptyLayout.visibility = View.VISIBLE
                } else {
                    recyclerView.visibility = View.VISIBLE
                    emptyLayout.visibility = View.GONE
                    val adapter = DiscSwapAdapter(games) { game ->
                        gameSelected = true
                        listener?.onGameSelectedForDiscSwap(game)
                        dismiss()
                    }
                    recyclerView.adapter = adapter
                    setupRecyclerViewDpad(recyclerView)
                }
            }
        }
    }

    private fun setupRecyclerViewDpad(recyclerView: RecyclerView) {
        // Focus first item after layout
        recyclerView.post {
            val firstHolder = recyclerView.findViewHolderForAdapterPosition(0)
            firstHolder?.itemView?.let {
                it.isFocusable = true
                it.isFocusableInTouchMode = true
                it.requestFocus()
            }
        }

        // Ensure RecyclerView scrolls to focused item
        recyclerView.addOnChildAttachStateChangeListener(
            object : RecyclerView.OnChildAttachStateChangeListener {
                override fun onChildViewAttachedToWindow(view: View) {
                    view.isFocusable = true
                    view.isFocusableInTouchMode = true
                }
                override fun onChildViewDetachedFromWindow(view: View) {}
            },
        )
    }

    private class DiscSwapAdapter(
        private val games: List<GameInfo>,
        private val onItemClick: (GameInfo) -> Unit,
    ) : RecyclerView.Adapter<DiscSwapAdapter.ViewHolder>() {

        class ViewHolder(view: View) : RecyclerView.ViewHolder(view) {
            val imgBoxart: ImageView = view.findViewById(R.id.img_boxart)
            val tvTitle: TextView = view.findViewById(R.id.tv_game_title)
            val tvProduct: TextView = view.findViewById(R.id.tv_product_number)
        }

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
            val view = LayoutInflater.from(parent.context)
                .inflate(R.layout.item_disc_swap_game, parent, false)
            return ViewHolder(view)
        }

        override fun onBindViewHolder(holder: ViewHolder, position: Int) {
            val game = games[position]
            holder.tvTitle.text = game.game_title
            holder.tvProduct.text = buildString {
                append(game.device_infomation)
                if (game.product_number.isNotBlank()) {
                    append(" | ")
                    append(game.product_number)
                }
            }

            loadBoxart(holder.imgBoxart, game)

            holder.itemView.setOnClickListener {
                onItemClick(game)
            }

            // Gamepad support: A button / Enter / D-pad Center
            holder.itemView.setOnKeyListener { v, keyCode, event ->
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
            holder.itemView.setOnFocusChangeListener { v, hasFocus ->
                val scale = if (hasFocus) 1.03f else 1.0f
                val alpha = if (hasFocus) 1.0f else 0.9f
                v.animate()
                    .scaleX(scale).scaleY(scale)
                    .alpha(alpha)
                    .setDuration(150)
                    .start()
            }
        }

        override fun getItemCount(): Int = games.size

        private fun loadBoxart(imageView: ImageView, game: GameInfo) {
            if (game.image_url.isNullOrEmpty()) {
                Glide.with(imageView)
                    .load(R.drawable.missing)
                    .into(imageView)
                return
            }

            if (game.image_url!!.startsWith("http")) {
                var url = game.image_url
                if (game.isCloudOnly) {
                    url += "?" + sigin
                }
                Glide.with(imageView)
                    .load(url)
                    .error(R.drawable.missing)
                    .into(imageView)
            } else {
                Glide.with(imageView)
                    .load(game.image_url?.let { File(it) })
                    .error(R.drawable.missing)
                    .into(imageView)
            }
        }
    }

    companion object {
        const val TAG = "DiscSwapGameList"
        private const val ARG_CURRENT_GAME_CODE = "current_game_code"

        fun newInstance(currentGameCode: String?): DiscSwapGameListBottomSheet {
            return DiscSwapGameListBottomSheet().apply {
                arguments = Bundle().apply {
                    putString(ARG_CURRENT_GAME_CODE, currentGameCode)
                }
            }
        }
    }
}
