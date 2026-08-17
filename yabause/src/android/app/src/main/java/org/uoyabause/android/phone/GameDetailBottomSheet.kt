package org.uoyabause.android.phone

import android.content.Intent
import android.os.Bundle
import android.util.Log
import android.view.KeyEvent
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ArrayAdapter
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.RatingBar
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.widget.ListPopupWindow
import com.bumptech.glide.Glide
import com.bumptech.glide.request.RequestOptions
import com.frybits.harmony.getHarmonySharedPreferences
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import com.google.android.material.button.MaterialButton
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.firestore.FirebaseFirestore
import jp.wasabeef.glide.transformations.BlurTransformation
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.tasks.await
import kotlinx.coroutines.withContext
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.GameInfo
import org.uoyabause.android.GameInfo.Companion.sigin
import org.uoyabause.android.ReportListActivity
import org.uoyabause.android.achievements.RetroAchievementsManager
import java.io.File

class GameDetailBottomSheet : BottomSheetDialogFragment() {
    interface Listener {
        fun onPlayGame(game: GameInfo)

        fun onDeleteGame(game: GameInfo)

        fun onCreateShortcut(game: GameInfo)
    }

    private var listener: Listener? = null
    private var gameInfo: GameInfo? = null

    fun setListener(l: Listener) {
        listener = l
    }

