@file:OptIn(kotlinx.coroutines.DelicateCoroutinesApi::class)

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

package org.uoyabause.android.phone

import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.content.pm.ShortcutInfo
import android.content.pm.ShortcutManager
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.Drawable
import android.graphics.drawable.Icon
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.view.ContextMenu
import android.view.LayoutInflater
import android.view.MenuInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Filter
import android.widget.Filterable
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.widget.PopupMenu
import androidx.cardview.widget.CardView
import androidx.recyclerview.widget.RecyclerView
import com.bumptech.glide.Glide
import com.bumptech.glide.load.engine.GlideException
import com.bumptech.glide.request.RequestListener
import com.bumptech.glide.request.RequestOptions
import com.bumptech.glide.request.RequestOptions.bitmapTransform
import com.frybits.harmony.getHarmonySharedPreferences
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.firebase.analytics.FirebaseAnalytics
import com.google.firebase.auth.FirebaseAuth
import jp.wasabeef.glide.transformations.BlurTransformation
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.tasks.await
import kotlinx.coroutines.withContext
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.GameInfo
import org.uoyabause.android.GameInfo.Companion.sigin
import org.uoyabause.android.YabauseStorage
import org.uoyabause.android.phone.GameItemAdapter.GameViewHolder
import java.io.File
import java.util.Locale
import javax.sql.DataSource

