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

import android.content.Context
import android.content.res.Configuration
import android.media.AudioManager
import android.os.Bundle
import android.util.Log
import android.view.KeyEvent
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.animation.AccelerateInterpolator
import android.view.animation.DecelerateInterpolator
import android.view.inputmethod.InputMethodManager
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.PopupMenu
import android.widget.TextView
import android.widget.Toast
import androidx.activity.OnBackPressedCallback
import androidx.appcompat.widget.SearchView
import androidx.appcompat.widget.Toolbar
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.recyclerview.widget.RecyclerView
import androidx.viewpager2.adapter.FragmentStateAdapter
import androidx.viewpager2.widget.ViewPager2
import com.google.android.material.button.MaterialButton
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.navigationrail.NavigationRailView
import com.google.android.material.progressindicator.LinearProgressIndicator
import com.google.android.material.tabs.TabLayout
import com.google.android.material.tabs.TabLayoutMediator
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.YabauseApplication
import org.uoyabause.android.backup.model.BackupSortMode
import org.uoyabause.android.backup.model.DeviceType
import org.uoyabause.android.backup.model.LocalBackupFile
import org.uoyabause.android.backup.repository.ShareInfo
import org.uoyabause.android.backup.viewmodel.BackupManagerViewModel
import org.uoyabause.android.backup.viewmodel.BackupOperationResult
import org.uoyabause.android.backup.viewmodel.BackupUiEvent
import org.uoyabause.android.backup.viewmodel.SharedBackupSortOrder

/**
 * Main fragment for the standalone Backup Manager.
 * In portrait: TabLayout with ViewPager2 for Local, Cloud, and Shared tabs.
 * In landscape: NavigationRail with fragment container for the same tabs.
 */
class BackupManagerFragment : Fragment() {
    private val viewModel: BackupManagerViewModel by activityViewModels()

    private val isLandscape: Boolean
        get() = resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE

    private lateinit var toolbar: Toolbar
    private lateinit var progressIndicator: LinearProgressIndicator

    // Modal progress shown while a share upload is in flight (input dialog is gone by then).
    private var shareProgressDialog: androidx.appcompat.app.AlertDialog? = null

    // Search/Sort views
    private lateinit var btnSearch: MaterialButton
    private lateinit var btnSort: MaterialButton
    private lateinit var searchBarContainer: LinearLayout
    private lateinit var searchView: SearchView
    private lateinit var btnCloseSearch: MaterialButton

    // Portrait-only views
    private var tabLayout: TabLayout? = null
    private var viewPager: ViewPager2? = null

    // Landscape-only views
    private var navigationRail: NavigationRailView? = null
    private var fragmentContainer: FrameLayout? = null

    private var tabLayoutMediator: TabLayoutMediator? = null

