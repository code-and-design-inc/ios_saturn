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


    ============================================================================

 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing permissions and limitations under
 * the License.
 */

package org.uoyabause.android.tv

import android.Manifest
import android.app.Activity
import android.app.UiModeManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.content.res.Configuration
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Color
import android.graphics.drawable.Drawable
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.DisplayMetrics
import android.util.Log
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.core.view.setPadding
import androidx.leanback.app.BackgroundManager
import androidx.leanback.app.BrowseSupportFragment
import androidx.leanback.widget.ArrayObjectAdapter
import androidx.leanback.widget.HeaderItem
import androidx.leanback.widget.ListRow
import androidx.leanback.widget.ListRowPresenter
import androidx.leanback.widget.OnItemViewClickedListener
import androidx.leanback.widget.OnItemViewSelectedListener
import androidx.leanback.widget.Presenter
import androidx.leanback.widget.Row
import androidx.leanback.widget.RowPresenter
import androidx.lifecycle.lifecycleScope
import androidx.multidex.MultiDexApplication
import androidx.preference.PreferenceManager
import com.google.android.gms.ads.AdRequest
import com.google.android.gms.ads.FullScreenContentCallback
import com.google.android.gms.ads.LoadAdError
import com.google.android.gms.ads.MobileAds
import com.google.android.gms.ads.interstitial.InterstitialAd
import com.google.android.gms.ads.interstitial.InterstitialAdLoadCallback
import com.google.android.gms.analytics.HitBuilders.ScreenViewBuilder
import com.google.android.gms.analytics.Tracker
import com.google.firebase.analytics.FirebaseAnalytics
import com.google.firebase.auth.FirebaseAuth
import io.noties.markwon.Markwon
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import org.devmiyax.yabasanshiro.BuildConfig
import org.devmiyax.yabasanshiro.R
import org.devmiyax.yabasanshiro.StartupActivity
import org.uoyabause.android.AdActivity
import org.uoyabause.android.FileDialog
import org.uoyabause.android.FileDialog.FileSelectedListener
import org.uoyabause.android.GameInfo
import org.uoyabause.android.GameSelectPresenter
import org.uoyabause.android.GameSelectPresenter.GameSelectPresenterListener
import org.uoyabause.android.SettingsActivity
import org.uoyabause.android.ShowPinInFragment.Companion.newInstance
import org.uoyabause.android.YabauseApplication
import org.uoyabause.android.YabauseStorage
import org.uoyabause.android.YabauseStorage.Companion.storage
import org.uoyabause.android.storage.PreferencesManager
import java.io.File
import java.net.InetAddress
import java.net.NetworkInterface
import java.net.URI
import java.net.URLDecoder
import java.util.Collections
import java.util.Timer