class GameItemAdapter(
    private val originalDataSet: MutableList<GameInfo?>?,
) : RecyclerView.Adapter<GameViewHolder>(),
    Filterable {
    private var dataSet: MutableList<GameInfo?>? = originalDataSet?.toMutableList()
    private var currentSearchQuery: String = ""

    // 表示モード定数（companion objectは後で統合）

    // 現在の表示モード（デフォルトは詳細表示）
    private var viewMode = Companion.VIEW_TYPE_DETAILED

    // 選択されたアイテムの位置
    private var selectedPosition = -1

    fun setViewMode(mode: Int) {
        if (viewMode != mode) {
            viewMode = mode
            notifyDataSetChanged()
        }
    }

    fun getViewMode(): Int = viewMode

    fun setSelectedPosition(position: Int) {
        val oldPosition = selectedPosition
        selectedPosition = position
        Log.d("GameItemAdapter", "setSelectedPosition: oldPosition=$oldPosition, newPosition=$position")

        // 古い選択を更新
        if (oldPosition != -1) {
            Log.d("GameItemAdapter", "notifyItemChanged for old position: $oldPosition")
            notifyItemChanged(oldPosition)
        }
        // 新しい選択を更新
        if (selectedPosition != -1) {
            Log.d("GameItemAdapter", "notifyItemChanged for new position: $selectedPosition")
            notifyItemChanged(selectedPosition)
            // 選択変更を通知
            mListener?.onItemSelected(selectedPosition, dataSet?.get(selectedPosition))
        }
    }

    fun getSelectedPosition(): Int = selectedPosition

    fun getSelectedGame(): GameInfo? = if (selectedPosition >= 0 && selectedPosition < (dataSet?.size ?: 0)) {
        dataSet?.get(selectedPosition)
    } else {
        null
    }

    class GameViewHolder(
        var rootview: View,
    ) : RecyclerView.ViewHolder(rootview),
        View.OnCreateContextMenuListener {
        var textViewName: TextView
        var textViewVersion: TextView
        var imageViewIcon: ImageView
        var menuButton: ImageButton

        init {
            textViewName =
                rootview.findViewById<View>(R.id.textViewName) as TextView
            textViewVersion =
                rootview.findViewById<View>(R.id.textViewVersion) as TextView
            imageViewIcon =
                rootview.findViewById<View>(R.id.imageView) as ImageView

            if (Build.VERSION.SDK_INT > 23) {
                var card = rootview.findViewById<View>(R.id.card_view_main) as CardView
                card.setCardBackgroundColor(rootview.context.getColorStateList(R.color.card_view_selectable))
                card.isFocusable = true
                card.setOnCreateContextMenuListener(this)
            }

            menuButton = rootview.findViewById<View>(R.id.game_card_menu) as ImageButton

            /*
            ViewGroup.LayoutParams lp = imageViewIcon.getLayoutParams();
            lp.width = CARD_WIDTH;
            lp.height = CARD_HEIGHT;
            imageViewIcon.setLayoutParams(lp);
             */
        }

        override fun onCreateContextMenu(
            menu: ContextMenu?,
            v: View?,
            menuInfo: ContextMenu.ContextMenuInfo?,
        ) {
        }
    }

    override fun getItemViewType(position: Int): Int = viewMode

    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int,
    ): GameViewHolder {
        val layoutId =
            when (viewType) {
                Companion.VIEW_TYPE_COMPACT -> R.layout.cards_layout_compact
                Companion.VIEW_TYPE_TEXT_ONLY -> R.layout.cards_layout_text_only
                Companion.VIEW_TYPE_SLIM -> R.layout.cards_layout_slim
                else -> R.layout.cards_layout
            }

        val view =
            LayoutInflater
                .from(parent.context)
                .inflate(layoutId, parent, false)
        view.setOnClickListener(GameSelectFragmentPhone.myOnClickListener)

        // work here if you need to control height of your items
        // keep in mind that parent is RecyclerView in this case
        // val height = 10;
        // view.minimumHeight = height

/*
        if (parent?.resources?.configuration?.orientation == Configuration.ORIENTATION_LANDSCAPE) {
            try {
                val lp: GridLayoutManager.LayoutParams =
                    view.getLayoutParams() as GridLayoutManager.LayoutParams

                val tv = TypedValue()
                if (parent.context.theme.resolveAttribute(android.R.attr.actionBarSize, tv, true)) {
                    val actionBarHeight = TypedValue.complexToDimensionPixelSize(tv.data,
                        parent.resources.displayMetrics)
                    lp.height = (parent.getMeasuredHeight() - actionBarHeight - 48 )
                    view.setLayoutParams(lp)
                }

            } catch (e: Exception) {

            }
        }else{
            val tv = TypedValue()
            if (parent.context.theme.resolveAttribute(android.R.attr.actionBarSize, tv, true)) {
                val actionBarHeight = TypedValue.complexToDimensionPixelSize(tv.data,
                    parent.resources.displayMetrics)
                view.minimumHeight = (parent.getMeasuredHeight() - actionBarHeight - 48 ) / 4
            }

        }
*/
        return GameViewHolder(view)
    }

    private var mListener: OnItemClickListener? = null

    fun setOnItemClickListener(listener: OnItemClickListener?) {
        mListener = listener
    }

    interface OnItemClickListener {
        fun onItemClick(
            position: Int,
            item: GameInfo?,
            v: View?,
        )

        fun onGameRemoved(item: GameInfo?)

        fun onItemSelected(
            position: Int,
            item: GameInfo?,
        )

        fun onGameStart(item: GameInfo?)
    }

    @SuppressLint("NotifyDataSetChanged", "CheckResult")
    override fun onBindViewHolder(
        holder: GameViewHolder,
        position: Int,
    ) {
        val textViewName = holder.textViewName
        val textViewVersion = holder.textViewVersion
        val imageView = holder.imageViewIcon
        val ctx = holder.rootview.context
        val game = dataSet?.get(position)
        if (game != null) {
            textViewName.text = game.game_title

            // Set card appearance based on whether it's a cloud-only game
            val cardView = holder.rootview.findViewById<CardView>(R.id.card_view_main)

            if (game.isCloudOnly) {
                // Apply toned-down appearance for cloud-only games
                textViewVersion.text = ctx.getString(R.string.cloud_only_game)

                // Add cloud icon indicator
                textViewVersion.setCompoundDrawablesWithIntrinsicBounds(
                    R.drawable.cloud_upload_48px,
                    0,
                    0,
                    0,
                )
                textViewVersion.compoundDrawablePadding = 8
            } else {
                // Regular appearance for local games
                textViewVersion.setCompoundDrawablesWithIntrinsicBounds(0, 0, 0, 0)

                // Set device information
                textViewVersion.text = ""
                if (game.device_infomation == "CD-1/1") {
                } else {
                    textViewVersion.text = game.device_infomation
                }

                // Update game state and rating
                CoroutineScope(Dispatchers.IO).launch {
                    game.updateState()
                    var rate = ""
                    for (i in 0 until game.rating) rate += "★"
                    if (game.device_infomation == "CD-1/1") {
                    } else {
                        rate += " " + game.device_infomation
                    }
                    withContext(Dispatchers.Main) {
                        textViewVersion.text = rate
                        // 非同期処理後に選択状態の色を再適用
                        val isSelected = position == selectedPosition
                        if (isSelected) {
                            textViewVersion.setTextColor(ctx.getColor(android.R.color.white))
                        } else {
                            textViewVersion.setTextColor(ctx.getColor(R.color.lb_default_brand_color_dark))
                        }
                    }
                }
            }

            // 選択状態を反映（クラウドゲーム処理の後に適用）
            val isSelected = position == selectedPosition
            Log.d("GameItemAdapter", "onBindViewHolder: position=$position, selectedPosition=$selectedPosition, isSelected=$isSelected")
            if (isSelected) {
                cardView.setCardBackgroundColor(ctx.getColor(R.color.colorPrimaryDark))
                textViewName.setTextColor(ctx.getColor(R.color.colorAccent))
                textViewVersion.setTextColor(ctx.getColor(R.color.colorAccent))
                // 選択時はアルファ値を1.0にして見やすくする
                cardView.alpha = 1.0f
                textViewName.alpha = 1.0f
                Log.d("GameItemAdapter", "Applied selected style to position $position")
            } else {
                cardView.setCardBackgroundColor(ctx.getColor(R.color.default_background))
                textViewName.setTextColor(ctx.getColor(R.color.colorPrimaryDark))
                textViewVersion.setTextColor(ctx.getColor(R.color.colorPrimaryDark))
                // 非選択時はクラウドゲームの場合のアルファ値を適用
                if (game.isCloudOnly) {
                    cardView.alpha = 0.8f
                    textViewName.alpha = 0.7f
                } else {
                    cardView.alpha = 1.0f
                    textViewName.alpha = 1.0f
                }
                Log.d("GameItemAdapter", "Applied unselected style to position $position")
            }

            // Load image (skip for text-only mode)
            if (viewMode != Companion.VIEW_TYPE_TEXT_ONLY && game.image_url != null && game.image_url != "") {
                if (game.image_url!!.startsWith("http")) {
                    var url = game.image_url
                    if (game.isCloudOnly) {
                        url += "?" + sigin
                    }
                    val glideRequest =
                        Glide
                            .with(imageView)
                            .load(url)
                            .error(R.drawable.missing) // エラー時のフォールバック画像を設定

                    // Apply grayscale effect for cloud-only games
                    if (game.isCloudOnly) {
                        // Use Glide's built-in transformations
                        glideRequest.apply(RequestOptions.bitmapTransform(BlurTransformation(8)))
                    }

                    glideRequest
                        .listener(
                            object : RequestListener<Drawable> {
                                override fun onLoadFailed(
                                    e: GlideException?,
                                    model: Any?,
                                    target: com.bumptech.glide.request.target.Target<Drawable>?,
                                    isFirstResource: Boolean,
                                ): Boolean {
                                    var mFirebaseAnalytics = FirebaseAnalytics.getInstance(ctx)
                                    val bundle = Bundle()
                                    bundle.putString(FirebaseAnalytics.Param.ITEM_ID, game.product_number)
                                    bundle.putString(FirebaseAnalytics.Param.ITEM_NAME, game.image_url)
                                    mFirebaseAnalytics.logEvent(
                                        "yab_fail_load_image",
                                        bundle,
                                    )
                                    // エラー画像はerror()で設定済みなので、ここでは何もしない
                                    return false // Glideにエラー画像の設定を任せる
                                }

                                override fun onResourceReady(
                                    resource: Drawable?,
                                    model: Any?,
                                    target: com.bumptech.glide.request.target.Target<Drawable>?,
                                    dataSource: com.bumptech.glide.load.DataSource?,
                                    isFirstResource: Boolean,
                                ): Boolean {
                                    // Glideが自動的に画像を設定するのでfalseを返す
                                    return false
                                }
                            },
                        ).into(imageView)
                } else {
                    Glide
                        .with(holder.rootview.context)
                        .load(game.image_url?.let { File(it) })
                        .into(imageView)
                }
            }

            // Set click listener (selection only, no game start)
            holder.rootview.setOnClickListener {
                // 選択状態を更新（ゲーム開始はしない）
                val adapterPosition = holder.bindingAdapterPosition
                if (adapterPosition != RecyclerView.NO_POSITION) {
                    setSelectedPosition(adapterPosition)
                }
            }
        }

        // Set menu button click listener
        holder.menuButton.setOnClickListener(
            View.OnClickListener {
                val adapterPosition = holder.bindingAdapterPosition
                if (adapterPosition != RecyclerView.NO_POSITION) {
                    showPopupMenu(
                        holder.menuButton,
                        adapterPosition,
                    )
                }
            },
        )
    }

    /**
     * Check if a game is already backed up to the cloud
     * @param gameInfo The GameInfo object to check
     * @param context The context to use for creating the GameBackupManager
     * @return true if the game is already backed up, false otherwise
     */
    private suspend fun isGameBackedUp(
        gameInfo: GameInfo,
        context: android.content.Context,
    ): Boolean {
        // Check if user is signed in
        val auth = FirebaseAuth.getInstance()
        if (auth.currentUser == null) {
            return false
        }

        try {
            // Get the file path or URI
            val filePath = gameInfo.file_path

            // Calculate hash
            val hash =
                if (filePath.startsWith("content://")) {
                    org.uoyabause.android.backup
                        .GameBackupManager(context)
                        .calculateSHA256(android.net.Uri.parse(filePath))
                } else {
                    org.uoyabause.android.backup
                        .GameBackupManager(context)
                        .calculateSHA256(java.io.File(filePath))
                }

            // Check if this hash exists in the user's backups
            val firestore = com.google.firebase.firestore.FirebaseFirestore
                .getInstance()
            val userDocRef = firestore.collection("users").document(auth.currentUser!!.uid)

            val existingBackup =
                userDocRef
                    .collection("backups")
                    .whereEqualTo("hash", hash)
                    .get()
                    .await()

            return !existingBackup.isEmpty
        } catch (e: Exception) {
            android.util.Log.e("GameItemAdapter", "Error checking if game is backed up: ${e.message}")
            return false
        }
    }

    /**
     * Checks if the current user is an admin
     * @param userId The user ID to check
     * @return true if user is admin, false otherwise
     */
    private suspend fun checkIsAdmin(userId: String): Boolean = try {
        val db = com.google.firebase.firestore.FirebaseFirestore
            .getInstance()
        val adminDoc =
            withContext(Dispatchers.IO) {
                db
                    .collection("admins")
                    .document(userId)
                    .get()
                    .await()
            }
        adminDoc.exists()
    } catch (e: Exception) {
        Log.e("GameItemAdapter", "Error checking admin status", e)
        false
    }

    fun showPopupMenu(
        view: View,
        position: Int,
        boxartImageView: ImageView? = null,
    ) { // inflate menu
        val popup = PopupMenu(view.context, view)
        val inflater: MenuInflater = popup.getMenuInflater()

        val game = dataSet?.get(position)

        // Use different menu for cloud-only games
        if (game?.isCloudOnly == true) {
            inflater.inflate(R.menu.cloud_game_item_popup_menu, popup.getMenu())
        } else {
            inflater.inflate(R.menu.game_item_popup_menu, popup.getMenu())
/*
            // Check if the game is already backed up to the cloud
            if (game != null) {
                // Launch coroutine to check if game is backed up
                CoroutineScope(Dispatchers.Main).launch {
                    try {
                        val isBackedUp = isGameBackedUp(game, view.context)
                        // Update menu item text based on backup status
                        if (isBackedUp) {
                            popup.menu.findItem(R.id.backup_to_cloud)?.title =
                                view.context.getString(R.string.remove_from_cloud)
                        }
                    } catch (e: Exception) {
                        android.util.Log.e("GameItemAdapter", "Error checking backup status: ${e.message}")
                    }
                }
            }

 */
        }

        popup.setOnMenuItemClickListener(
            PopupMenu.OnMenuItemClickListener {
                when (it.itemId) {
                    R.id.download_from_cloud -> {
                        val gameInfo = dataSet?.get(position)!!
                        if (gameInfo.isCloudOnly && gameInfo.cloudBackupInfo != null) {
                            // Handle cloud-only game download - just click the item to download
                            if (null != mListener) {
                                mListener!!.onItemClick(position, dataSet?.get(position), null)
                            }
                            return@OnMenuItemClickListener true
                        }
                        return@OnMenuItemClickListener false
                    }

/* Disable cloud backup
                R.id.backup_to_cloud -> {
                    val gameInfo = dataSet?.get(position)!!

                    // Check if user is signed in
                    val auth = FirebaseAuth.getInstance()
                    if (auth.currentUser == null) {
                        Toast.makeText(
                            view.context,
                            "Please sign in to use cloud backup",
                            Toast.LENGTH_SHORT
                        ).show()
                        return@OnMenuItemClickListener true
                    }

                    // Check if this is a backup or remove operation
                    CoroutineScope(Dispatchers.Main).launch {
                        try {
                            val isBackedUp = isGameBackedUp(gameInfo, view.context)

                            if (isBackedUp) {
                                // This is a remove operation
                                handleRemoveFromCloud(view, gameInfo)
                            } else {
                                // This is a backup operation
                                handleBackupToCloud(view, gameInfo)
                            }
                        } catch (e: Exception) {
                            android.util.Log.e("GameItemAdapter", "Error determining backup status: ${e.message}")
                            // Default to backup operation if we can't determine status
                            handleBackupToCloud(view, gameInfo)
                        }
                    }

                    true
                }

 */
                    R.id.detail -> {
                        val gameInfo = dataSet?.get(position)!!
                        // Firestoreからproduct_numberを使用してゲームのドキュメントIDを検索
                        val db = com.google.firebase.firestore.FirebaseFirestore
                            .getInstance()
                        db
                            .collection("games")
                            .whereEqualTo("product_number", gameInfo.product_number)
                            .get()
                            .addOnSuccessListener { documents ->
                                if (!documents.isEmpty) {
                                    // ゲームが見つかった場合、そのドキュメントIDを使用してウェブページを開く
                                    val gameDoc = documents.documents[0]
                                    val gameId = gameDoc.id
                                    val url = "https://www.yabasanshiro.com/games/$gameId"
                                    val intent = android.content.Intent(android.content.Intent.ACTION_VIEW)
                                    intent.data = android.net.Uri.parse(url)
                                    view.context.startActivity(intent)
                                } else {
                                    // ゲームが見つからない場合はエラーメッセージを表示
                                    android.widget.Toast
                                        .makeText(
                                            view.context,
                                            "Game not found in database",
                                            android.widget.Toast.LENGTH_SHORT,
                                        ).show()
                                }
                            }.addOnFailureListener { e ->
                                // エラーが発生した場合はエラーメッセージを表示
                                android.widget.Toast
                                    .makeText(
                                        view.context,
                                        "Error: ${e.message}",
                                        android.widget.Toast.LENGTH_SHORT,
                                    ).show()
                                android.util.Log.e("GameItemAdapter", "Error querying games collection", e)
                            }
                    }
                    R.id.restore_defaults -> {
                        var gameInfo = dataSet?.get(position)!!
                        val gamePreference = view.context.getHarmonySharedPreferences(gameInfo.product_number)
                        gamePreference.edit().clear().apply()
                    }
                    R.id.report -> {
                        val gameInfo = dataSet?.get(position)!!
                        val reportDialog = org.uoyabause.android.ReportDialog(view.context, gameInfo.product_number)
                        val fragmentManager = (view.context as androidx.fragment.app.FragmentActivity).supportFragmentManager
                        reportDialog.show(fragmentManager, "ReportDialog")
                    }
                    R.id.reproduce_report -> {
                        val gameInfo = dataSet?.get(position)!!

                        // Check if user is signed in
                        val auth = FirebaseAuth.getInstance()
                        if (auth.currentUser == null) {
                            Toast
                                .makeText(
                                    view.context,
                                    "Please sign in to access reports",
                                    Toast.LENGTH_SHORT,
                                ).show()
                            return@OnMenuItemClickListener true
                        }

                        // Check if user is admin
                        CoroutineScope(Dispatchers.Main).launch {
                            try {
                                val isAdmin = checkIsAdmin(auth.currentUser!!.uid)

                                if (isAdmin) {
                                    // Launch ReportListActivity
                                    val intent =
                                        Intent(view.context, org.uoyabause.android.ReportListActivity::class.java).apply {
                                            putExtra(
                                                org.uoyabause.android.ReportListActivity.EXTRA_PRODUCT_NUMBER,
                                                gameInfo.product_number,
                                            )
                                            putExtra(org.uoyabause.android.ReportListActivity.EXTRA_GAME_TITLE, gameInfo.game_title)
                                            putExtra(org.uoyabause.android.ReportListActivity.EXTRA_FILE_PATH, gameInfo.file_path)
                                            putExtra(org.uoyabause.android.ReportListActivity.EXTRA_ISO_FILE_PATH, gameInfo.iso_file_path)
                                        }
                                    view.context.startActivity(intent)
                                } else {
                                    Toast
                                        .makeText(
                                            view.context,
                                            "This feature is only available for administrators",
                                            Toast.LENGTH_LONG,
                                        ).show()
                                }
                            } catch (e: Exception) {
                                Log.e("GameItemAdapter", "Error checking admin status", e)
                                Toast
                                    .makeText(
                                        view.context,
                                        "Error: ${e.message}",
                                        Toast.LENGTH_SHORT,
                                    ).show()
                            }
                        }
                        true
                    }
                    R.id.create_shortcut -> {
                        val gameInfo = dataSet?.get(position)!!
                        createHomeScreenShortcut(view.context, gameInfo, boxartImageView)
                        true
                    }
                    R.id.delete -> {
/*
                    val prefs = view.context.getSharedPreferences("private", Context.MODE_PRIVATE)
                    val hasDonated = prefs.getBoolean("donated", false)

                    if (!BuildConfig.BUILD_TYPE.equals("pro") && !hasDonated) {
                        AlertDialog.Builder(view.context)
                                .setTitle(R.string.not_available)
                                .setMessage(R.string.only_pro_version)
                                .setPositiveButton(R.string.got_it) { _, _ ->
                                    val url = "https://play.google.com/store/apps/details?id=org.uoyabause.uranus.pro"
                                    val intent = Intent(Intent.ACTION_VIEW)
                                    intent.data = Uri.parse(url)
                                    intent.setPackage("com.android.vending")
                                    view.context.startActivity(intent)
                                }.setNegativeButton(R.string.cancel) { _, _ ->
                                }
                                .show()
                    } else {

 */
                        Log.d("textext", "R.id.delete is selected")
                        AlertDialog
                            .Builder(view.context)
                            .setTitle(R.string.delete_confirm_title)
                            .setMessage(R.string.delete_confirm)
                            .setPositiveButton(R.string.ok) { _, _ ->

                                var gameInfo = dataSet?.get(position)!!

                                // Store the game info and position for retry after permission is granted
                                val gameToDelete = gameInfo
                                val positionToDelete = position

                                // Function to perform the actual deletion
                                fun performDeletion() {
                                    GlobalScope.launch(Dispatchers.IO) {
                                        gameToDelete.removeInstance(null) // No callback needed for retry
                                        withContext(Dispatchers.Main) {
                                            dataSet!!.removeAt(positionToDelete)
                                            mListener?.onGameRemoved(gameToDelete)
                                            notifyItemRemoved(positionToDelete)
                                            // Auto-select next game after deletion
                                            selectNextGameAfterDeletion(positionToDelete)
                                        }
                                    }
                                }

                                GlobalScope.launch(Dispatchers.IO) {
                                    // Create permission callback for SAF permission requests
                                    val permissionCallback =
                                        object : org.uoyabause.android.PermissionRequestCallback {
                                            override fun onPermissionRequired(
                                                uri: android.net.Uri,
                                                message: String,
                                            ): Boolean {
                                                // Log the permission requirement
                                                android.util.Log.w("GameItemAdapter", "Permission required for URI: $uri")
                                                android.util.Log.w("GameItemAdapter", "Message: $message")

                                                // Request permission on main thread
                                                android.os.Handler(android.os.Looper.getMainLooper()).post {
                                                    // Get the fragment instance to request permission
                                                    val activity = view.context as? androidx.appcompat.app.AppCompatActivity
                                                    val fragment =
                                                        activity?.supportFragmentManager?.findFragmentByTag("GameSelectFragmentPhone")
                                                            ?: activity?.supportFragmentManager?.fragments?.find {
                                                                it is GameSelectFragmentPhone
                                                            }

                                                    if (fragment is GameSelectFragmentPhone) {
                                                        fragment.requestWritePermission(uri) { permissionGranted ->
                                                            if (permissionGranted) {
                                                                android.util.Log.d(
                                                                    "GameItemAdapter",
                                                                    "Permission granted, retrying deletion",
                                                                )
                                                                // Permission granted, retry deletion
                                                                performDeletion()
                                                            } else {
                                                                android.util.Log.w(
                                                                    "GameItemAdapter",
                                                                    "Permission denied, performing database-only deletion",
                                                                )
                                                                // Permission denied, show message and perform database-only deletion
                                                                android.widget.Toast
                                                                    .makeText(
                                                                        view.context,
                                                                        "File deletion failed due to permission. Removed from game list only.",
                                                                        android.widget.Toast.LENGTH_LONG,
                                                                    ).show()

                                                                // Remove from database only
                                                                GlobalScope.launch(Dispatchers.IO) {
                                                                    org.uoyabause.android.YabauseStorage.dao
                                                                        .delete(gameToDelete)
                                                                    withContext(Dispatchers.Main) {
                                                                        dataSet!!.removeAt(positionToDelete)
                                                                        mListener?.onGameRemoved(gameToDelete)
                                                                        notifyItemRemoved(positionToDelete)
                                                                        // Auto-select next game after deletion
                                                                        selectNextGameAfterDeletion(positionToDelete)
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    } else {
                                                        android.util.Log.e(
                                                            "GameItemAdapter",
                                                            "Could not find GameSelectFragmentPhone to request permission",
                                                        )
                                                        android.widget.Toast
                                                            .makeText(
                                                                view.context,
                                                                "Cannot delete file: $message",
                                                                android.widget.Toast.LENGTH_LONG,
                                                            ).show()
                                                    }
                                                }

                                                // Return false to indicate permission request is in progress
                                                return false
                                            }
                                        }

                                    // Delete file and update UI
                                    gameToDelete.removeInstance(permissionCallback)

                                    // After deletion, update UI on main thread
                                    withContext(Dispatchers.Main) {
                                        dataSet!!.removeAt(positionToDelete)
                                        mListener?.onGameRemoved(gameToDelete)
                                        notifyItemRemoved(positionToDelete)
                                        // Auto-select next game after deletion
                                        selectNextGameAfterDeletion(positionToDelete)
                                    }
                                }
                            }.setNegativeButton(R.string.no) { _, _ ->
                            }.show()
                    }
                    else -> {
                        Log.d("textext", "Unknown value (value = $it.itemId)")
                    }
                }
                true
            },
        )
        popup.show()
    }

    override fun getItemCount(): Int = dataSet?.size ?: 0

    fun removeItem(id: Int) {
        val index = dataSet?.indexOfFirst({ it!!.id == id })
        if (index != null && index != -1) {
            dataSet?.removeAt(index)
            originalDataSet?.removeAll { it?.id == id }
            notifyItemRemoved(index)
            // Auto-select next game after deletion
            selectNextGameAfterDeletion(index)
        }
    }

    private fun selectNextGameAfterDeletion(deletedPosition: Int) {
        if (dataSet.isNullOrEmpty()) {
            // No games left, clear selection
            selectedPosition = -1
            mListener?.onItemSelected(-1, null)
            return
        }

        // Select the next available game
        val newPosition =
            when {
                // If deleted game was at the end, select the previous game
                deletedPosition >= dataSet!!.size -> dataSet!!.size - 1
                // Otherwise, select the game that moved to the deleted position
                else -> deletedPosition
            }

        if (newPosition >= 0 && newPosition < dataSet!!.size) {
            selectedPosition = newPosition
            notifyItemChanged(newPosition)
            mListener?.onItemSelected(newPosition, dataSet!![newPosition])
        }
    }

    // 検索機能
    override fun getFilter(): Filter {
        return object : Filter() {
            override fun performFiltering(constraint: CharSequence?): FilterResults {
                val query = constraint?.toString()?.lowercase(Locale.getDefault()) ?: ""
                currentSearchQuery = query

                val filteredList =
                    if (query.isEmpty()) {
                        originalDataSet?.toMutableList()
                    } else {
                        originalDataSet
                            ?.filter {
                                it?.game_title?.lowercase(Locale.getDefault())?.contains(query) == true
                            }?.toMutableList()
                    }

                val results = FilterResults()
                results.values = filteredList
                return results
            }

            @Suppress("UNCHECKED_CAST")
            override fun publishResults(
                constraint: CharSequence?,
                results: FilterResults?,
            ) {
                dataSet = results?.values as? MutableList<GameInfo?>
                notifyDataSetChanged()
            }
        }
    }

    // ソート機能
    fun sortByName() {
        dataSet?.sortBy { it?.game_title?.lowercase(Locale.getDefault()) }
        notifyDataSetChanged()
        // ソート後に最初のアイテムを選択してBoxArtを更新
        if (!dataSet.isNullOrEmpty()) {
            setSelectedPosition(0)
        }
    }

    fun sortByDate() {
        dataSet?.sortBy { it?.release_date }
        notifyDataSetChanged()
        // ソート後に最初のアイテムを選択してBoxArtを更新
        if (!dataSet.isNullOrEmpty()) {
            setSelectedPosition(0)
        }
    }

    fun sortByRecentlyPlayed() {
        dataSet?.sortByDescending { it?.lastplay_date }
        notifyDataSetChanged()
        // ソート後に最初のアイテムを選択してBoxArtを更新
        if (!dataSet.isNullOrEmpty()) {
            setSelectedPosition(0)
        }
    }

    // 現在のフィルタリング状態を維持したままソート
    fun applyCurrentFilter() {
        if (currentSearchQuery.isNotEmpty()) {
            val filter = filter
            filter.filter(currentSearchQuery)
        }
    }

    /**
     * Creates a progress dialog using AlertDialog with custom layout
     */
    private fun createProgressDialog(
        context: Context,
        message: String,
    ): Pair<AlertDialog, TextView> {
        val dialogView =
            LinearLayout(context).apply {
                orientation = LinearLayout.HORIZONTAL
                setPadding(48, 32, 48, 32)

                addView(
                    ProgressBar(context).apply {
                        isIndeterminate = true
                    },
                )

                addView(
                    TextView(context).apply {
                        text = message
                        setPadding(32, 0, 0, 0)
                        layoutParams =
                            LinearLayout
                                .LayoutParams(
                                    LinearLayout.LayoutParams.WRAP_CONTENT,
                                    LinearLayout.LayoutParams.WRAP_CONTENT,
                                ).apply {
                                    gravity = android.view.Gravity.CENTER_VERTICAL
                                }
                    },
                )
            }

        val messageTextView = dialogView.getChildAt(1) as TextView

        val dialog =
            MaterialAlertDialogBuilder(context)
                .setView(dialogView)
                .setCancelable(false)
                .create()

        return Pair(dialog, messageTextView)
    }

    /**
     * Handle the "Backup to Cloud" operation
     * @param view The view that triggered the operation
     * @param gameInfo The GameInfo object to backup
     */
    private fun handleBackupToCloud(
        view: View,
        gameInfo: GameInfo,
    ) {
        // Show legal warning dialog
        val builder = AlertDialog.Builder(view.context)
        builder.setTitle(view.context.getString(R.string.legal_warning_title))
        builder.setMessage(view.context.getString(R.string.legal_warning_message))
        builder.setPositiveButton(view.context.getString(R.string.yes)) { dialog, which ->
            // Start backup process
            val gameBackupManager = org.uoyabause.android.backup
                .GameBackupManager(view.context)

            // Launch coroutine to perform backup
            CoroutineScope(Dispatchers.Main).launch {
                try {
                    // Perform backup
                    val result = gameBackupManager.backupGame(gameInfo)

                    // Check if backup limit was reached
                    if (!result.success && result.limitReached && result.backupList.isNotEmpty()) {
                        // Show the replacement dialog
                        org.uoyabause.android.backup.BackupReplaceDialog.show(
                            view.context,
                            result.backupList,
                        ) { backupToReplace ->
                            // User selected a backup to replace
                            CoroutineScope(Dispatchers.Main).launch {
                                try {
                                    // Show progress dialog
                                    val (replaceProgressDialog, _) = createProgressDialog(view.context, "Replacing backup...")
                                    replaceProgressDialog.show()

                                    // Perform replacement
                                    val replaceResult = gameBackupManager.replaceBackup(gameInfo, backupToReplace)

                                    // Dismiss progress dialog
                                    replaceProgressDialog.dismiss()

                                    // Show result
                                    if (replaceResult.success) {
                                        Toast
                                            .makeText(
                                                view.context,
                                                view.context.getString(R.string.replace_backup_success),
                                                Toast.LENGTH_SHORT,
                                            ).show()
                                    } else {
                                        Toast
                                            .makeText(
                                                view.context,
                                                "${view.context.getString(R.string.replace_backup_failed)}: ${replaceResult.message}",
                                                Toast.LENGTH_LONG,
                                            ).show()
                                    }
                                } catch (e: Exception) {
                                    Toast
                                        .makeText(
                                            view.context,
                                            "${view.context.getString(R.string.replace_backup_failed)}: ${e.message}",
                                            Toast.LENGTH_LONG,
                                        ).show()
                                }
                            }
                        }
                        return@launch
                    }

                    // Show progress dialog
                    val (progressDialog, _) = createProgressDialog(view.context, "Backing up game...")
                    progressDialog.show()

                    // If we got here, either the backup was successful or there was an error
                    // that wasn't related to the backup limit

                    // Dismiss progress dialog
                    progressDialog.dismiss()

                    // Show result
                    if (result.success) {
                        Toast
                            .makeText(
                                view.context,
                                view.context.getString(R.string.backup_success),
                                Toast.LENGTH_SHORT,
                            ).show()
                    } else {
                        Toast
                            .makeText(
                                view.context,
                                "${view.context.getString(R.string.backup_failed)}: ${result.message}",
                                Toast.LENGTH_LONG,
                            ).show()
                    }
                } catch (e: Exception) {
                    Toast
                        .makeText(
                            view.context,
                            "${view.context.getString(R.string.backup_failed)}: ${e.message}",
                            Toast.LENGTH_LONG,
                        ).show()
                }
            }
        }
        builder.setNegativeButton(view.context.getString(R.string.no)) { dialog, which ->
            dialog.dismiss()
        }
        builder.show()
    }

    /**
     * Handle the "Remove from Cloud" operation
     * @param view The view that triggered the operation
     * @param gameInfo The GameInfo object to remove from cloud
     */
    private fun handleRemoveFromCloud(
        view: View,
        gameInfo: GameInfo,
    ) {
        // Show confirmation dialog
        val builder = AlertDialog.Builder(view.context)
        builder.setTitle(view.context.getString(R.string.remove_from_cloud))
        builder.setMessage("Are you sure you want to remove this game from the cloud?")
        builder.setPositiveButton(view.context.getString(R.string.yes)) { dialog, which ->
            // Start remove process
            CoroutineScope(Dispatchers.Main).launch {
                try {
                    // Show progress dialog
                    val (progressDialog, _) = createProgressDialog(view.context, "Removing game from cloud...")
                    progressDialog.show()

                    // First get the list of backed up games
                    val gameBackupManager = org.uoyabause.android.backup
                        .GameBackupManager(view.context)
                    val backedUpGames = gameBackupManager.getBackedUpGames()

                    // Find the matching backup by product number
                    val backupToDelete = backedUpGames.find { it.productNumber == gameInfo.product_number }

                    if (backupToDelete != null) {
                        // Delete the backup
                        val result = gameBackupManager.deleteBackup(backupToDelete)

                        // Dismiss progress dialog
                        progressDialog.dismiss()

                        // Show result
                        if (result.success) {
                            Toast
                                .makeText(
                                    view.context,
                                    view.context.getString(R.string.remove_success),
                                    Toast.LENGTH_SHORT,
                                ).show()
                        } else {
                            Toast
                                .makeText(
                                    view.context,
                                    "${view.context.getString(R.string.remove_failed)}: ${result.message}",
                                    Toast.LENGTH_LONG,
                                ).show()
                        }
                    } else {
                        // Dismiss progress dialog
                        progressDialog.dismiss()

                        // Show error
                        Toast
                            .makeText(
                                view.context,
                                "${view.context.getString(R.string.remove_failed)}: Backup not found",
                                Toast.LENGTH_LONG,
                            ).show()
                    }
                } catch (e: Exception) {
                    Toast
                        .makeText(
                            view.context,
                            "${view.context.getString(R.string.remove_failed)}: ${e.message}",
                            Toast.LENGTH_LONG,
                        ).show()
                }
            }
        }
        builder.setNegativeButton(view.context.getString(R.string.no)) { dialog, which ->
            dialog.dismiss()
        }
        builder.show()
    }

    private fun loadBitmapFromUrl(
        context: Context,
        gameInfo: GameInfo,
    ): Bitmap? {
        val imageUrl =
            if (!gameInfo.image_url.isNullOrEmpty()) {
                if (gameInfo.image_url!!.startsWith("http")) {
                    // For cloud games or HTTP URLs, add signature if needed
                    if (gameInfo.isCloudOnly) {
                        gameInfo.image_url + "?" + sigin
                    } else {
                        gameInfo.image_url
                    }
                } else {
                    // Local file path
                    gameInfo.image_url
                }
            } else {
                null
            }

        return if (imageUrl != null) {
            val glideRequest =
                if (imageUrl.startsWith("http")) {
                    Glide.with(context).asBitmap().load(imageUrl)
                } else {
                    Glide.with(context).asBitmap().load(File(imageUrl))
                }
            glideRequest.submit(512, 512).get()
        } else {
            null
        }
    }

    private fun createHomeScreenShortcut(
        context: Context,
        gameInfo: GameInfo,
        boxartImageView: ImageView? = null,
    ) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val shortcutManager = context.getSystemService(ShortcutManager::class.java)

            if (shortcutManager != null && shortcutManager.isRequestPinShortcutSupported) {
                // Create intent to launch the game directly
                val intent =
                    Intent(context, org.uoyabause.android.Yabause::class.java).apply {
                        action = Intent.ACTION_VIEW
                        // Check if file path contains content:// URI
                        if (gameInfo.file_path.contains("content://")) {
                            putExtra("org.uoyabause.android.FileNameUri", gameInfo.file_path)
                            putExtra("org.uoyabause.android.FileDir", gameInfo.iso_file_path)
                        } else {
                            putExtra("org.uoyabause.android.FileNameEx", gameInfo.file_path)
                        }
                        putExtra("org.uoyabause.android.gamecode", gameInfo.product_number)
                        flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
                    }

                // Load boxart as icon - first try to get from ImageView, then fallback to loading
                val iconBitmap: Bitmap? =
                    try {
                        // First, try to get the bitmap from the boxart ImageView if it's provided
                        if (boxartImageView != null && boxartImageView.drawable != null) {
                            when (val drawable = boxartImageView.drawable) {
                                is BitmapDrawable -> {
                                    // If it's already a BitmapDrawable, get the bitmap
                                    val originalBitmap = drawable.bitmap
                                    // Scale up if needed for better icon quality
                                    if (originalBitmap.width < 512 || originalBitmap.height < 512) {
                                        Bitmap.createScaledBitmap(originalBitmap, 512, 512, true)
                                    } else {
                                        originalBitmap
                                    }
                                }
                                else -> {
                                    // For other drawable types, draw to a bitmap
                                    val drawable = boxartImageView.drawable
                                    val bitmap = Bitmap.createBitmap(512, 512, Bitmap.Config.ARGB_8888)
                                    val canvas = Canvas(bitmap)
                                    drawable.setBounds(0, 0, 512, 512)
                                    drawable.draw(canvas)
                                    bitmap
                                }
                            }
                        } else {
                            // Fallback to loading from URL if ImageView is not available
                            loadBitmapFromUrl(context, gameInfo)
                        }
                    } catch (e: Exception) {
                        Log.e("GameItemAdapter", "Failed to load boxart for shortcut", e)
                        null
                    }

                val shortcutInfo =
                    ShortcutInfo
                        .Builder(context, "game_${gameInfo.product_number}")
                        .setShortLabel(gameInfo.game_title ?: "Saturn Game")
                        .setLongLabel(gameInfo.game_title ?: "Saturn Game")
                        .setIntent(intent)
                        .apply {
                            if (iconBitmap != null) {
                                // Use adaptive icon for better display on Android 8.0+
                                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                                    setIcon(Icon.createWithAdaptiveBitmap(iconBitmap))
                                } else {
                                    setIcon(Icon.createWithBitmap(iconBitmap))
                                }
                            } else {
                                // Use default app icon if boxart is not available
                                setIcon(Icon.createWithResource(context, org.devmiyax.yabasanshiro.R.mipmap.ic_launcher))
                            }
                        }.build()

                // Request to pin the shortcut
                shortcutManager.requestPinShortcut(shortcutInfo, null)

                Toast
                    .makeText(
                        context,
                        context.getString(org.devmiyax.yabasanshiro.R.string.shortcut_created),
                        Toast.LENGTH_SHORT,
                    ).show()
            } else {
                Toast
                    .makeText(
                        context,
                        context.getString(org.devmiyax.yabasanshiro.R.string.shortcut_not_supported),
                        Toast.LENGTH_SHORT,
                    ).show()
            }
        } else {
            // API 24-25: Shortcuts not supported via ShortcutManager
            Toast
                .makeText(
                    context,
                    context.getString(org.devmiyax.yabasanshiro.R.string.shortcut_not_supported),
                    Toast.LENGTH_SHORT,
                ).show()
        }
    }

    companion object {
        private const val CARD_WIDTH = 320
        private const val CARD_HEIGHT = 224

        // 表示モード定数
        const val VIEW_TYPE_DETAILED = 0
        const val VIEW_TYPE_COMPACT = 1
        const val VIEW_TYPE_TEXT_ONLY = 2
        const val VIEW_TYPE_SLIM = 3
    }
}