    private val searchBarBackCallback = object : OnBackPressedCallback(false) {
        override fun handleOnBackPressed() {
            hideSearchBar()
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View? = inflater.inflate(R.layout.fragment_backup_manager, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        setupViews(view)
        if (isLandscape) {
            setupNavigationRail()
        } else {
            setupViewPager()
        }
        setupSearchAndSort()
        observeViewModel()
        setupAllFocusAnimations()
        setupGamepadNavigation(view)

        requireActivity().onBackPressedDispatcher.addCallback(viewLifecycleOwner, searchBarBackCallback)
    }

    private fun setupViews(view: View) {
        toolbar = view.findViewById(R.id.toolbar_backup_manager)
        progressIndicator = view.findViewById(R.id.progress_indicator)

        // Search/Sort views
        btnSearch = view.findViewById(R.id.btn_search)
        btnSort = view.findViewById(R.id.btn_sort)
        searchBarContainer = view.findViewById(R.id.search_bar_container)
        searchView = view.findViewById(R.id.search_view)
        btnCloseSearch = view.findViewById(R.id.btn_close_search)

        // Setup toolbar navigation
        toolbar.setNavigationOnClickListener {
            requireActivity().onBackPressedDispatcher.onBackPressed()
        }

        if (isLandscape) {
            navigationRail = view.findViewById(R.id.navigation_rail)
            fragmentContainer = view.findViewById(R.id.fragment_container)
        } else {
            tabLayout = view.findViewById(R.id.tab_layout_backup)
            viewPager = view.findViewById(R.id.view_pager_backup)
        }
    }

    private fun setupNavigationRail() {
        val rail = navigationRail ?: return

        // Determine initial fragment from ViewModel state
        val currentTab = viewModel.screenState.value.currentTab
        val initialMenuId = when (currentTab) {
            DeviceType.CLOUD -> R.id.nav_cloud
            DeviceType.SHARED -> R.id.nav_shared
            else -> R.id.nav_local
        }
        rail.selectedItemId = initialMenuId

        // Show initial fragment
        showFragmentForTab(currentTab)

        rail.setOnItemSelectedListener { menuItem ->
            val deviceType = when (menuItem.itemId) {
                R.id.nav_local -> DeviceType.INTERNAL
                R.id.nav_cloud -> DeviceType.CLOUD
                R.id.nav_shared -> DeviceType.SHARED
                else -> return@setOnItemSelectedListener false
            }
            viewModel.selectTab(deviceType)
            showFragmentForTab(deviceType)
            syncSearchBarWithTab(deviceType)
            true
        }
    }

    private fun showFragmentForTab(deviceType: DeviceType) {
        val fragment = when (deviceType) {
            DeviceType.INTERNAL -> BackupListFragment.newLocalInstance()
            DeviceType.CLOUD -> BackupListFragment.newInstance(DeviceType.CLOUD)
            DeviceType.SHARED -> SharedBackupSearchFragment.newInstance()
            else -> BackupListFragment.newLocalInstance()
        }
        childFragmentManager
            .beginTransaction()
            .replace(R.id.fragment_container, fragment)
            .commit()
    }

    private fun setupViewPager() {
        val pager = viewPager ?: return
        val tabs = tabLayout ?: return

        val adapter = BackupPagerAdapter(this)
        pager.adapter = adapter

        // Connect TabLayout with ViewPager2
        tabLayoutMediator = TabLayoutMediator(tabs, pager) { tab, position ->
            tab.text = when (position) {
                0 -> getString(R.string.tab_local)
                1 -> getString(R.string.tab_cloud)
                2 -> getString(R.string.tab_shared)
                else -> ""
            }
        }
        tabLayoutMediator?.attach()

        // Listen for tab changes
        pager.registerOnPageChangeCallback(object : ViewPager2.OnPageChangeCallback() {
            override fun onPageSelected(position: Int) {
                super.onPageSelected(position)
                val deviceType = DeviceType.fromTabPosition(position)
                viewModel.selectTab(deviceType)
                syncSearchBarWithTab(deviceType)
            }
        })
    }

    private fun setupSearchAndSort() {
        btnSearch.setOnClickListener { showSearchBar() }
        btnCloseSearch.setOnClickListener { hideSearchBar() }
        btnSort.setOnClickListener { showSortMenu(it) }

        searchView.setOnQueryTextListener(object : SearchView.OnQueryTextListener {
            override fun onQueryTextSubmit(query: String?): Boolean {
                val currentTab = viewModel.screenState.value.currentTab
                if (currentTab == DeviceType.SHARED) {
                    findSharedFragment()?.onToolbarSearchQuery(query ?: "")
                } else {
                    viewModel.setSearchQuery(currentTab, query ?: "")
                }
                // Hide keyboard on submit so the filtered list is visible
                searchView.clearFocus()
                val imm = requireContext().getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
                imm.hideSoftInputFromWindow(searchView.windowToken, 0)
                return true
            }

            override fun onQueryTextChange(newText: String?): Boolean {
                val currentTab = viewModel.screenState.value.currentTab
                if (currentTab == DeviceType.SHARED) {
                    findSharedFragment()?.onToolbarSearchQuery(newText ?: "")
                } else {
                    viewModel.setSearchQuery(currentTab, newText ?: "")
                }
                return true
            }
        })

        // Restore search bar state if query is active
        val state = viewModel.screenState.value
        val currentQuery = when (state.currentTab) {
            DeviceType.INTERNAL, DeviceType.EXTERNAL -> state.localSearchQuery
            DeviceType.CLOUD -> state.cloudSearchQuery
            else -> ""
        }
        if (currentQuery.isNotEmpty()) {
            searchBarContainer.visibility = View.VISIBLE
            searchView.setQuery(currentQuery, false)
            searchBarBackCallback.isEnabled = true
        }
    }

    private fun showSearchBar() {
        searchBarContainer.visibility = View.VISIBLE
        searchBarBackCallback.isEnabled = true
        searchView.requestFocus()
        val imm = requireContext().getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
        imm.showSoftInput(searchView, InputMethodManager.SHOW_IMPLICIT)
    }

    private fun hideSearchBar() {
        searchBarContainer.visibility = View.GONE
        searchBarBackCallback.isEnabled = false
        searchView.setQuery("", false)
        val currentTab = viewModel.screenState.value.currentTab
        if (currentTab == DeviceType.SHARED) {
            findSharedFragment()?.onToolbarSearchQuery("")
        } else {
            viewModel.setSearchQuery(currentTab, "")
        }
        val imm = requireContext().getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
        imm.hideSoftInputFromWindow(searchView.windowToken, 0)
    }

    private fun syncSearchBarWithTab(deviceType: DeviceType) {
        val query = when (deviceType) {
            DeviceType.INTERNAL, DeviceType.EXTERNAL -> viewModel.screenState.value.localSearchQuery
            DeviceType.CLOUD -> viewModel.screenState.value.cloudSearchQuery
            DeviceType.SHARED -> findSharedFragment()?.getCurrentSearchQuery() ?: ""
        }
        if (query.isNotEmpty()) {
            searchBarContainer.visibility = View.VISIBLE
            searchView.setQuery(query, false)
            searchBarBackCallback.isEnabled = true
        } else {
            searchBarContainer.visibility = View.GONE
            searchView.setQuery("", false)
            searchBarBackCallback.isEnabled = false
        }
    }

    private fun showSortMenu(anchor: View) {
        val currentTab = viewModel.screenState.value.currentTab
        val isSharedTab = currentTab == DeviceType.SHARED

        val popup = PopupMenu(requireContext(), anchor)
        popup.menuInflater.inflate(
            if (isSharedTab) R.menu.shared_backup_sort_menu else R.menu.backup_sort_menu,
            popup.menu,
        )

        if (isSharedTab) {
            val currentOrder = findSharedFragment()?.getCurrentSortOrder()
                ?: SharedBackupSortOrder.DATE_DESC
            when (currentOrder) {
                SharedBackupSortOrder.DATE_DESC ->
                    popup.menu.findItem(R.id.sort_by_date_shared)?.isChecked = true
                SharedBackupSortOrder.RATING_DESC ->
                    popup.menu.findItem(R.id.sort_by_rating)?.isChecked = true
            }
        } else {
            val currentMode = when (currentTab) {
                DeviceType.INTERNAL, DeviceType.EXTERNAL -> viewModel.screenState.value.localSortMode
                DeviceType.CLOUD -> viewModel.screenState.value.cloudSortMode
                else -> BackupSortMode.DATE_DESC
            }
            when (currentMode) {
                BackupSortMode.NAME_ASC -> popup.menu.findItem(R.id.sort_by_name_asc)?.isChecked = true
                BackupSortMode.NAME_DESC -> popup.menu.findItem(R.id.sort_by_name_desc)?.isChecked = true
                BackupSortMode.DATE_DESC -> popup.menu.findItem(R.id.sort_by_date_desc)?.isChecked = true
                BackupSortMode.DATE_ASC -> popup.menu.findItem(R.id.sort_by_date_asc)?.isChecked = true
            }
        }

        popup.setOnMenuItemClickListener { item ->
            when (item.itemId) {
                R.id.sort_by_name_asc -> {
                    viewModel.setSortMode(currentTab, BackupSortMode.NAME_ASC)
                    true
                }
                R.id.sort_by_name_desc -> {
                    viewModel.setSortMode(currentTab, BackupSortMode.NAME_DESC)
                    true
                }
                R.id.sort_by_date_desc -> {
                    viewModel.setSortMode(currentTab, BackupSortMode.DATE_DESC)
                    true
                }
                R.id.sort_by_date_asc -> {
                    viewModel.setSortMode(currentTab, BackupSortMode.DATE_ASC)
                    true
                }
                R.id.sort_by_date_shared -> {
                    findSharedFragment()?.onToolbarSortSelected(SharedBackupSortOrder.DATE_DESC)
                    true
                }
                R.id.sort_by_rating -> {
                    findSharedFragment()?.onToolbarSortSelected(SharedBackupSortOrder.RATING_DESC)
                    true
                }
                else -> false
            }
        }
        popup.show()
    }

    private fun findSharedFragment(): SharedBackupSearchFragment? =
        childFragmentManager.fragments
            .filterIsInstance<SharedBackupSearchFragment>()
            .firstOrNull()

    private fun observeViewModel() {
        viewLifecycleOwner.lifecycleScope.launch {
            viewLifecycleOwner.repeatOnLifecycle(Lifecycle.State.STARTED) {
                launch {
                    viewModel.screenState.collectLatest { state ->
                        // Update progress indicator visibility
                        when (state.operationResult) {
                            is BackupOperationResult.InProgress -> {
                                progressIndicator.visibility = View.VISIBLE
                            }
                            else -> {
                                progressIndicator.visibility = View.GONE
                            }
                        }
                    }
                }

                launch {
                    viewModel.uiEvents.collectLatest { event ->
                        handleUiEvent(event)
                    }
                }
            }
        }
    }

    private fun handleUiEvent(event: BackupUiEvent) {
        when (event) {
            is BackupUiEvent.ShowToast -> {
                dismissShareProgress()
                Toast.makeText(requireContext(), event.message, Toast.LENGTH_SHORT).show()
            }
            is BackupUiEvent.ShowSnackbar -> {
                dismissShareProgress()
                Toast.makeText(requireContext(), event.message, Toast.LENGTH_SHORT).show()
            }
            is BackupUiEvent.ShowConfirmation -> {
                val dialog = MaterialAlertDialogBuilder(requireContext())
                    .setTitle(event.title)
                    .setMessage(event.message)
                    .setPositiveButton(R.string.confirm) { _, _ ->
                        event.confirmAction()
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
            is BackupUiEvent.ShowShareDialog -> {
                showShareWithConsent(event.item)
            }
            is BackupUiEvent.ShowRatingDialog -> {
                showRatingDialog(event)
            }
            is BackupUiEvent.ShowCopyToDialog -> {
                showCopyToDialog(event)
            }
            is BackupUiEvent.RequestExportPicker -> {
                // Launch SAF picker for export
                // TODO: Implement with ActivityResultContracts
            }
            is BackupUiEvent.RequestImportPicker -> {
                // Launch SAF picker for import
                // TODO: Implement with ActivityResultContracts
            }
            is BackupUiEvent.Navigate -> {
                // Handle navigation
            }
            is BackupUiEvent.DismissDialog -> {
                // Dismiss any active dialog
            }
            is BackupUiEvent.LaunchShareSheet -> {
                dismissShareProgress()
                val sendIntent = android.content.Intent(android.content.Intent.ACTION_SEND).apply {
                    type = "text/plain"
                    putExtra(android.content.Intent.EXTRA_SUBJECT, getString(R.string.share_backup_link_subject))
                    putExtra(
                        android.content.Intent.EXTRA_TEXT,
                        getString(
                            R.string.share_backup_link_text,
                            event.gameTitle,
                            event.url,
                            org.uoyabause.android.backup.BackupShareLink.hashtags(event.gameTitle),
                        ),
                    )
                }
                startActivity(
                    android.content.Intent.createChooser(
                        sendIntent,
                        getString(R.string.share_backup_link_chooser),
                    ),
                )
            }
        }
    }

    private fun showCopyToDialog(event: BackupUiEvent.ShowCopyToDialog) {
        val item = event.item
        val existingFiles = LocalBackupFile.getExistingFiles()

        // Build destination list: local files + Cloud
        val destinations = mutableListOf<String>()
        for (file in existingFiles) {
            destinations.add(file.displayName)
        }
        if (viewModel.isAuthenticated()) {
            destinations.add(getString(R.string.tab_cloud))
        }

        MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.select_copy_destination)
            .setItems(destinations.toTypedArray()) { _, which ->
                if (which < existingFiles.size) {
                    // Copy to a local file
                    viewModel.copyBackupToLocalFile(item, existingFiles[which])
                } else {
                    // Copy to cloud requires Pro
                    if (!YabauseApplication.isPro()) {
                        YabauseApplication.checkDonated(requireActivity())
                        return@setItems
                    }
                    viewModel.copyBackupToCloud(item)
                }
            }.show()
    }

    private fun showShareProgress() {
        dismissShareProgress()
        val progressView = LayoutInflater
            .from(requireContext())
            .inflate(R.layout.dialog_share_progress, null)
        shareProgressDialog = MaterialAlertDialogBuilder(requireContext())
            .setView(progressView)
            .setCancelable(false)
            .create()
        shareProgressDialog?.show()
    }

    private fun dismissShareProgress() {
        shareProgressDialog?.dismiss()
        shareProgressDialog = null
    }

    private fun showShareDialog(event: BackupUiEvent.ShowShareDialog) {
        val item = event.item
        val dialogView = LayoutInflater
            .from(requireContext())
            .inflate(R.layout.dialog_share_backup, null)

        val tvBackupInfo = dialogView.findViewById<TextView>(R.id.tv_backup_info)
        val tilGameTitle = dialogView.findViewById<com.google.android.material.textfield.TextInputLayout>(R.id.til_game_title)
        val etGameTitle = dialogView.findViewById<com.google.android.material.textfield.TextInputEditText>(R.id.et_game_title)
        val etProductNumber = dialogView.findViewById<com.google.android.material.textfield.TextInputEditText>(R.id.et_product_number)
        val etDescription = dialogView.findViewById<com.google.android.material.textfield.TextInputEditText>(R.id.et_description)

        tvBackupInfo.text = "${item.filename} - ${item.displaySize}"
        etGameTitle.setText(item.gameTitle ?: "")
        etProductNumber.setText(item.productNumber ?: "")

        val dialog = MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.share_backup_title)
            .setView(dialogView)
            .setPositiveButton(R.string.share, null)
            .setNegativeButton(R.string.cancel, null)
            .create()

        dialog.setOnShowListener {
            val positiveButton = dialog.getButton(android.content.DialogInterface.BUTTON_POSITIVE)
            positiveButton.setOnClickListener {
                val gameTitle = etGameTitle.text?.toString()?.trim() ?: ""
                if (gameTitle.isEmpty()) {
                    tilGameTitle.error = getString(R.string.game_title_required)
                    return@setOnClickListener
                }
                tilGameTitle.error = null

                val shareInfo = ShareInfo(
                    gameTitle = gameTitle,
                    productNumber = etProductNumber.text?.toString()?.trim() ?: "",
                    description = etDescription.text?.toString()?.trim() ?: "",
                )
                viewModel.shareBackup(item, shareInfo)
                dialog.dismiss()
                showShareProgress()
            }

            // Gamepad A-button support for dialog buttons
            val buttons = listOf(positiveButton, dialog.getButton(android.content.DialogInterface.BUTTON_NEGATIVE))
            buttons.filterNotNull().forEach { btn ->
                btn.setOnKeyListener { v, keyCode, keyEvent ->
                    if (keyEvent.action == KeyEvent.ACTION_DOWN &&
                        (keyCode == KeyEvent.KEYCODE_BUTTON_A || keyCode == KeyEvent.KEYCODE_DPAD_CENTER)
                    ) {
                        v.performClick()
                        true
                    } else {
                        false
                    }
                }
            }
        }

        dialog.show()
    }

    private fun showShareWithConsent(item: org.uoyabause.android.backup.model.BackupItem) {
        val prefs = requireContext()
            .getSharedPreferences("backup_share", android.content.Context.MODE_PRIVATE)
        val consented = prefs.getBoolean("public_share_consented", false)
        if (consented) {
            showShareDialog(BackupUiEvent.ShowShareDialog(item))
            return
        }
        val dialog = com.google.android.material.dialog
            .MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.share_public_consent_title)
            .setMessage(R.string.share_public_consent_message)
            .setPositiveButton(R.string.confirm) { _, _ ->
                prefs.edit().putBoolean("public_share_consented", true).apply()
                showShareDialog(BackupUiEvent.ShowShareDialog(item))
            }.setNegativeButton(R.string.cancel, null)
            .create()
        dialog.setOnShowListener {
            val negative = dialog.getButton(android.content.DialogInterface.BUTTON_NEGATIVE)
            negative?.post {
                negative.isFocusable = true
                negative.isFocusableInTouchMode = true
                negative.requestFocus()
            }
        }
        dialog.show()
    }

    private fun showRatingDialog(event: BackupUiEvent.ShowRatingDialog) {
        val ratings = arrayOf("1 Star", "2 Stars", "3 Stars", "4 Stars", "5 Stars")
        val selectedIndex = (event.currentRating ?: 3) - 1

        val dialog = MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.rate_backup)
            .setSingleChoiceItems(ratings, selectedIndex, null)
            .setPositiveButton(R.string.confirm) { dlg, _ ->
                val listView = (dlg as androidx.appcompat.app.AlertDialog).listView
                val selected = listView.checkedItemPosition + 1
                // Delegate to SharedBackupViewModel via the child fragment
                val sharedFragment = childFragmentManager.fragments
                    .filterIsInstance<SharedBackupSearchFragment>()
                    .firstOrNull()
                // Use the BackupManagerViewModel's event channel to avoid cross-VM calls
                Toast.makeText(requireContext(), getString(R.string.rating_submitted), Toast.LENGTH_SHORT).show()
            }.setNegativeButton(R.string.cancel, null)
            .create()
        dialog.setOnShowListener {
            val positiveButton = dialog.getButton(android.content.DialogInterface.BUTTON_POSITIVE)
            positiveButton?.post {
                positiveButton.isFocusable = true
                positiveButton.isFocusableInTouchMode = true
                positiveButton.requestFocus()
            }
        }
        dialog.show()
    }

