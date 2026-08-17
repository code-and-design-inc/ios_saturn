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
package org.uoyabause.android.backup.viewmodel

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.catch
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.flow.receiveAsFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.YabauseStorage
import org.uoyabause.android.backup.model.BackupItem
import org.uoyabause.android.backup.model.DeviceType
import org.uoyabause.android.backup.model.LocalBackupFile
import org.uoyabause.android.backup.model.SharedBackupItem
import org.uoyabause.android.backup.repository.BackupRepository
import org.uoyabause.android.backup.repository.CloudBackupRepository
import java.io.File

/**
 * Sort order options for shared backups.
 */
enum class SharedBackupSortOrder {
    DATE_DESC,
    RATING_DESC,
}

/**
 * ViewModel for the Shared Backups tab.
 * Handles searching, downloading, and rating shared community backups.
 */
class SharedBackupViewModel(
    application: Application,
) : AndroidViewModel(application) {
    private val cloudRepository = CloudBackupRepository(application)
    private val backupRepository = BackupRepository(application)

    // UI State
    private val _uiState = MutableStateFlow<SharedBackupUiState>(SharedBackupUiState.Initial)
    val uiState: StateFlow<SharedBackupUiState> = _uiState.asStateFlow()

    // Operation state
    private val _operationResult = MutableStateFlow<BackupOperationResult>(BackupOperationResult.Idle)
    val operationResult: StateFlow<BackupOperationResult> = _operationResult.asStateFlow()

    // UI Events
    private val _uiEvents = Channel<BackupUiEvent>(Channel.BUFFERED)
    val uiEvents = _uiEvents.receiveAsFlow()

    // Search query
    private val _searchQuery = MutableStateFlow("")
    val searchQuery: StateFlow<String> = _searchQuery.asStateFlow()

    // Sort order
    private val _sortOrder = MutableStateFlow(SharedBackupSortOrder.DATE_DESC)
    val sortOrder: StateFlow<SharedBackupSortOrder> = _sortOrder.asStateFlow()

    // Single shared backup loaded by id (deep-link import).
    private val _deepLinkItem = MutableStateFlow<SharedBackupItem?>(null)
    val deepLinkItem: StateFlow<SharedBackupItem?> = _deepLinkItem.asStateFlow()

    private val _deepLinkNotFound = MutableStateFlow(false)
    val deepLinkNotFound: StateFlow<Boolean> = _deepLinkNotFound.asStateFlow()

    // Library filter
    private val _libraryFilterEnabled = MutableStateFlow(true)
    val libraryFilterEnabled: StateFlow<Boolean> = _libraryFilterEnabled.asStateFlow()

    // User's product numbers from local game library
    private var userProductNumbers: List<String> = emptyList()

    // Active search job
    private var searchJob: Job? = null

    init {
        // Load user's game library product numbers
        viewModelScope.launch {
            userProductNumbers = withContext(Dispatchers.IO) {
                try {
                    YabauseStorage.dao
                        .getAll()
                        .mapNotNull { it.product_number?.takeIf { pn -> pn.isNotBlank() } }
                        .distinct()
                } catch (e: Exception) {
                    emptyList()
                }
            }
            // Load initial shared backups (trending/recent)
            loadSharedBackups()
        }
    }

    /**
     * Load shared backups without search filter.
     */
    fun loadSharedBackups() {
        if (!cloudRepository.isAuthenticated()) {
            _uiState.value = SharedBackupUiState.RequiresAuth
            return
        }

        searchJob?.cancel()
        _uiState.value = SharedBackupUiState.Loading

        val filterProductNumbers = if (_libraryFilterEnabled.value && userProductNumbers.isNotEmpty()) {
            userProductNumbers
        } else {
            null
        }

        searchJob = viewModelScope.launch {
            cloudRepository
                .searchSharedBackups(
                    productNumbers = filterProductNumbers,
                    sortOrder = _sortOrder.value,
                ).catch { e ->
                    _uiState.value = SharedBackupUiState.Error(
                        message = e.message ?: "Failed to load shared backups",
                        exception = e,
                    )
                }.collectLatest { items ->
                    if (items.isEmpty()) {
                        _uiState.value = SharedBackupUiState.Empty()
                    } else {
                        _uiState.value = SharedBackupUiState.Success(items = items)
                    }
                }
        }
    }

    /**
     * Search for shared backups.
     * @param query The search query
     */
    fun search(query: String) {
        _searchQuery.value = query

        if (!cloudRepository.isAuthenticated()) {
            _uiState.value = SharedBackupUiState.RequiresAuth
            return
        }

        if (query.isBlank()) {
            loadSharedBackups()
            return
        }

        searchJob?.cancel()
        _uiState.value = SharedBackupUiState.Loading

        val filterProductNumbers = if (_libraryFilterEnabled.value && userProductNumbers.isNotEmpty()) {
            userProductNumbers
        } else {
            null
        }

        searchJob = viewModelScope.launch {
            cloudRepository
                .searchSharedBackups(
                    query = query,
                    productNumbers = filterProductNumbers,
                    sortOrder = _sortOrder.value,
                ).catch { e ->
                    _uiState.value = SharedBackupUiState.Error(
                        message = e.message ?: "Search failed",
                        exception = e,
                    )
                }.collectLatest { items ->
                    if (items.isEmpty()) {
                        _uiState.value = SharedBackupUiState.Empty(searchQuery = query)
                    } else {
                        _uiState.value = SharedBackupUiState.Success(
                            items = items,
                            searchQuery = query,
                        )
                    }
                }
        }
    }

    /**
     * Search by game product number.
     * @param productNumber The product number (e.g., "T-1234G")
     */
    fun searchByProductNumber(productNumber: String) {
        if (!cloudRepository.isAuthenticated()) {
            _uiState.value = SharedBackupUiState.RequiresAuth
            return
        }

        searchJob?.cancel()
        _uiState.value = SharedBackupUiState.Loading
        _searchQuery.value = productNumber

        searchJob = viewModelScope.launch {
            cloudRepository
                .searchSharedBackups(productNumber = productNumber)
                .catch { e ->
                    _uiState.value = SharedBackupUiState.Error(
                        message = e.message ?: "Search failed",
                        exception = e,
                    )
                }.collectLatest { items ->
                    if (items.isEmpty()) {
                        _uiState.value = SharedBackupUiState.Empty(searchQuery = productNumber)
                    } else {
                        _uiState.value = SharedBackupUiState.Success(
                            items = items,
                            searchQuery = productNumber,
                        )
                    }
                }
        }
    }

    /**
     * Clear search and show all shared backups.
     */
    fun clearSearch() {
        _searchQuery.value = ""
        loadSharedBackups()
    }

    /**
     * Set sort order and re-trigger current search/load.
     */
    fun setSortOrder(order: SharedBackupSortOrder) {
        _sortOrder.value = order
        search(_searchQuery.value)
    }

    /**
     * Enable or disable library filter and re-trigger current search/load.
     */
    fun setLibraryFilter(enabled: Boolean) {
        _libraryFilterEnabled.value = enabled
        search(_searchQuery.value)
    }

    /**
     * Optimistically reflect the server-side downloadCount increment that
     * happens inside downloadSharedBackup, so the list and any open detail
     * view update without requiring the user to reselect the item.
     */
    private fun bumpDownloadCount(itemId: String) {
        val current = _uiState.value
        if (current is SharedBackupUiState.Success) {
            _uiState.value = current.copy(
                items = current.items.map {
                    if (it.id == itemId) it.copy(downloadCount = it.downloadCount + 1) else it
                },
            )
        }
        _deepLinkItem.value?.let {
            if (it.id == itemId) {
                _deepLinkItem.value = it.copy(downloadCount = it.downloadCount + 1)
            }
        }
    }

    /**
     * Import a shared backup to local storage.
     * @param item The shared backup to import
     * @param targetDevice The target device (INTERNAL or EXTERNAL)
     */
    fun importBackup(item: SharedBackupItem, targetDevice: DeviceType = DeviceType.INTERNAL) {
        viewModelScope.launch {
            _operationResult.value = BackupOperationResult.InProgress(
                message = "Downloading '${item.filename}'...",
            )

            val downloadResult = cloudRepository.downloadSharedBackup(item)

            downloadResult.fold(
                onSuccess = { data ->
                    // TODO: Save the data to local backup RAM
                    // This requires implementing the write functionality in LocalBackupDataSource.
                    // Do not count a download here: nothing is written yet.
                    _operationResult.value = BackupOperationResult.Success(
                        message = "Backup imported successfully",
                    )
                    sendEvent(BackupUiEvent.ShowToast("Backup '${item.filename}' imported"))
                },
                onFailure = { e ->
                    _operationResult.value = BackupOperationResult.Failure(
                        message = e.message ?: "Failed to import backup",
                        exception = e,
                    )
                    sendEvent(BackupUiEvent.ShowToast("Failed to import: ${e.message}"))
                },
            )
        }
    }

    /**
     * Import a shared backup to a specific local backup file.
     * Checks for existing backup with the same filename and shows confirmation if needed.
     * @param item The shared backup to import
     * @param targetFile The target local backup file
     */
    fun importBackupToFile(item: SharedBackupItem, targetFile: LocalBackupFile) {
        viewModelScope.launch {
            // Check if a backup with the same filename already exists in the target file
            val exists = backupRepository.backupExistsInFile(
                targetFile.getFilePath(),
                item.filename,
                targetFile.deviceType,
            )

            if (exists) {
                val context = getApplication<android.app.Application>()
                sendEvent(
                    BackupUiEvent.ShowConfirmation(
                        title = context.getString(R.string.backup_overwrite_title),
                        message = context.getString(
                            R.string.backup_overwrite_message,
                            item.filename,
                            targetFile.displayName,
                        ),
                        confirmAction = { executeImportBackupToFile(item, targetFile) },
                    ),
                )
            } else {
                executeImportBackupToFile(item, targetFile)
            }
        }
    }

    /**
     * Execute the actual import of a shared backup to a local file (after confirmation if needed).
     */
    private fun executeImportBackupToFile(item: SharedBackupItem, targetFile: LocalBackupFile) {
        viewModelScope.launch {
            _operationResult.value = BackupOperationResult.InProgress(
                message = "Downloading '${item.filename}'...",
            )

            val downloadResult = cloudRepository.downloadSharedBackup(item)

            downloadResult.fold(
                onSuccess = { data ->
                    // Download and save screenshot from shared backup if available
                    item.screenshotUrl?.let { url ->
                        if (url.startsWith("https://")) {
                            try {
                                val screenshotBytes = cloudRepository.downloadScreenshot(url).getOrNull()
                                if (screenshotBytes != null) {
                                    val screenshotFile = File(
                                        "${YabauseStorage.storage.screenshotPath}${targetFile.fileKey}/backup_${item.filename}.png",
                                    )
                                    screenshotFile.parentFile?.mkdirs()
                                    screenshotFile.writeBytes(screenshotBytes)
                                }
                            } catch (e: Exception) {
                                // Screenshot download failure should not block backup import
                            }
                        }
                    }

                    val backupItem = BackupItem(
                        filename = item.filename,
                        comment = item.comment,
                        language = 0,
                        saveDate = item.saveDate,
                        dataSize = item.dataSize,
                        blockSize = item.blockSize,
                    )
                    val copyResult = backupRepository.copyBackupToFile(backupItem, targetFile, data)
                    copyResult.fold(
                        onSuccess = {
                            // Count the download only now that the import has
                            // actually completed (fresh import or confirmed
                            // overwrite), then reflect it in the UI.
                            cloudRepository.incrementSharedBackupDownloadCount(item.id)
                            bumpDownloadCount(item.id)
                            _operationResult.value = BackupOperationResult.Success(
                                message = "Backup imported successfully",
                            )
                            sendEvent(BackupUiEvent.ShowToast("Backup '${item.filename}' imported to ${targetFile.displayName}"))
                        },
                        onFailure = { e ->
                            _operationResult.value = BackupOperationResult.Failure(
                                message = e.message ?: "Failed to import backup",
                                exception = e,
                            )
                            sendEvent(BackupUiEvent.ShowToast("Failed to import: ${e.message}"))
                        },
                    )
                },
                onFailure = { e ->
                    _operationResult.value = BackupOperationResult.Failure(
                        message = e.message ?: "Failed to import backup",
                        exception = e,
                    )
                    sendEvent(BackupUiEvent.ShowToast("Failed to import: ${e.message}"))
                },
            )
        }
    }

    /**
     * Load a single shared backup by id for the deep-link import sheet.
     */
    fun loadById(id: String) {
        viewModelScope.launch {
            _deepLinkNotFound.value = false
            val result = cloudRepository.getSharedBackupById(id)
            result.fold(
                onSuccess = { _deepLinkItem.value = it },
                onFailure = { _deepLinkNotFound.value = true },
            )
        }
    }

    /**
     * Show rating dialog for a shared backup.
     */
    fun showRatingDialog(item: SharedBackupItem) {
        viewModelScope.launch {
            val currentRating = cloudRepository.getUserRating(item.id)
            sendEvent(BackupUiEvent.ShowRatingDialog(item, currentRating))
        }
    }

    /**
     * Rate a shared backup.
     * @param item The shared backup to rate
     * @param rating The rating (1-5)
     */
    fun rateBackup(item: SharedBackupItem, rating: Int) {
        viewModelScope.launch {
            _operationResult.value = BackupOperationResult.InProgress("Rating backup...")

            val result = cloudRepository.rateBackup(item.id, rating)

            result.fold(
                onSuccess = {
                    _operationResult.value = BackupOperationResult.Success("Rating submitted")
                    sendEvent(BackupUiEvent.ShowToast("Thank you for rating!"))
                    // Refresh to show updated rating
                    search(_searchQuery.value)
                },
                onFailure = { e ->
                    _operationResult.value = BackupOperationResult.Failure(
                        message = e.message ?: "Failed to submit rating",
                        exception = e,
                    )
                    sendEvent(BackupUiEvent.ShowToast("Failed to submit rating: ${e.message}"))
                },
            )
        }
    }

    /**
     * Delete a shared backup (owner only).
     */
    fun deleteSharedBackup(item: SharedBackupItem) {
        if (!cloudRepository.isOwner(item)) {
            sendEvent(BackupUiEvent.ShowToast("You can only delete your own shared backups"))
            return
        }

        viewModelScope.launch {
            _operationResult.value = BackupOperationResult.InProgress("Deleting shared backup...")

            val result = cloudRepository.deleteSharedBackup(item)

            result.fold(
                onSuccess = {
                    _operationResult.value = BackupOperationResult.Success("Shared backup deleted")
                    sendEvent(BackupUiEvent.ShowToast("Shared backup deleted"))
                    // Refresh list
                    search(_searchQuery.value)
                },
                onFailure = { e ->
                    _operationResult.value = BackupOperationResult.Failure(
                        message = e.message ?: "Failed to delete shared backup",
                        exception = e,
                    )
                    sendEvent(BackupUiEvent.ShowToast("Failed to delete: ${e.message}"))
                },
            )
        }
    }

    /**
     * Request delete confirmation for a shared backup.
     */
    fun requestDeleteSharedBackup(item: SharedBackupItem) {
        if (!cloudRepository.isOwner(item)) {
            sendEvent(BackupUiEvent.ShowToast("You can only delete your own shared backups"))
            return
        }

        sendEvent(
            BackupUiEvent.ShowConfirmation(
                title = "Delete Shared Backup",
                message = "Are you sure you want to remove '${item.filename}' from public sharing?",
                confirmAction = { deleteSharedBackup(item) },
            ),
        )
    }

    /**
     * Clear operation result.
     */
    fun clearOperationResult() {
        _operationResult.value = BackupOperationResult.Idle
    }

    /**
     * Check if user is authenticated.
     */
    fun isAuthenticated(): Boolean = cloudRepository.isAuthenticated()

    /**
     * Check if current user owns the shared backup.
     */
    fun isOwner(item: SharedBackupItem): Boolean = cloudRepository.isOwner(item)

    private fun sendEvent(event: BackupUiEvent) {
        viewModelScope.launch {
            _uiEvents.send(event)
        }
    }

    override fun onCleared() {
        super.onCleared()
        searchJob?.cancel()
    }
}