    fun setGameInfo(info: GameInfo) {
        gameInfo = info
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View? = inflater.inflate(R.layout.bottom_sheet_game_detail, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val game = gameInfo ?: return

        bindData(view, game)
        bindAchievementData(view, game)
        setupButtons(view, game)
        setupGamepadSupport(view)
    }

    private fun bindData(view: View, game: GameInfo) {
        val imgBoxart = view.findViewById<ImageView>(R.id.img_detail_boxart)
        val tvTitle = view.findViewById<TextView>(R.id.tv_detail_title)
        val tvProduct = view.findViewById<TextView>(R.id.tv_detail_product)
        val tvInfo = view.findViewById<TextView>(R.id.tv_detail_info)
        val tvDate = view.findViewById<TextView>(R.id.tv_detail_date)
        val ivDateIcon = view.findViewById<ImageView>(R.id.iv_date_icon)
        val ratingBar = view.findViewById<RatingBar>(R.id.rating_detail)

        tvTitle.text = game.game_title
        tvProduct.text = game.product_number

        // Info text: device info + area
        val infoText = buildString {
            if (game.device_infomation != "CD-1/1") append(game.device_infomation)
            if (game.area.isNotBlank()) {
                if (isNotEmpty()) append(" | ")
                append(game.area)
            }
        }
        tvInfo.text = infoText

        // Date
        if (game.release_date.isNotBlank()) {
            tvDate.text = game.release_date
            tvDate.visibility = View.VISIBLE
            ivDateIcon.visibility = View.VISIBLE
        } else {
            tvDate.visibility = View.GONE
            ivDateIcon.visibility = View.GONE
        }

        ratingBar.rating = game.rating.toFloat()
        ratingBar.contentDescription = getString(R.string.rating_description, game.rating)

        // Update rating asynchronously
        CoroutineScope(Dispatchers.IO).launch {
            game.updateState()
            withContext(Dispatchers.Main) {
                if (isAdded) {
                    ratingBar.rating = game.rating.toFloat()
                }
            }
        }

        // Limit boxart height to 40% of screen to prevent tall cover art from pushing content off-screen
        val displayMetrics = resources.displayMetrics
        imgBoxart.maxHeight = (displayMetrics.heightPixels * 0.4).toInt()

        // Load boxart
        loadBoxart(imgBoxart, game)
    }

    private fun bindAchievementData(view: View, game: GameInfo) {
        val raSection = view.findViewById<View>(R.id.ra_section)
        val tvAchievements = view.findViewById<TextView>(R.id.tv_ra_achievements)
        val raProgressBar = view.findViewById<com.google.android.material.progressindicator.LinearProgressIndicator>(R.id.ra_progress_bar)
        val raHardcoreSection = view.findViewById<View>(R.id.ra_hardcore_section)
        val tvHardcore = view.findViewById<TextView>(R.id.tv_ra_hardcore)
        val raProgressBarHardcore = view.findViewById<com.google.android.material.progressindicator.LinearProgressIndicator>(R.id.ra_progress_bar_hardcore)
        val raLeaderboardSection = view.findViewById<View>(R.id.ra_leaderboard_section)
        val raLeaderboardEntries = view.findViewById<LinearLayout>(R.id.ra_leaderboard_entries)

        if (game.raGameId == null || game.raNotSupported || game.raTotal == 0) {
            raSection.visibility = View.GONE
            return
        }

        raSection.visibility = View.VISIBLE

        // Softcore row
        tvAchievements.text = "${getString(R.string.softcore)}: ${game.raUnlocked} / ${game.raTotal}"
        val progressPct = if (game.raTotal > 0) (game.raUnlocked * 100) / game.raTotal else 0
        raProgressBar.progress = progressPct

        // Hardcore row (only shown when user has hardcore unlocks)
        if (game.raUnlockedHardcore > 0) {
            raHardcoreSection.visibility = View.VISIBLE
            tvHardcore.text = "${getString(R.string.hardcore)}: ${game.raUnlockedHardcore} / ${game.raTotal}"
            val hardcorePct = if (game.raTotal > 0) (game.raUnlockedHardcore * 100) / game.raTotal else 0
            raProgressBarHardcore.progress = hardcorePct
        } else {
            raHardcoreSection.visibility = View.GONE
        }

        // Reset leaderboard UI before async fetch
        raLeaderboardEntries.removeAllViews()
        raLeaderboardSection.visibility = View.GONE

        val raGameId = game.raGameId ?: return
        CoroutineScope(Dispatchers.IO).launch {
            val manager = RetroAchievementsManager.getInstance(requireContext())
            val leaderboards = manager.fetchGameLeaderboards(raGameId)
            withContext(Dispatchers.Main) {
                if (isAdded && leaderboards != null && leaderboards.isNotEmpty()) {
                    raLeaderboardEntries.removeAllViews()
                    raLeaderboardSection.visibility = View.VISIBLE
                    leaderboards.forEach { entry ->
                        val entryView = TextView(requireContext()).apply {
                            text = entry
                            setTextAppearance(com.google.android.material.R.style.TextAppearance_Material3_BodySmall)
                            setTextColor(requireContext().getColor(android.R.color.tab_indicator_text))
                            setPadding(0, 4, 0, 4)
                        }
                        raLeaderboardEntries.addView(entryView)
                    }
                }
            }
        }
    }

    private fun setupButtons(view: View, game: GameInfo) {
        val btnPlay = view.findViewById<MaterialButton>(R.id.btn_play)
        val btnDelete = view.findViewById<MaterialButton>(R.id.btn_delete)
        val btnShortcut = view.findViewById<MaterialButton>(R.id.btn_shortcut)
        val btnOverflow = view.findViewById<MaterialButton>(R.id.btn_overflow_menu)

        // Cloud-only game: hide delete and shortcut
        if (game.isCloudOnly) {
            btnDelete.visibility = View.GONE
            btnShortcut.visibility = View.GONE
            btnPlay.text = getString(R.string.download)
        }

        btnPlay.setOnClickListener {
            listener?.onPlayGame(game)
            dismiss()
        }

        btnDelete.setOnClickListener {
            val dialog = MaterialAlertDialogBuilder(requireContext())
                .setTitle(R.string.confirm_delete_title)
                .setMessage(getString(R.string.confirm_delete_message, game.game_title))
                .setPositiveButton(R.string.delete) { _, _ ->
                    listener?.onDeleteGame(game)
                    dismiss()
                }.setNegativeButton(R.string.cancel, null)
                .create()
            dialog.setOnShowListener {
                val negativeButton = dialog.getButton(android.content.DialogInterface.BUTTON_NEGATIVE)
                negativeButton?.post {
                    negativeButton.isFocusable = true
                    negativeButton.isFocusableInTouchMode = true
                    negativeButton.requestFocus()
                }
            }
            dialog.show()
        }

        btnShortcut.setOnClickListener {
            listener?.onCreateShortcut(game)
            dismiss()
        }

        btnOverflow.setOnClickListener {
            showOverflowMenu(it, game)
        }

        // Set initial focus to Play button
        btnPlay.post { btnPlay.requestFocus() }

        // Focus order depends on layout variant (two-row for landscape narrow screens, one-row otherwise)
        val isLandscape = resources.configuration.orientation == android.content.res.Configuration.ORIENTATION_LANDSCAPE
        val isTwoRowLayout = isLandscape && resources.configuration.screenWidthDp < 700
        if (game.isCloudOnly) {
            btnPlay.nextFocusLeftId = R.id.btn_play
            btnPlay.nextFocusRightId = R.id.btn_overflow_menu
            btnOverflow.nextFocusLeftId = R.id.btn_play
        } else if (isTwoRowLayout) {
            // Two-row: Row 1 Delete <-> Shortcut, Row 2 Play <-> Overflow
            btnDelete.nextFocusRightId = R.id.btn_shortcut
            btnDelete.nextFocusDownId = R.id.btn_play
            btnShortcut.nextFocusLeftId = R.id.btn_delete
            btnShortcut.nextFocusDownId = R.id.btn_play
            btnPlay.nextFocusLeftId = R.id.btn_play
            btnPlay.nextFocusRightId = R.id.btn_overflow_menu
            btnPlay.nextFocusUpId = R.id.btn_delete
            btnOverflow.nextFocusLeftId = R.id.btn_play
            btnOverflow.nextFocusUpId = R.id.btn_shortcut
        } else {
            // One-row: Delete <-> Shortcut <-> Play <-> Overflow
            btnDelete.nextFocusRightId = R.id.btn_shortcut
            btnShortcut.nextFocusLeftId = R.id.btn_delete
            btnShortcut.nextFocusRightId = R.id.btn_play
            btnPlay.nextFocusLeftId = R.id.btn_shortcut
            btnPlay.nextFocusRightId = R.id.btn_overflow_menu
            btnOverflow.nextFocusLeftId = R.id.btn_play
        }
    }

    private fun loadBoxart(imageView: ImageView, game: GameInfo) {
        if (game.image_url.isNullOrEmpty()) {
            Glide
                .with(imageView)
                .load(R.drawable.missing)
                .into(imageView)
            return
        }

        if (game.image_url!!.startsWith("http")) {
            var url = game.image_url
            if (game.isCloudOnly) {
                url += "?" + sigin
            }
            val request = Glide
                .with(imageView)
                .load(url)
                .error(R.drawable.missing)

            if (game.isCloudOnly) {
                request.apply(RequestOptions.bitmapTransform(BlurTransformation(8)))
            }
            request.into(imageView)
        } else {
            Glide
                .with(imageView)
                .load(game.image_url?.let { File(it) })
                .error(R.drawable.missing)
                .into(imageView)
        }
    }

    private fun setupGamepadSupport(view: View) {
        val buttons = listOf<View>(
            view.findViewById(R.id.btn_delete),
            view.findViewById(R.id.btn_shortcut),
            view.findViewById(R.id.btn_play),
            view.findViewById(R.id.btn_overflow_menu),
        )

        buttons.forEach { button ->
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

    private fun showOverflowMenu(view: View, game: GameInfo) {
        val options = arrayOf(
            getString(R.string.restore_defaults),
            getString(R.string.reproduce_report),
        )

        val listPopupWindow = ListPopupWindow(requireContext())
        listPopupWindow.anchorView = view

        val adapter = ArrayAdapter(
            requireContext(),
            android.R.layout.simple_list_item_1,
            options,
        )
        listPopupWindow.setAdapter(adapter)

        listPopupWindow.setOnItemClickListener { _, _, position, _ ->
            when (position) {
                0 -> handleRestoreDefaults(game)
                1 -> handleReproduceReport(game)
            }
            listPopupWindow.dismiss()
        }

        listPopupWindow.width = 400 // Set width in pixels
        listPopupWindow.isModal = true

        try {
            listPopupWindow.show()
        } catch (e: Exception) {
            Log.e(TAG, "Error showing ListPopupWindow", e)
            Toast.makeText(requireContext(), "Error: ${e.message}", Toast.LENGTH_SHORT).show()
        }
    }

    private fun handleRestoreDefaults(game: GameInfo) {
        val dialog = MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.restore_defaults)
            .setMessage(R.string.restore_defaults_confirm)
            .setPositiveButton(R.string.ok) { _, _ ->
                val gamePreference = requireContext()
                    .getHarmonySharedPreferences(game.product_number)
                gamePreference.edit().clear().apply()

                Toast
                    .makeText(
                        requireContext(),
                        R.string.restore_defaults_success,
                        Toast.LENGTH_SHORT,
                    ).show()
            }.setNegativeButton(R.string.cancel, null)
            .create()
        dialog.setOnShowListener {
            val negativeButton = dialog.getButton(android.content.DialogInterface.BUTTON_NEGATIVE)
            negativeButton?.post {
                negativeButton.isFocusable = true
                negativeButton.isFocusableInTouchMode = true
                negativeButton.requestFocus()
            }
        }
        dialog.show()
    }

    private fun handleReproduceReport(game: GameInfo) {
        // Firebase authentication check
        val auth = FirebaseAuth.getInstance()
        if (auth.currentUser == null) {
            Toast
                .makeText(
                    requireContext(),
                    "Please sign in to access reports",
                    Toast.LENGTH_SHORT,
                ).show()
            return
        }

        // Admin permission check
        CoroutineScope(Dispatchers.Main).launch {
            try {
                val isAdmin = checkIsAdmin(auth.currentUser!!.uid)

                if (isAdmin) {
                    // Launch ReportListActivity
                    val intent = Intent(requireContext(), ReportListActivity::class.java).apply {
                        putExtra(ReportListActivity.EXTRA_PRODUCT_NUMBER, game.product_number)
                        putExtra(ReportListActivity.EXTRA_GAME_TITLE, game.game_title)
                        putExtra(ReportListActivity.EXTRA_FILE_PATH, game.file_path)
                        putExtra(ReportListActivity.EXTRA_ISO_FILE_PATH, game.iso_file_path)
                    }
                    startActivity(intent)
                    dismiss()
                } else {
                    Toast
                        .makeText(
                            requireContext(),
                            "This feature is only available for administrators",
                            Toast.LENGTH_LONG,
                        ).show()
                }
            } catch (e: Exception) {
                Toast
                    .makeText(
                        requireContext(),
                        "Error: ${e.message}",
                        Toast.LENGTH_SHORT,
                    ).show()
            }
        }
    }

    private suspend fun checkIsAdmin(userId: String): Boolean = try {
        val db = FirebaseFirestore.getInstance()
        val adminDoc = withContext(Dispatchers.IO) {
            db
                .collection("admins")
                .document(userId)
                .get()
                .await()
        }
        adminDoc.exists()
    } catch (e: Exception) {
        false
    }

    companion object {
        const val TAG = "GameDetailBottomSheet"
    }
}