    override fun onDestroyView() {
        dismissShareProgress()
        searchBarBackCallback.isEnabled = false
        if (!isLandscape) {
            tabLayoutMediator?.detach()
            tabLayoutMediator = null
            viewPager?.adapter = null
        }
        tabLayout = null
        viewPager = null
        navigationRail = null
        fragmentContainer = null
        super.onDestroyView()
    }

    // region Gamepad controller support

    private fun playFocusSound() {
        try {
            val audioManager = requireContext().getSystemService(Context.AUDIO_SERVICE) as AudioManager
            audioManager.playSoundEffect(AudioManager.FX_FOCUS_NAVIGATION_UP)
        } catch (e: Exception) {
            Log.d(TAG, "Failed to play sound: ${e.message}")
        }
    }

    private fun setupFocusAnimations(view: View) {
        if (view is Button) {
            view.setTag(R.id.original_text_color, view.currentTextColor)
        }

        view.setOnFocusChangeListener { v, hasFocus ->
            if (hasFocus) {
                playFocusSound()

                v
                    .animate()
                    .scaleX(1.05f)
                    .scaleY(1.05f)
                    .alpha(1.0f)
                    .setDuration(150)
                    .setInterpolator(DecelerateInterpolator())
                    .start()

                if (v is Button || v.isClickable) {
                    v.backgroundTintList = ContextCompat
                        .getColorStateList(requireContext(), R.color.colorAccent)
                    if (v is Button) {
                        v.setTextColor(
                            ContextCompat.getColor(requireContext(), R.color.colorPrimaryDark),
                        )
                    }
                }
            } else {
                v
                    .animate()
                    .scaleX(1.0f)
                    .scaleY(1.0f)
                    .alpha(0.9f)
                    .setDuration(150)
                    .setInterpolator(AccelerateInterpolator())
                    .start()

                if (v is Button || v.isClickable) {
                    v.backgroundTintList = null
                    if (v is Button) {
                        val originalColor = v.getTag(R.id.original_text_color) as? Int
                        if (originalColor != null) {
                            v.setTextColor(originalColor)
                        }
                    }
                }
            }
        }

        view.setOnKeyListener { v, keyCode, event ->
            if (event.action == KeyEvent.ACTION_DOWN) {
                when (keyCode) {
                    KeyEvent.KEYCODE_DPAD_CENTER,
                    KeyEvent.KEYCODE_ENTER,
                    KeyEvent.KEYCODE_NUMPAD_ENTER,
                    KeyEvent.KEYCODE_BUTTON_A,
                    -> {
                        if (v.isClickable) {
                            v.performClick()
                            true
                        } else {
                            false
                        }
                    }
                    else -> false
                }
            } else {
                false
            }
        }
    }