class GameSelectFragment :
    BrowseSupportFragment(),
    FileSelectedListener,
    GameSelectPresenterListener {
    private val handler = Handler(Looper.getMainLooper())
    private var rowsAdapter: ArrayObjectAdapter? = null
    private var defaultBackground: Drawable? = null
    private var metrics: DisplayMetrics? = null
    private val backgroundTimer: Timer? = null
    private val backgroundUri: URI? = null
    private var backgroundManager: BackgroundManager? = null
    private var tracker: Tracker? = null
    private var interstitialAd: InterstitialAd? = null
    private var firebaseAnalytics: FirebaseAnalytics? = null
    private var initialDialog: AlertDialog? = null
    var isFirstUpdate = true
    var fragmentView: View? = null
    lateinit var presenter: GameSelectPresenter
    var alphabet =
        arrayOf(
            "A",
            "B",
            "C",
            "D",
            "E",
            "F",
            "G",
            "H",
            "I",
            "J",
            "K",
            "L",
            "M",
            "N",
            "O",
            "P",
            "Q",
            "R",
            "S",
            "T",
            "U",
            "V",
            "W",
            "X",
            "Y",
            "Z",
        )
    private var progressDialog: AlertDialog? = null
    private var progressDialogMessage: TextView? = null

    private val storagePermissionLauncher =
        registerForActivityResult(
            ActivityResultContracts.RequestMultiplePermissions(),
        ) { permissions ->
            val allGranted = permissions.entries.all { it.value }
            if (allGranted) {
                Log.i(TAG, "Storage permissions granted.")
                updateGameList()
            } else {
                Log.i(TAG, "Storage permissions denied.")
            }
        }

    private val settingsActivityLauncher =
        registerForActivityResult(
            ActivityResultContracts.StartActivityForResult(),
        ) { result ->
            if (initialDialog != null) {
                initialDialog?.dismiss()
                initialDialog = null
            }
            when (result.resultCode) {
                GAMELIST_NEED_TO_UPDATED -> {
                    refreshLevel = 3
                    if (checkStoragePermission() == 0) {
                        updateGameList()
                    }
                    updateBackGraound()
                }
                GAMELIST_NEED_TO_RESTART -> {
                    val intent = Intent(activity, StartupActivity::class.java)
                    intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_NEW_TASK)
                    startActivity(intent)
                    requireActivity().finish()
                }
                else -> {
                    updateBackGraound()
                }
            }
        }

    /**
     * Called when the 'show camera' button is clicked.
     * Callback is defined in resource layout definition.
     */
    fun checkStoragePermission(): Int {
        if (Build.VERSION.SDK_INT >= 23 && Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            // Verify that all required contact permissions have been granted.
            if (ActivityCompat.checkSelfPermission(
                    requireActivity(),
                    Manifest.permission.READ_EXTERNAL_STORAGE,
                )
                != PackageManager.PERMISSION_GRANTED ||
                ActivityCompat.checkSelfPermission(
                    requireActivity(),
                    Manifest.permission.WRITE_EXTERNAL_STORAGE,
                )
                != PackageManager.PERMISSION_GRANTED
            ) {
                // Storage permissions have not been granted.
                Log.i(TAG, "Storage permissions has NOT been granted. Requesting permissions.")
                storagePermissionLauncher.launch(PERMISSIONS_STORAGE)
                return -1
            }
        }
        return 0
    }

    fun showDialog(message: String) {
        if (initialDialog != null) {
            initialDialog?.dismiss()
            initialDialog = null
        }
        if (progressDialog == null) {
            val context = activity ?: return
            val layout =
                LinearLayout(context).apply {
                    orientation = LinearLayout.HORIZONTAL
                    setPadding(48, 32, 48, 32)
                    gravity = android.view.Gravity.CENTER_VERTICAL
                }
            val progressBar =
                ProgressBar(context).apply {
                    isIndeterminate = true
                }
            progressDialogMessage =
                TextView(context).apply {
                    text = message
                    setPadding(32, 0, 0, 0)
                }
            layout.addView(progressBar)
            layout.addView(progressDialogMessage)
            progressDialog =
                AlertDialog
                    .Builder(context)
                    .setView(layout)
                    .setCancelable(false)
                    .create()
            progressDialog!!.show()
        }
    }

    fun updateDialogString(msg: String) {
        if (initialDialog != null) {
            initialDialog?.dismiss()
            initialDialog = null
        }
        if (progressDialog == null) {
            showDialog(msg)
        } else {
            progressDialogMessage?.text = msg
        }
    }

    fun dismissDialog() {
        if (progressDialog != null) {
            if (progressDialog!!.isShowing) {
                progressDialog!!.dismiss()
            }
            progressDialog = null
            progressDialogMessage = null
        }
    }

    override fun fileSelected(file: File?) {
        if (file != null) {
            presenter!!.fileSelected(file)
        }
    }

    // @Override
    // public void onUpdateGameList() {
    //    loadRows();
    //    dismissDialog();
    //    if(isFirstUpdate) {
    //        isFirstUpdate = false;
    //        presenter.checkSignIn();
    //    }
    // }
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        presenter = GameSelectPresenter(this, yabauseActivityLauncher, this)
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R) {
            if (requireContext().display?.isMinimalPostProcessingSupported()!!) {
                requireActivity().window.setPreferMinimalPostProcessing(true)
            } else {
            }
        }
    }

    var yabauseActivityLauncher =
        registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
            if (!YabauseApplication.isPro()) {
                val rn = Math.random()
                if (rn <= 0.5) {
                    val uiModeManager =
                        requireActivity().getSystemService(Context.UI_MODE_SERVICE) as UiModeManager
                    if (uiModeManager.currentModeType != Configuration.UI_MODE_TYPE_TELEVISION) {
                        if (interstitialAd != null) {
                            interstitialAd!!.show(requireActivity())
                        } else {
                            val intent = Intent(activity, AdActivity::class.java)
                            startActivity(intent)
                        }
                    } else {
                        val intent = Intent(activity, AdActivity::class.java)
                        startActivity(intent)
                    }
                } else if (rn > 0.5) {
                    val intent = Intent(activity, AdActivity::class.java)
                    startActivity(intent)
                }
            }
        }

    @Deprecated("Deprecated in Java")
    override fun onActivityCreated(savedInstanceState: Bundle?) {
        Log.i(TAG, "onCreate")
        @Suppress("DEPRECATION")
        super.onActivityCreated(savedInstanceState)
        firebaseAnalytics = FirebaseAnalytics.getInstance(requireActivity())
        val application = requireActivity().application as YabauseApplication
        tracker = application.defaultTracker
        MobileAds.initialize(requireContext())

        showMigrationDialogIfNeeded()

        requestNewInterstitial()

        val intent = requireActivity().intent
        val uri = intent.data
        if (uri != null && !uri.pathSegments.isEmpty()) {
            val pathSegments = uri.pathSegments
            var filename = pathSegments[1]
            Log.d(TAG, "filename: $filename")
            try {
                filename = URLDecoder.decode(filename, "UTF-8")
                GlobalScope.launch(Dispatchers.IO) {
                    val game = YabauseStorage.dao.findByFilePath(filename)
                    if (game != null) {
                        launch(Dispatchers.Main) {
                            presenter.startGame(game, yabauseActivityLauncher)
                        }
                    }
                }
            } catch (e: Exception) {
                Log.d(TAG, e.localizedMessage!!)
            }
        }
        prepareBackgroundManager()
        setupUIElements()
        setupEventListeners()
        if (rowsAdapter == null) {
            rowsAdapter = ArrayObjectAdapter(ListRowPresenter())
            val gridHeader = HeaderItem(0, "PREFERENCES")
            val mGridPresenter = GridItemPresenter()
            val gridRowAdapter = ArrayObjectAdapter(mGridPresenter)
            gridRowAdapter.add(resources.getString(R.string.setting))
            val uiModeManager =
                requireActivity().getSystemService(Context.UI_MODE_SERVICE) as UiModeManager
            if (uiModeManager.currentModeType != Configuration.UI_MODE_TYPE_TELEVISION) {
                //    gridRowAdapter.add(getResources().getString(R.string.invite));
            }
            // val prefs = activity!!.getSharedPreferences("private", Context.MODE_PRIVATE)
            // Boolean hasDonated = prefs.getBoolean("donated", false);
            // if( !hasDonated) {
            //    gridRowAdapter.add(getResources().getString(R.string.donation));
            // }
            gridRowAdapter.add("+")
            gridRowAdapter.add(resources.getString(R.string.refresh_db))
            // gridRowAdapter.add("GoogleDrive");
            val auth = FirebaseAuth.getInstance()
            if (auth.currentUser != null) {
                gridRowAdapter.add(resources.getString(R.string.sign_out))
            } else {
                gridRowAdapter.add(resources.getString(R.string.sign_in))
            }
            gridRowAdapter.add(resources.getString(R.string.sign_in_to_other_devices))
            rowsAdapter!!.add(ListRow(gridHeader, gridRowAdapter))
            setSelectedPosition(0, false)
            adapter = rowsAdapter
        }
        if (checkStoragePermission() == 0) {
            updateBackGraound()
            updateGameList()
        }
        /*
        View rootView = getTitleView();
        TextView tv = (TextView) rootView.findViewById(R.id.title_text);
        if( tv != null ) {
            tv.setTextSize(14);
        }
*/
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View? {
        fragmentView = super.onCreateView(inflater, container, savedInstanceState)
        val uiModeManager = requireActivity().getSystemService(Context.UI_MODE_SERVICE) as UiModeManager
        if (uiModeManager.currentModeType != Configuration.UI_MODE_TYPE_TELEVISION) {
            val rootView = titleView
            val tv = rootView?.findViewById<View>(R.id.title_text) as TextView?
            if (tv != null) {
                tv.textSize = 24f
            }
        }
        return fragmentView
    }

    private var signInActivityLauncher =
        registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
            presenter.onSignIn(result.resultCode, result.data)
            if (presenter.currentUserName != null) {
                // val m = navigationView!!.menu
                // val miLogin = m.findItem(R.id.menu_item_login)
                // miLogin.setTitle(R.string.sign_out)
            }
        }

    private var updateJob: Job? = null

    fun updateGameList() {
        if (updateJob?.isActive == true) return
        if (!presenter.prepareStorage()) return

        val level = refreshLevel
        refreshLevel = 0
        updateJob =
            lifecycleScope.launch {
                showDialog("Updating")
                try {
                    presenter.updateGameDatabase(level) { message ->
                        updateDialogString("Updating .. $message")
                    }

                    loadRows()
                    dismissDialog()

                    if (isFirstUpdate) {
                        isFirstUpdate = false
                        val ac: Activity? = this@GameSelectFragment.activity
                        if (ac != null && ac.intent.getBooleanExtra("showPin", false)) {
                            if (!YabauseApplication.isPro()) {
                                YabauseApplication.checkDonated(ac)
                            } else {
                                newInstance().show(childFragmentManager, "sample")
                            }
                        } else {
                            presenter.checkSignIn(signInActivityLauncher)
                        }
                    }
                } catch (e: Exception) {
                    dismissDialog()
                }
            }
    }

    override fun onResume() {
        super.onResume()
        isForeground = this
        if (tracker != null) {
            tracker!!.setScreenName(TAG)
            tracker!!.send(ScreenViewBuilder().build())
        }

        updateSignInOutString()
    }

    override fun onPause() {
        isForeground = null
        dismissDialog()
        super.onPause()
    }

    override fun onDestroy() {
        // this.setSelectedPosition(-1, false)
        System.gc()
        super.onDestroy()
        /*
        if (null != backgroundTimer) {
            Log.d(TAG, "onDestroy: " + backgroundTimer.toString());
            backgroundTimer.cancel();
        }
*/
    }

    override fun onSignOut() {
        updateSignInOutString()
    }

    private fun updateSignInOutString() {
        val auth = FirebaseAuth.getInstance()
        for (i in 0 until rowsAdapter!!.size()) {
            var ls = rowsAdapter!![i] as ListRow
            if (ls.getHeaderItem().getName() == "PREFERENCES") {
                var adapter = ls!!.getAdapter() as ArrayObjectAdapter
                for (j in 0 until adapter.size()) {
                    var item = adapter!!.get(j) as String

                    if (auth.currentUser != null) {
                        if (item == resources.getString(R.string.sign_in)) {
                            adapter!!.replace(j, resources.getString(R.string.sign_out))
                            adapter!!.notifyItemRangeChanged(j, 1)
                            rowsAdapter!!.notifyItemRangeChanged(i, 1)
                            return
                        }
                    } else {
                        if (item == resources.getString(R.string.sign_out)) {
                            adapter!!.replace(j, resources.getString(R.string.sign_in))
                            adapter!!.notifyItemRangeChanged(j, 1)
                            rowsAdapter!!.notifyItemRangeChanged(i, 1)
                            return
                        }
                    }
                }
            }
        }
    }

    private fun loadRows() {
        GlobalScope.launch(Dispatchers.IO) {
            var datacount = 0
            try {
                datacount = YabauseStorage.dao.getRowCount()
            } catch (e: Exception) {
                Log.d(TAG, e.localizedMessage!!)
            }

            if (datacount == 0) {
                launch(Dispatchers.Main) {
                    var viewMessage = TextView(requireContext())

                    val markwon =
                        Markwon.create(
                            requireContext(),
                        )

                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                        val welcomeMessage =
                            resources.getString(
                                R.string.welcome_11,
                                YabauseStorage.storage.gamePath,
                                YabauseStorage.storage.externalGamePath ?: "",
                            )

                        markwon.setMarkdown(viewMessage, welcomeMessage)
                    } else {
                        val welcomeMessage =
                            resources.getString(R.string.welcome, YabauseStorage.storage.gamePath)
                        markwon.setMarkdown(viewMessage, welcomeMessage)
                    }

                    viewMessage.setPadding(64)

                    initialDialog =
                        AlertDialog
                            .Builder(requireActivity(), R.style.Theme_AppCompat)
                            .setView(viewMessage)
                            .setPositiveButton(R.string.ok) { _, _ ->
                            }.create()

                    initialDialog?.show()
                }

                return@launch
            }

            if (!isAdded) return@launch

            launch(Dispatchers.Main) {
                var addindex = 0
                rowsAdapter = ArrayObjectAdapter(ListRowPresenter())

                // -----------------------------------------------------------------
                // Recent Play Game
                GlobalScope.launch(Dispatchers.IO) {
                    var rlist: List<GameInfo> = emptyList()
                    try {
                        rlist = YabauseStorage.dao.getRecentGames()
                    } catch (e: Exception) {
                        println(e)
                    }
                    launch(Dispatchers.Main) {
                        val recentHeader = HeaderItem(addindex.toLong(), "RECENT")
                        val itx = rlist!!.iterator()
                        val cardPresenterRecent = CardPresenter()
                        val listRowAdapterRecent = ArrayObjectAdapter(cardPresenterRecent)
                        var hit = false
                        while (itx.hasNext()) {
                            val game = itx.next()
                            listRowAdapterRecent.add(game)
                            hit = true
                        }

                        // ----------------------------------------------------------------------
                        // Refernce
                        if (hit) {
                            rowsAdapter!!.add(ListRow(recentHeader, listRowAdapterRecent))
                            addindex++
                        }
                        val gridHeader = HeaderItem(addindex.toLong(), "PREFERENCES")
                        val mGridPresenter = GridItemPresenter()
                        val gridRowAdapter = ArrayObjectAdapter(mGridPresenter)
                        // gridRowAdapter.add("Backup");
                        gridRowAdapter.add(resources.getString(R.string.setting))
                        val uiModeManager =
                            requireActivity().getSystemService(Context.UI_MODE_SERVICE) as UiModeManager
                        if (uiModeManager.currentModeType != Configuration.UI_MODE_TYPE_TELEVISION) {
                            //    gridRowAdapter.add(getResources().getString(R.string.invite));
                        }
                        // val prefs = activity!!.getSharedPreferences("private", Context.MODE_PRIVATE)
                        // Boolean hasDonated = prefs.getBoolean("donated", false);
                        // if( !hasDonated) {
                        //    gridRowAdapter.add(getResources().getString(R.string.donation));
                        // }
                        gridRowAdapter.add("+")
                        gridRowAdapter.add(resources.getString(R.string.refresh_db))
                        // gridRowAdapter.add("GoogleDrive");
                        val auth = FirebaseAuth.getInstance()
                        if (auth.currentUser != null) {
                            gridRowAdapter.add(resources.getString(R.string.sign_out))
                        } else {
                            gridRowAdapter.add(resources.getString(R.string.sign_in))
                        }
                        gridRowAdapter.add(resources.getString(R.string.sign_in_to_other_devices))
                        rowsAdapter!!.add(ListRow(gridHeader, gridRowAdapter))
                        addindex++

                        // -----------------------------------------------------------------
                        //
                        GlobalScope.launch(Dispatchers.IO) {
                            var list: MutableList<GameInfo>? = null
                            try {
                                list = YabauseStorage.dao.getAllSortedByTitle().toMutableList()
                            } catch (e: Exception) {
                                println(e)
                            }

                            launch(Dispatchers.Main) {
                                var i: Int
                                i = 0
                                while (i < alphabet.size) {
                                    hit = false
                                    val cardPresenter = CardPresenter()
                                    val listRowAdapter = ArrayObjectAdapter(cardPresenter)
                                    val it = list!!.iterator()
                                    while (it.hasNext()) {
                                        val game = it.next()
                                        if (game.game_title.uppercase().indexOf(alphabet[i]) == 0) {
                                            listRowAdapter.add(game)
                                            Log.d("GameSelect", alphabet[i] + ":" + game.game_title)
                                            it.remove()
                                            hit = true
                                        }
                                    }
                                    if (hit) {
                                        val header = HeaderItem(addindex.toLong(), alphabet[i])
                                        rowsAdapter!!.add(ListRow(header, listRowAdapter))
                                        addindex++
                                    }
                                    i++
                                }
                                val cardPresenter = CardPresenter()
                                val listRowAdapter = ArrayObjectAdapter(cardPresenter)
                                val it: Iterator<GameInfo> = list!!.iterator()
                                while (it.hasNext()) {
                                    val game = it.next()
                                    Log.d("GameSelect", "Others:" + game.game_title)
                                    listRowAdapter.add(game)
                                }
                                val header = HeaderItem(addindex.toLong(), "Others")
                                rowsAdapter!!.add(ListRow(header, listRowAdapter))
                                adapter = rowsAdapter
                            }
                        }
                    }
                }
            }
        }
    }

    private fun prepareBackgroundManager() {
        backgroundManager = BackgroundManager.getInstance(activity)
        backgroundManager!!.setAutoReleaseOnStop(false)
        backgroundManager!!.attach(requireActivity().window)
        defaultBackground =
            null // getBrandColor(); //getResources().getDrawable(R.drawable.saturn);
        metrics = DisplayMetrics()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            val windowMetrics = requireActivity().windowManager.currentWindowMetrics
            val bounds = windowMetrics.bounds
            metrics!!.widthPixels = bounds.width()
            metrics!!.heightPixels = bounds.height()
        } else {
            @Suppress("DEPRECATION")
            requireActivity().windowManager.defaultDisplay.getMetrics(metrics)
        }
    }

    private fun updateBackGraound() {
        // val sp = PreferenceManager.getDefaultSharedPreferences(activity)
        val imagePath = "err" // sp.getString("select_image", "err");
        if (imagePath == "err") {
            defaultBackground = null // getResources().getDrawable(R.drawable.saturn);
            backgroundManager!!.drawable = defaultBackground
        } else {
            try {
                val options = BitmapFactory.Options()
                options.inPreferredConfig = Bitmap.Config.ARGB_8888
                val bitmap = BitmapFactory.decodeFile(imagePath, options)

/*
                getActivity().grantUriPermission("org.uoyabause.android",
                        Uri.parse(imagePath),
                        Intent.FLAG_GRANT_READ_URI_PERMISSION);

                InputStream inputStream = getActivity().getContentResolver().openInputStream(Uri.parse(imagePath));

                BitmapFactory.Options imageOptions = new BitmapFactory.Options();
                imageOptions.inJustDecodeBounds = true;
                BitmapFactory.decodeStream(inputStream, null, imageOptions);
                Log.v("image", "Original Image Size: " + imageOptions.outWidth + " x " + imageOptions.outHeight);

                inputStream.close();

                Bitmap bitmap;
                int imageSizeMax = 1920;
                inputStream = getActivity().getContentResolver().openInputStream(Uri.parse(imagePath));
                float imageScaleWidth = (float)imageOptions.outWidth / imageSizeMax;
                float imageScaleHeight = (float)imageOptions.outHeight / imageSizeMax;

                if (imageScaleWidth > 2 && imageScaleHeight > 2) {
                    BitmapFactory.Options imageOptions2 = new BitmapFactory.Options();
                    int imageScale = (int)Math.floor((imageScaleWidth > imageScaleHeight ? imageScaleHeight : imageScaleWidth));
                    for (int i = 2; i <= imageScale; i *= 2) {
                        imageOptions2.inSampleSize = i;
                    }
                    bitmap = BitmapFactory.decodeStream(inputStream, null, imageOptions2);
                    Log.v("image", "Sample Size: 1/" + imageOptions2.inSampleSize);
                } else {
                    bitmap = BitmapFactory.decodeStream(inputStream);
                }

                inputStream.close();
                //defaultBackground = Drawable.createFromStream(inputStream, imagePath );
 */
                backgroundManager!!.setBitmap(bitmap)
            } catch (e: Exception) {
                defaultBackground = null // getResources().getDrawable(R.drawable.saturn);
                backgroundManager!!.drawable = defaultBackground
            }
        }
    }

    private fun setupUIElements() {
        // setBadgeDrawable(getActivity().getResources().getDrawable( R.drawable.banner));
        title =
            getString(R.string.app_name) + getVersionName(activity) // Badge, when set, takes precedent
        // over title
        headersState = HEADERS_HIDDEN
        isHeadersTransitionOnBackEnabled = true

        // set fastLane (or headers) background color
        brandColor = ContextCompat.getColor(requireContext(), R.color.fastlane_background)
        // set search icon color
        searchAffordanceColor = ContextCompat.getColor(requireContext(), R.color.search_opaque)
    }

    private fun setupEventListeners() {
/*
        setOnSearchClickedListener(new View.OnClickListener() {

            @Override
            public void onClick(View view) {
                Toast.makeText(getActivity(), "Implement your own in-app search", Toast.LENGTH_LONG)
                        .show();
            }
        });
*/
        setOnSearchClickedListener(null)
        onItemViewClickedListener = ItemViewClickedListener()
        onItemViewSelectedListener = ItemViewSelectedListener()
    }

    /*
        protected void updateBackground(String uri) {
            int width = metrics.widthPixels;
            int height = metrics.heightPixels;
            Glide.with(getActivity())
                    .load(uri)
                    .centerCrop()
                    .error(defaultBackground)
                    .into(new SimpleTarget<GlideDrawable>(width, height) {
                        @Override
                        public void onResourceReady(GlideDrawable resource,
                                                    GlideAnimation<? super GlideDrawable>
                                                            glideAnimation) {
                            backgroundManager.setDrawable(resource);
                        }
                    });
            backgroundTimer.cancel();
        }


    ============================================================================

    private void startBackgroundTimer() {
        if (null != backgroundTimer) {
            backgroundTimer.cancel();
        }
        backgroundTimer = new Timer();
        backgroundTimer.schedule(new UpdateBackgroundTask(), BACKGROUND_UPDATE_DELAY);
    }
*/
    val settingActivity = 0x01
    val downloadActivity = 0x03

    var signinActivityLauncher =
        registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
            presenter!!.onSignIn(result.resultCode, result.data)
            updateSignInOutString()
        }

    private inner class ItemViewClickedListener : OnItemViewClickedListener {
        override fun onItemClicked(
            itemViewHolder: Presenter.ViewHolder,
            item: Any,
            rowViewHolder: RowPresenter.ViewHolder,
            row: Row,
        ) {
            if (item is GameInfo) {
                presenter.startGame(item, yabauseActivityLauncher)
            } else if (item is String) {
                if (item == getString(R.string.sign_in)) {
                    presenter.signIn(signinActivityLauncher)
                } else if (item == getString(R.string.sign_out)) {
                    presenter.signOut()
                } else if (item == getString(R.string.sign_in_to_other_devices)) {
                    newInstance().show(childFragmentManager, "sample")
                } else if (item == getString(R.string.setting)) {
                    val intent = Intent(activity, SettingsActivity::class.java)
                    settingsActivityLauncher.launch(intent)
                } else if (item == "+") {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                        val prefs =
                            requireActivity().getSharedPreferences(
                                "private",
                                MultiDexApplication.MODE_PRIVATE,
                            )
                        val installCount = prefs.getInt("installCount", 3)
                        if (installCount > 0) {
                            val intent = Intent(Intent.ACTION_OPEN_DOCUMENT)
                            intent.addCategory(Intent.CATEGORY_OPENABLE)
                            intent.type = "*/*"
                            readRequestLauncher.launch(intent)
                        } else {
                            val message = resources.getString(R.string.or_place_file_to, YabauseStorage.storage.gamePath)
                            val rtn = YabauseApplication.checkDonated(requireActivity(), message)
                            if (rtn == 0) {
                                val intent = Intent(Intent.ACTION_OPEN_DOCUMENT)
                                intent.addCategory(Intent.CATEGORY_OPENABLE)
                                intent.type = "*/*"
                                readRequestLauncher.launch(intent)
                            }
                        }
                    } else {
                        val yabroot = File(storage.rootPath)
                        val sharedPref =
                            PreferenceManager.getDefaultSharedPreferences(
                                requireActivity(),
                            )
                        val lastDir = sharedPref.getString("pref_lastDir", yabroot.path)
                        val fd = FileDialog(requireActivity(), lastDir)
                        fd.addFileListener(this@GameSelectFragment)
                        fd.showDialog()
                    }
                } else if (item.indexOf(getString(R.string.refresh_db)) >= 0) {
                    refreshLevel = 3
                    if (checkStoragePermission() == 0) {
                        updateGameList()
                    }
                    // }else if(  ((String) item).indexOf(getString(R.string.donation)) >= 0){
                    //    Intent intent = new Intent(getActivity(), DonateActivity.class);
                    //    startActivity(intent);
                } else if (item.indexOf("GoogleDrive") >= 0) {
                    onGoogleDriveClciked()
                }
            }
        }
    }

    private fun onGoogleDriveClciked() {
        val pm = requireActivity().packageManager
        try {
            pm.getPackageInfo("org.uoyabause.gdrive", PackageManager.GET_ACTIVITIES)
            val intent = Intent("org.uoyabause.gdrive.LAUNCH")
            startActivity(intent)
        } catch (e: PackageManager.NameNotFoundException) {
            val intent =
                Intent(Intent.ACTION_VIEW, Uri.parse("market://details?id=org.uoyabause.android"))
            try {
                requireActivity().startActivity(intent)
            } catch (ex: Exception) {
                ex.printStackTrace()
            }
        }
    }

    private inner class ItemViewSelectedListener : OnItemViewSelectedListener {
        override fun onItemSelected(
            itemViewHolder: Presenter.ViewHolder?,
            item: Any?,
            rowViewHolder: RowPresenter.ViewHolder?,
            row: Row?,
        ) {
        }
    }

    private inner class GridItemPresenter : Presenter() {
        override fun onCreateViewHolder(parent: ViewGroup): ViewHolder {
            val view = TextView(parent.context)
            view.layoutParams =
                ViewGroup.LayoutParams(
                    GRID_ITEM_WIDTH,
                    GRID_ITEM_HEIGHT,
                )
            view.isFocusable = true
            view.isFocusableInTouchMode = true
            view.setBackgroundColor(ContextCompat.getColor(requireContext(), R.color.default_background))
            view.setTextColor(Color.WHITE)
            view.gravity = Gravity.CENTER
            return ViewHolder(view)
        }

        override fun onBindViewHolder(
            viewHolder: ViewHolder,
            item: Any?,
        ) {
            (viewHolder.view as TextView).text = item as String
        }

        override fun onUnbindViewHolder(viewHolder: ViewHolder) {}
    }

    override fun onShowMessage(string_id: Int) {
        showSnackbar(string_id)
    }

    override fun onShowDialog(message: String) {
        showDialog(message)
    }

    override fun onUpdateDialogMessage(message: String) {
        updateDialogString(message)
    }

    override fun onDismissDialog() {
        dismissDialog()
    }

    override fun onLoadRows() {
        loadRows()
    }

    private fun showSnackbar(id: Int) {
        Toast.makeText(activity, getString(id), Toast.LENGTH_SHORT).show()
        /*
        Snackbar
                .make(fragmentView, getString(id), Snackbar.LENGTH_SHORT)
                .show();


    ============================================================================

        new AlertDialog.Builder(this)
                .setMessage(getString(id))
                .setPositiveButton("OK", null)
                .show();
*/
    }

    private var readRequestLauncher =
        registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
            if (result.resultCode == Activity.RESULT_OK) {
                if (result.data != null) {
                    val uri = result.data!!.data
                    if (uri != null) {
                        presenter.onSelectFile(uri)
                    }
                }
            }
        }

    @Deprecated("Deprecated in Java")
    override fun onActivityResult(
        requestCode: Int,
        resultCode: Int,
        data: Intent?,
    ) {
        @Suppress("DEPRECATION")
        super.onActivityResult(requestCode, resultCode, data)

        if (initialDialog != null) {
            initialDialog?.dismiss()
            initialDialog = null
        }
        when (requestCode) {
            // READ_REQUEST_CODE -> {
            //    if (resultCode == Activity.RESULT_OK && data != null) {
            //        val uri = data.data
            //        if (uri != null) {
            //           presenter.onSelectFile(uri)
            //        }
            //    }
            // }
            downloadActivity -> {
                if (resultCode == 0) {
                    refreshLevel = 3
                    if (checkStoragePermission() == 0) {
                        updateGameList()
                    }
                }
                if (resultCode == GAMELIST_NEED_TO_UPDATED) {
                    refreshLevel = 3
                    if (checkStoragePermission() == 0) {
                        updateGameList()
                    }
                    updateBackGraound()
                } else if (resultCode == GAMELIST_NEED_TO_RESTART) {
                    val intent = Intent(activity, StartupActivity::class.java)
                    intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_NEW_TASK)
                    startActivity(intent)
                    requireActivity().finish()
                } else {
                    updateBackGraound()
                }
            }
            GameSelectPresenter.YABAUSE_ACTIVITY ->
                if (BuildConfig.BUILD_TYPE != "pro") {
                    val prefs = requireActivity().getSharedPreferences("private", Context.MODE_PRIVATE)
                    val hasDonated = prefs.getBoolean("donated", false)
                    if (hasDonated == false) {
                        val rn = Math.random()
                        if (rn <= 0.5) {
                            val uiModeManager =
                                requireActivity().getSystemService(Context.UI_MODE_SERVICE) as UiModeManager
                            if (uiModeManager.currentModeType != Configuration.UI_MODE_TYPE_TELEVISION) {
                                if (interstitialAd != null) {
                                    interstitialAd!!.show(requireActivity())
                                } else {
                                    val intent = Intent(activity, AdActivity::class.java)
                                    startActivity(intent)
                                }
                            } else {
                                val intent = Intent(activity, AdActivity::class.java)
                                startActivity(intent)
                            }
                        } else if (rn > 0.5) {
                            val intent = Intent(activity, AdActivity::class.java)
                            startActivity(intent)
                        }
                    }
                }
            else -> {
            }
        }
    }

    private fun requestNewInterstitial() {
        val adRequest = AdRequest.Builder().build()
        InterstitialAd.load(
            requireActivity(),
            requireActivity().getString(R.string.banner_ad_unit_id),
            adRequest,
            object : InterstitialAdLoadCallback() {
                override fun onAdLoaded(ad: InterstitialAd) {
                    // The interstitialAd reference will be null until
                    // an ad is loaded.
                    this@GameSelectFragment.interstitialAd = ad
                    // Log.i(BrowseSupportFragment.TAG, "onAdLoaded")
                }

                override fun onAdFailedToLoad(loadAdError: LoadAdError) {
                    // Handle the error
                    // Log.i(BrowseSupportFragment.TAG, loadAdError.message)
                    interstitialAd = null
                }
            },
        )

        interstitialAd?.fullScreenContentCallback =
            object : FullScreenContentCallback() {
                override fun onAdDismissedFullScreenContent() {
                    Log.d(TAG, "Ad was dismissed.")
                    // requestNewInterstitial()
                }

                override fun onAdShowedFullScreenContent() {
                    Log.d(TAG, "Ad showed fullscreen content.")
                    interstitialAd = null
                }
            }
    }

    private fun showMigrationDialogIfNeeded() {
        val prefsManager = PreferencesManager.getInstance(requireContext())
        if (prefsManager.shouldShowMigrationDialog()) {
            AlertDialog
                .Builder(requireContext())
                .setTitle(R.string.migration_dialog_title)
                .setMessage(R.string.migration_dialog_message)
                .setPositiveButton(android.R.string.ok) { _, _ ->
                    prefsManager.markMigrationDialogShown()
                }.setCancelable(false)
                .show()
        }
    }

    var refreshLevel = 0

    companion object {
        private const val TAG = "GameSelectFragment"
        private const val BACKGROUND_UPDATE_DELAY = 300
        private const val GRID_ITEM_WIDTH = 266
        private const val GRID_ITEM_HEIGHT = 200
        private const val NUM_ROWS = 6
        private const val NUM_COLS = 15
        private const val REQUEST_INVITE = 0x1121

        @JvmField
        var isForeground: GameSelectFragment? = null
        private const val REQUEST_STORAGE = 1
        private val PERMISSIONS_STORAGE =
            arrayOf(
                Manifest.permission.READ_EXTERNAL_STORAGE,
                Manifest.permission.WRITE_EXTERNAL_STORAGE,
            )

        /**
         * Get IP address from first non-localhost interface
         *
         * @param ipv4 true=return ipv4, false=return ipv6
         * @return address or empty string
         */
        fun getIPAddress(useIPv4: Boolean): String {
            try {
                val interfaces: List<NetworkInterface> =
                    Collections.list(NetworkInterface.getNetworkInterfaces())
                for (intf in interfaces) {
                    val addrs: List<InetAddress> = Collections.list(intf.inetAddresses)
                    for (addr in addrs) {
                        if (!addr.isLoopbackAddress) {
                            val sAddr = addr.hostAddress ?: continue
                            // boolean isIPv4 = InetAddressUtils.isIPv4Address(sAddr);
                            val isIPv4 = sAddr.indexOf(':') < 0
                            if (useIPv4) {
                                if (isIPv4) return sAddr
                            } else {
                                if (!isIPv4) {
                                    val delim = sAddr.indexOf('%') // drop ip6 zone suffix
                                    return if (delim < 0) {
                                        sAddr.uppercase()
                                    } else {
                                        sAddr
                                            .substring(
                                                0,
                                                delim,
                                            ).uppercase()
                                    }
                                }
                            }
                        }
                    }
                }
            } catch (ex: Exception) {
            } // for now eat exceptions
            return ""
        }

        /**
         * @param context
         * @return
         */
        fun getVersionName(context: Context?): String {
//        return getIPAddress(true);
            val pm = context!!.packageManager
            var versionName = ""
            try {
                val packageInfo = pm.getPackageInfo(context.packageName, 0)
                versionName = packageInfo.versionName ?: ""
            } catch (e: PackageManager.NameNotFoundException) {
                e.printStackTrace()
            }
            return versionName
        }

        const val GAMELIST_NEED_TO_UPDATED = 0x8001
        const val GAMELIST_NEED_TO_RESTART = 0x8002
        // const val READ_REQUEST_CODE = 0x8003
    }
}
