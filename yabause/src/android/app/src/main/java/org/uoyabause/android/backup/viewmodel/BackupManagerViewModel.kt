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
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.catch
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.flow.receiveAsFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.YabauseStorage
import org.uoyabause.android.backup.BackupShareLink
import org.uoyabause.android.backup.model.BackupItem
import org.uoyabause.android.backup.model.BackupSortMode
import org.uoyabause.android.backup.model.DeviceType
import org.uoyabause.android.backup.model.LocalBackupFile
import org.uoyabause.android.backup.repository.BackupRepository
import org.uoyabause.android.backup.repository.CloudBackupRepository
import org.uoyabause.android.backup.repository.ShareInfo
import java.io.File

/**
 * ViewModel for the Backup Manager screen.
 * Handles backup operations for Local, Cloud, and Shared storage.
 */
class BackupManagerViewModel(
    application: Application,
) : AndroidViewModel(application) {
    private val backupRepository = BackupRepository(application)
    private val cloudRepository = CloudBackupRepository(application)

    // UI State
    private val _screenState = MutableStateFlow(BackupManagerScreenState())
    val screenState: StateFlow<BackupManagerScreenState> = _screenState.asStateFlow()

    // UI Events
    private val _uiEvents = Channel<BackupUiEvent>(Channel.BUFFERED)
    val uiEvents = _uiEvents.receiveAsFlow()

    // Current local backup loading job (cancelled when switching files)
    private var localBackupJob: kotlinx.coroutines.Job? = null

    // Current cloud backup items (for real-time updates)
    private var cloudBackupJob: kotlinx.coroutines.Job? = null

    // Raw (unfiltered) items for re-filtering when search/sort changes
    private var rawLocalItems: List<BackupItem> = emptyList()
    private var rawCloudItems: List<BackupItem> = emptyList()

    init {
        // Discover existing local backup files and auto-select MEMORY_STANDARD as default
        val existingFiles = LocalBackupFile.getExistingFiles()
        val firstFile = existingFiles.find { it == LocalBackupFile.MEMORY_STANDARD }
            ?: existingFiles.firstOrNull()
        _screenState.update {
            it.copy(
                availableLocalFiles = existingFiles,
                selectedLocalFile = firstFile,
            )
        }
        if (firstFile != null) {
            loadLocalBackup(firstFile)
        } else {
            _screenState.update {
                it.copy(localState = BackupUiState.Empty("No local backup files found"))
            }
        }
    }

    /**
     * Switch to a different tab.
     */
    fun selectTab(deviceType: DeviceType) {
        _screenState.update { it.copy(currentTab = deviceType) }
        loadBackups(deviceType)
    }

    /**
     * Load backups for the specified device type.
     */
    fun loadBackups(deviceType: DeviceType) {
        when (deviceType) {
            DeviceType.INTERNAL -> {
                // Local tab: reload current selected local file
                val selectedFile = _screenState.value.selectedLocalFile
                if (selectedFile != null) {
                    loadLocalBackup(selectedFile)
                }
            }
            DeviceType.EXTERNAL -> {
                // Also treated as local tab
                val selectedFile = _screenState.value.selectedLocalFile
                if (selectedFile != null) {
                    loadLocalBackup(selectedFile)
                }
            }
            DeviceType.CLOUD -> loadCloudBackups()
            DeviceType.SHARED -> { /* Handled by SharedBackupViewModel */ }
        }
    }

    /**
     * Select a local backup file and load its contents.
     */
    fun selectLocalFile(file: LocalBackupFile) {
        _screenState.update { it.copy(selectedLocalFile = file) }
        loadLocalBackup(file)
    }

    /**
     * Refresh current tab's data.
     */
    fun refresh() {
        _screenState.update { it.copy(isRefreshing = true) }
        loadBackups(_screenState.value.currentTab)
    }

    private fun loadLocalBackup(file: LocalBackupFile) {
        // Cancel any previous load to prevent stale data from overwriting
        localBackupJob?.cancel()
        localBackupJob = viewModelScope.launch {
            _screenState.update { it.copy(localState = BackupUiState.Loading) }

            try {
                val filePath = file.getFilePath()
                val items = backupRepository.getBackupItemsFromFile(filePath, file.deviceType)
                val storageStatus = backupRepository.getStorageStatusFromFile(
                    filePath,
                    file.biosSize,
                    file.biosBlockSize,
                )

                if (items.isEmpty()) {
                    rawLocalItems = emptyList()
                    _screenState.update {
                        it.copy(
                            localState = BackupUiState.Empty(
                                message = "No backups found",
                                storageStatus = storageStatus,
                            ),
                            isRefreshing = false,
                        )
                    }
                } else {
                    rawLocalItems = items
                    val state = _screenState.value
                    val filtered = applySortAndFilter(items, state.localSortMode, state.localSearchQuery)
                    _screenState.update {
                        it.copy(
                            localState = BackupUiState.Success(
                                items = filtered,
                                storageStatus = storageStatus,
                            ),
                            isRefreshing = false,
                        )
                    }
                }
            } catch (e: Exception) {
                _screenState.update {
                    it.copy(
                        localState = BackupUiState.Error(
                            message = e.message ?: "Failed to load backups",
                            exception = e,
                        ),
                        isRefreshing = false,
                    )
                }
            }
        }
    }

    private fun loadCloudBackups() {
        // Cancel previous job if any
        cloudBackupJob?.cancel()

        if (!cloudRepository.isAuthenticated()) {
            _screenState.update {
                it.copy(
                    cloudState = BackupUiState.RequiresAuth,
                    isRefreshing = false,
                )
            }
            return
        }

        _screenState.update { it.copy(cloudState = BackupUiState.Loading) }

        cloudBackupJob = viewModelScope.launch {
            cloudRepository
                .getCloudBackupItems()
                .catch { e ->
                    _screenState.update {
                        it.copy(
                            cloudState = BackupUiState.Error(
                                message = e.message ?: "Failed to load cloud backups",
                                exception = e,
                            ),
                            isRefreshing = false,
                        )
                    }
                }.collectLatest { items ->
                    val limits = cloudRepository.getBackupLimits()

                    if (items.isEmpty()) {
                        rawCloudItems = emptyList()
                        _screenState.update {
                            it.copy(
                                cloudState = BackupUiState.Empty("No cloud backups found"),
                                isRefreshing = false,
                            )
                        }
                    } else {
                        rawCloudItems = items
                        val state = _screenState.value
                        val filtered = applySortAndFilter(items, state.cloudSortMode, state.cloudSearchQuery)
                        _screenState.update {
                            it.copy(
                                cloudState = BackupUiState.Success(
                                    items = filtered,
                                    backupLimits = limits.copy(currentCount = items.size),
                                ),
                                isRefreshing = false,
                            )
                        }
                    }
                }
        }
    }

    /**
     * Show the "Copy to..." dialog for a backup item.
     */
    fun showCopyToDialog(item: BackupItem) {
        sendEvent(BackupUiEvent.ShowCopyToDialog(item))
    }

    /**
     * Copy a backup to a specific local backup file.
     * Checks for existing backup with the same filename and shows confirmation if needed.
     */
    fun copyBackupToLocalFile(item: BackupItem, targetFile: LocalBackupFile) {
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
                        confirmAction = { executeCopyBackupToLocalFile(item, targetFile) },
                    ),
                )
            } else {
                executeCopyBackupToLocalFile(item, targetFile)
            }
        }
    }

    /**
     * Execute the actual copy of a backup to a local file (after confirmation if needed).
     */
    private fun executeCopyBackupToLocalFile(item: BackupItem, targetFile: LocalBackupFile) {
        viewModelScope.launch {
            _screenState.update {
                it.copy(
                    operationResult = BackupOperationResult.InProgress(
                        message = "Copying backup to ${targetFile.displayName}...",
                    ),
                )
            }

            try {
                // Get backup data from source
                val backupData = if (item.deviceType == DeviceType.CLOUD) {
                    cloudRepository.downloadBackup(item).getOrNull()
                } else {
                    val selectedFile = _screenState.value.selectedLocalFile
                    if (selectedFile != null) {
                        backupRepository.getBackupDataFromFile(selectedFile.getFilePath(), item)
                    } else {
                        backupRepository.getBackupData(item)
                    }
                }

                if (backupData == null) {
                    _screenState.update {
                        it.copy(
                            operationResult = BackupOperationResult.Failure("Failed to read backup data"),
                        )
                    }
                    sendEvent(BackupUiEvent.ShowToast("Failed to read backup data"))
                    return@launch
                }

                // Download and save screenshot from cloud if available
                if (item.deviceType == DeviceType.CLOUD) {
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
                                // Screenshot download failure should not block backup copy
                            }
                        }
                    }
                } else if (item.backupFileKey != targetFile.fileKey) {
                    // Local-to-local copy: copy screenshot to target file directory
                    item.screenshotUrl?.let { path ->
                        try {
                            val sourceFile = File(path)
                            if (sourceFile.exists()) {
                                val targetScreenshot = File(
                                    "${YabauseStorage.storage.screenshotPath}${targetFile.fileKey}/backup_${item.filename}.png",
                                )
                                targetScreenshot.parentFile?.mkdirs()
                                sourceFile.copyTo(targetScreenshot, overwrite = true)
                            }
                        } catch (e: Exception) {
                            // Screenshot copy failure should not block backup copy
                        }
                    }
                }

                val result = backupRepository.copyBackupToFile(item, targetFile, backupData)

                result.fold(
                    onSuccess = {
                        _screenState.update {
                            it.copy(
                                operationResult = BackupOperationResult.Success("Backup copied successfully"),
                            )
                        }
                        sendEvent(BackupUiEvent.ShowToast("Backup copied to ${targetFile.displayName}"))
                        // Reload if the target is the currently selected file
                        if (_screenState.value.selectedLocalFile == targetFile) {
                            loadLocalBackup(targetFile)
                        }
                    },
                    onFailure = { e ->
                        _screenState.update {
                            it.copy(
                                operationResult = BackupOperationResult.Failure(
                                    message = e.message ?: "Failed to copy backup",
                                    exception = e,
                                ),
                            )
                        }
                        sendEvent(BackupUiEvent.ShowToast("Failed to copy backup: ${e.message}"))
                    },
                )
            } catch (e: Exception) {
                _screenState.update {
                    it.copy(
                        operationResult = BackupOperationResult.Failure(
                            message = e.message ?: "Failed to copy backup",
                            exception = e,
                        ),
                    )
                }
                sendEvent(BackupUiEvent.ShowToast("Failed to copy backup: ${e.message}"))
            }
        }
    }

    /**
     * Copy a backup to cloud.
     */
    fun copyBackupToCloud(item: BackupItem) {
        viewModelScope.launch {
            _screenState.update {
                it.copy(
                    operationResult = BackupOperationResult.InProgress(
                        message = "Copying backup to Cloud...",
                    ),
                )
            }
            uploadToCloud(item)
        }
    }

    private suspend fun uploadToCloud(item: BackupItem) {
        if (!cloudRepository.isAuthenticated()) {
            _screenState.update {
                it.copy(operationResult = BackupOperationResult.Failure("Please sign in to use cloud backup"))
            }
            sendEvent(BackupUiEvent.ShowToast("Please sign in to use cloud backup"))
            return
        }

        val selectedFile = _screenState.value.selectedLocalFile
        val backupData = if (selectedFile != null) {
            backupRepository.getBackupDataFromFile(selectedFile.getFilePath(), item)
        } else {
            backupRepository.getBackupData(item)
        }
        if (backupData == null) {
            _screenState.update {
                it.copy(operationResult = BackupOperationResult.Failure("Failed to read backup data"))
            }
            sendEvent(BackupUiEvent.ShowToast("Failed to read backup data"))
            return
        }

        // Read local screenshot file if available
        val screenshotData = item.screenshotUrl?.let { path ->
            if (!path.startsWith("https://")) {
                try {
                    val file = File(path)
                    if (file.exists()) file.readBytes() else null
                } catch (e: Exception) {
                    null
                }
            } else {
                null
            }
        }

        val result = cloudRepository.uploadBackup(backupData, item, screenshotData)

        result.fold(
            onSuccess = {
                _screenState.update {
                    it.copy(operationResult = BackupOperationResult.Success("Backup uploaded to cloud"))
                }
                sendEvent(BackupUiEvent.ShowToast("Backup uploaded to cloud"))
                loadCloudBackups()
            },
            onFailure = { e ->
                _screenState.update {
                    it.copy(
                        operationResult = BackupOperationResult.Failure(
                            message = e.message ?: "Failed to upload backup",
                            exception = e,
                        ),
                    )
                }
                sendEvent(BackupUiEvent.ShowToast("Failed to upload backup: ${e.message}"))
            },
        )
    }

    /**
     * Delete a backup.
     */
    fun deleteBackup(item: BackupItem) {
        viewModelScope.launch {
            _screenState.update {
                it.copy(operationResult = BackupOperationResult.InProgress("Deleting backup..."))
            }

            val result = when (item.deviceType) {
                DeviceType.CLOUD -> cloudRepository.deleteBackup(item)
                else -> {
                    val selectedFile = _screenState.value.selectedLocalFile
                    if (selectedFile != null) {
                        backupRepository.deleteBackupFromFile(selectedFile.getFilePath(), item)
                    } else {
                        backupRepository.deleteBackup(item)
                    }
                }
            }

            result.fold(
                onSuccess = {
                    _screenState.update {
                        it.copy(operationResult = BackupOperationResult.Success("Backup deleted"))
                    }
                    sendEvent(BackupUiEvent.ShowToast("Backup deleted"))
                    // Reload the current local file or cloud
                    when (item.deviceType) {
                        DeviceType.CLOUD -> loadCloudBackups()
                        else -> {
                            val selectedFile = _screenState.value.selectedLocalFile
                            if (selectedFile != null) {
                                loadLocalBackup(selectedFile)
                            }
                        }
                    }
                },
                onFailure = { e ->
                    _screenState.update {
                        it.copy(
                            operationResult = BackupOperationResult.Failure(
                                message = e.message ?: "Failed to delete backup",
                                exception = e,
                            ),
                        )
                    }
                    sendEvent(BackupUiEvent.ShowToast("Failed to delete backup: ${e.message}"))
                },
            )
        }
    }

    /**
     * Request delete confirmation.
     */
    fun requestDelete(item: BackupItem) {
        sendEvent(
            BackupUiEvent.ShowConfirmation(
                title = "Delete Backup",
                message = "Are you sure you want to delete '${item.filename}'?",
                confirmAction = { deleteBackup(item) },
            ),
        )
    }

    /**
     * Export a backup to external storage.
     */
    fun requestExport(item: BackupItem) {
        sendEvent(
            BackupUiEvent.RequestExportPicker(
                item = item,
                suggestedName = "${item.filename}.bup",
            ),
        )
    }

    /**
     * Handle export result from SAF picker.
     */
    fun handleExportResult(item: BackupItem, uri: Uri) {
        viewModelScope.launch {
            _screenState.update {
                it.copy(operationResult = BackupOperationResult.InProgress("Exporting backup..."))
            }

            val selectedFile = _screenState.value.selectedLocalFile
            val result = if (selectedFile != null) {
                backupRepository.exportBackupFromFile(selectedFile.getFilePath(), item, uri)
            } else {
                backupRepository.exportBackup(item, uri)
            }

            result.fold(
                onSuccess = {
                    _screenState.update {
                        it.copy(operationResult = BackupOperationResult.Success("Backup exported"))
                    }
                    sendEvent(BackupUiEvent.ShowToast("Backup exported successfully"))
                },
                onFailure = { e ->
                    _screenState.update {
                        it.copy(
                            operationResult = BackupOperationResult.Failure(
                                message = e.message ?: "Failed to export backup",
                                exception = e,
                            ),
                        )
                    }
                    sendEvent(BackupUiEvent.ShowToast("Failed to export backup: ${e.message}"))
                },
            )
        }
    }

    /**
     * Request sharing a backup publicly.
     */
    fun requestShare(item: BackupItem) {
        if (!cloudRepository.isAuthenticated()) {
            sendEvent(BackupUiEvent.ShowToast("Please sign in to share backups"))
            return
        }
        sendEvent(BackupUiEvent.ShowShareDialog(item))
    }

    /**
     * Share a backup publicly with the provided info.
     */
    fun shareBackup(item: BackupItem, shareInfo: ShareInfo) {
        viewModelScope.launch {
            _screenState.update {
                it.copy(operationResult = BackupOperationResult.InProgress("Sharing backup..."))
            }

            // Get backup data
            val backupData = if (item.deviceType == DeviceType.CLOUD) {
                cloudRepository.downloadBackup(item).getOrNull()
            } else {
                val selectedFile = _screenState.value.selectedLocalFile
                if (selectedFile != null) {
                    backupRepository.getBackupDataFromFile(selectedFile.getFilePath(), item)
                } else {
                    backupRepository.getBackupData(item)
                }
            }

            if (backupData == null) {
                _screenState.update {
                    it.copy(operationResult = BackupOperationResult.Failure("Failed to read backup data"))
                }
                sendEvent(BackupUiEvent.ShowToast("Failed to read backup data"))
                return@launch
            }

            // Read the screenshot so the shared copy gets its own image. Local items
            // have a file path; cloud items have a remote URL we download.
            val screenshotData = item.screenshotUrl?.let { path ->
                if (!path.startsWith("https://")) {
                    try {
                        val file = File(path)
                        if (file.exists()) file.readBytes() else null
                    } catch (e: Exception) {
                        null
                    }
                } else {
                    cloudRepository.downloadScreenshot(path).getOrNull()
                }
            }

            val result = cloudRepository.shareBackup(item, backupData, shareInfo, screenshotData)

            result.fold(
                onSuccess = { backupId ->
                    _screenState.update {
                        it.copy(operationResult = BackupOperationResult.Success("Backup shared successfully"))
                    }
                    sendEvent(
                        BackupUiEvent.LaunchShareSheet(
                            url = BackupShareLink.buildUrl(backupId),
                            gameTitle = shareInfo.gameTitle,
                        ),
                    )
                },
                onFailure = { e ->
                    _screenState.update {
                        it.copy(
                            operationResult = BackupOperationResult.Failure(
                                message = e.message ?: "Failed to share backup",
                                exception = e,
                            ),
                        )
                    }
                    sendEvent(BackupUiEvent.ShowToast("Failed to share backup: ${e.message}"))
                },
            )
        }
    }

    /**
     * Import a backup from shared backups to a local file.
     */
    fun importBackupToFile(item: BackupItem, targetFile: LocalBackupFile) {
        copyBackupToLocalFile(item, targetFile)
    }

    /**
     * Download a cloud backup to local storage.
     */
    fun downloadFromCloud(item: BackupItem, targetDevice: DeviceType) {
        viewModelScope.launch {
            _screenState.update {
                it.copy(operationResult = BackupOperationResult.InProgress("Downloading backup..."))
            }

            val downloadResult = cloudRepository.downloadBackup(item)

            downloadResult.fold(
                onSuccess = { data ->
                    _screenState.update {
                        it.copy(operationResult = BackupOperationResult.Success("Backup downloaded"))
                    }
                    sendEvent(BackupUiEvent.ShowToast("Backup downloaded"))
                },
                onFailure = { e ->
                    _screenState.update {
                        it.copy(
                            operationResult = BackupOperationResult.Failure(
                                message = e.message ?: "Failed to download backup",
                                exception = e,
                            ),
                        )
                    }
                    sendEvent(BackupUiEvent.ShowToast("Failed to download backup: ${e.message}"))
                },
            )
        }
    }

    /**
     * Set the search query for the specified tab and re-filter the list.
     */
    fun setSearchQuery(deviceType: DeviceType, query: String) {
        when (deviceType) {
            DeviceType.INTERNAL, DeviceType.EXTERNAL -> {
                _screenState.update { it.copy(localSearchQuery = query) }
                refilterLocal()
            }
            DeviceType.CLOUD -> {
                _screenState.update { it.copy(cloudSearchQuery = query) }
                refilterCloud()
            }
            DeviceType.SHARED -> { /* Handled by SharedBackupViewModel */ }
        }
    }

    /**
     * Set the sort mode for the specified tab and re-sort the list.
     */
    fun setSortMode(deviceType: DeviceType, mode: BackupSortMode) {
        when (deviceType) {
            DeviceType.INTERNAL, DeviceType.EXTERNAL -> {
                _screenState.update { it.copy(localSortMode = mode) }
                refilterLocal()
            }
            DeviceType.CLOUD -> {
                _screenState.update { it.copy(cloudSortMode = mode) }
                refilterCloud()
            }
            DeviceType.SHARED -> { /* Handled by SharedBackupViewModel */ }
        }
    }

    private fun refilterLocal() {
        val state = _screenState.value
        val localState = state.localState
        if (localState is BackupUiState.Success && rawLocalItems.isNotEmpty()) {
            val filtered = applySortAndFilter(rawLocalItems, state.localSortMode, state.localSearchQuery)
            _screenState.update {
                it.copy(
                    localState = localState.copy(items = filtered),
                )
            }
        }
    }

    private fun refilterCloud() {
        val state = _screenState.value
        val cloudState = state.cloudState
        if (cloudState is BackupUiState.Success && rawCloudItems.isNotEmpty()) {
            val filtered = applySortAndFilter(rawCloudItems, state.cloudSortMode, state.cloudSearchQuery)
            _screenState.update {
                it.copy(
                    cloudState = cloudState.copy(items = filtered),
                )
            }
        }
    }

    /**
     * Apply search filter and sort to a list of backup items.
     * Visible for testing.
     */
    internal fun applySortAndFilter(
        items: List<BackupItem>,
        sortMode: BackupSortMode,
        query: String,
    ): List<BackupItem> {
        val filtered = if (query.isBlank()) {
            items
        } else {
            items.filter {
                it.filename.contains(query, ignoreCase = true) ||
                    it.gameTitle?.contains(query, ignoreCase = true) == true
            }
        }
        return when (sortMode) {
            BackupSortMode.NAME_ASC -> filtered.sortedBy { it.filename.lowercase() }
            BackupSortMode.NAME_DESC -> filtered.sortedByDescending { it.filename.lowercase() }
            BackupSortMode.DATE_DESC -> filtered.sortedByDescending { it.saveDate }
            BackupSortMode.DATE_ASC -> filtered.sortedBy { it.saveDate }
        }
    }

    /**
     * Clear operation result.
     */
    fun clearOperationResult() {
        _screenState.update { it.copy(operationResult = BackupOperationResult.Idle) }
    }

    /**
     * Check if user is authenticated.
     */
    fun isAuthenticated(): Boolean = cloudRepository.isAuthenticated()

    private fun sendEvent(event: BackupUiEvent) {
        viewModelScope.launch {
            _uiEvents.send(event)
        }
    }

    override fun onCleared() {
        super.onCleared()
        localBackupJob?.cancel()
        cloudBackupJob?.cancel()
    }
}