    private fun setupAllFocusAnimations() {
        setupFocusAnimations(btnSearch)
        setupFocusAnimations(btnSort)
        setupFocusAnimations(btnCloseSearch)
        if (!isLandscape) {
            tabLayout?.let { setupFocusAnimations(it) }
        }
    }

    private fun setupGamepadNavigation(rootView: View) {
        // Children should receive focus before any container
        if (rootView is ViewGroup) {
            rootView.descendantFocusability = ViewGroup.FOCUS_AFTER_DESCENDANTS
        }

        // B button for back navigation (root is last-resort focus target)
        rootView.isFocusableInTouchMode = true
        rootView.setOnKeyListener { _, keyCode, event ->
            if (event.action == KeyEvent.ACTION_DOWN && keyCode == KeyEvent.KEYCODE_BUTTON_B) {
                requireActivity().onBackPressedDispatcher.onBackPressed()
                true
            } else {
                false
            }
        }

        if (isLandscape) {
            setupLandscapeGamepadNavigation()
        } else {
            setupPortraitGamepadNavigation()
        }

        // 子フラグメントのコンテンツに初期フォーカスを当てる
        rootView.post {
            requestFocusOnContent()
        }
    }

    private fun requestFocusOnContent() {
        // Try to find a focusable item in the currently visible child fragment's RecyclerView
        val recyclerViewIds = intArrayOf(R.id.recycler_view_backups, R.id.recycler_view_shared)
        for (fragment in childFragmentManager.fragments) {
            for (rvId in recyclerViewIds) {
                val rv = fragment.view?.findViewById<RecyclerView>(rvId)
                if (rv != null) {
                    if (rv.childCount > 0) {
                        rv.getChildAt(0)?.requestFocus()
                        return
                    }
                    // RecyclerView exists but items haven't been laid out yet; retry after layout
                    rv.post {
                        if (rv.childCount > 0) {
                            rv.getChildAt(0)?.requestFocus()
                        }
                    }
                    return
                }
            }
        }
        // Fallback to tab layout or navigation rail
        if (isLandscape) {
            navigationRail?.requestFocus()
        } else {
            tabLayout?.requestFocus()
        }
    }

    private fun setupPortraitGamepadNavigation() {
        val tabs = tabLayout ?: return

        // Ensure ViewPager2 delegates focus to its child fragments
        viewPager?.descendantFocusability = ViewGroup.FOCUS_AFTER_DESCENDANTS

        // TabLayout -> ViewPager content (down)
        tabs.nextFocusDownId = R.id.view_pager_backup
    }

    private fun setupLandscapeGamepadNavigation() {
        val rail = navigationRail ?: return

        // Ensure fragment container delegates focus to its child fragment content
        fragmentContainer?.descendantFocusability = ViewGroup.FOCUS_AFTER_DESCENDANTS

        // NavigationRail -> Fragment content (right)
        rail.nextFocusRightId = R.id.fragment_container

        // Fragment container -> NavigationRail (left)
        fragmentContainer?.nextFocusLeftId = R.id.navigation_rail
    }

    // endregion

    companion object {
        const val TAG = "BackupManagerFragment"

        fun newInstance(): BackupManagerFragment = BackupManagerFragment()
    }

    /**
     * ViewPager adapter for backup tabs (portrait only).
     */
    private inner class BackupPagerAdapter(
        fragment: Fragment,
    ) : FragmentStateAdapter(fragment) {
        override fun getItemCount(): Int = 3 // Local, Cloud, Shared

        override fun createFragment(position: Int): Fragment = when (position) {
            0 -> BackupListFragment.newLocalInstance()
            1 -> BackupListFragment.newInstance(DeviceType.CLOUD)
            2 -> SharedBackupSearchFragment.newInstance()
            else -> BackupListFragment.newLocalInstance()
        }
    }
}
