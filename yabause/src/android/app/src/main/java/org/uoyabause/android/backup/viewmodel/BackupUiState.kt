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

import org.uoyabause.android.backup.model.BackupItem
import org.uoyabause.android.backup.model.BackupSortMode
import org.uoyabause.android.backup.model.DeviceType
import org.uoyabause.android.backup.model.LocalBackupFile
import org.uoyabause.android.backup.model.SharedBackupItem
import org.uoyabause.android.backup.repository.BackupLimits
import org.uoyabause.android.backup.repository.StorageStatus

/**
 * Sealed class representing the UI state for the backup manager.
 */
sealed class BackupUiState {
    /**
     * Initial state before any data is loaded.
     */
    object Initial : BackupUiState()

    /**
     * Loading state while fetching data.
     */
    object Loading : BackupUiState()

    /**
     * Success state with backup data.
     */
    data class Success(
        val items: List<BackupItem>,
        val storageStatus: StorageStatus? = null,
        val backupLimits: BackupLimits? = null,
    ) : BackupUiState()

    /**
     * Error state when something goes wrong.
     */
    data class Error(
        val message: String,
        val exception: Throwable? = null,
    ) : BackupUiState()

    /**
     * Empty state when no backups are found.
     */
    data class Empty(
        val message: String = "No backups found",
        val storageStatus: StorageStatus? = null,
    ) : BackupUiState()

    /**
     * State requiring authentication.
     */
    object RequiresAuth : BackupUiState()
}

/**
 * Sealed class representing the UI state for shared backups.
 */
sealed class SharedBackupUiState {
    /**
     * Initial state.
     */
    object Initial : SharedBackupUiState()

    /**
     * Loading state.
     */
    object Loading : SharedBackupUiState()

    /**
     * Success state with shared backup data.
     */
    data class Success(
        val items: List<SharedBackupItem>,
        val searchQuery: String? = null,
    ) : SharedBackupUiState()

    /**
     * Error state.
     */
    data class Error(
        val message: String,
        val exception: Throwable? = null,
    ) : SharedBackupUiState()

    /**
     * Empty search results.
     */
    data class Empty(
        val searchQuery: String? = null,
    ) : SharedBackupUiState()

    /**
     * Requires authentication to access shared features.
     */
    object RequiresAuth : SharedBackupUiState()
}

/**
 * Sealed class representing backup operation results.
 */
sealed class BackupOperationResult {
    /**
     * Operation in progress.
     */
    data class InProgress(
        val message: String,
        /** -1 for indeterminate progress */
        val progress: Int = -1,
    ) : BackupOperationResult()

    /**
     * Operation succeeded.
     */
    data class Success(
        val message: String,
    ) : BackupOperationResult()

    /**
     * Operation failed.
     */
    data class Failure(
        val message: String,
        val exception: Throwable? = null,
    ) : BackupOperationResult()

    /**
     * No operation in progress.
     */
    object Idle : BackupOperationResult()
}

/**
 * Data class representing the selected backup for operations.
 */
data class SelectedBackup(
    val item: BackupItem,
    val operation: BackupOperation,
)

/**
 * Enum representing available backup operations.
 */
enum class BackupOperation {
    COPY_TO,
    DELETE,
    EXPORT,
    SHARE,
}

/**
 * Data class for backup manager screen state.
 */
data class BackupManagerScreenState(
    val currentTab: DeviceType = DeviceType.INTERNAL,
    val localState: BackupUiState = BackupUiState.Initial,
    val cloudState: BackupUiState = BackupUiState.Initial,
    val sharedState: SharedBackupUiState = SharedBackupUiState.Initial,
    val operationResult: BackupOperationResult = BackupOperationResult.Idle,
    val selectedBackup: SelectedBackup? = null,
    val isRefreshing: Boolean = false,
    val selectedLocalFile: LocalBackupFile? = null,
    val availableLocalFiles: List<LocalBackupFile> = emptyList(),
    val localSearchQuery: String = "",
    val cloudSearchQuery: String = "",
    val localSortMode: BackupSortMode = BackupSortMode.DATE_DESC,
    val cloudSortMode: BackupSortMode = BackupSortMode.DATE_DESC,
) {
    /**
     * Get the current state based on selected tab.
     */
    val currentState: Any
        get() = when (currentTab) {
            DeviceType.INTERNAL -> localState
            DeviceType.EXTERNAL -> localState
            DeviceType.CLOUD -> cloudState
            DeviceType.SHARED -> sharedState
        }
}

/**
 * Events that can be emitted from the ViewModel to the UI.
 */
sealed class BackupUiEvent {
    /**
     * Show a toast message.
     */
    data class ShowToast(
        val message: String,
    ) : BackupUiEvent()

    /**
     * Show a snackbar message.
     */
    data class ShowSnackbar(
        val message: String,
        val actionLabel: String? = null,
        val action: (() -> Unit)? = null,
    ) : BackupUiEvent()

    /**
     * Navigate to a screen.
     */
    data class Navigate(
        val destination: String,
    ) : BackupUiEvent()

    /**
     * Show confirmation dialog.
     */
    data class ShowConfirmation(
        val title: String,
        val message: String,
        val confirmAction: () -> Unit,
    ) : BackupUiEvent()

    /**
     * Show share dialog for backup sharing.
     */
    data class ShowShareDialog(
        val item: BackupItem,
    ) : BackupUiEvent()

    /**
     * Launch the system share sheet (ACTION_SEND) with a backup share link.
     */
    data class LaunchShareSheet(
        val url: String,
        val gameTitle: String,
    ) : BackupUiEvent()

    /**
     * Show rating dialog.
     */
    data class ShowRatingDialog(
        val item: SharedBackupItem,
        val currentRating: Int?,
    ) : BackupUiEvent()

    /**
     * Request file picker for export.
     */
    data class RequestExportPicker(
        val item: BackupItem,
        val suggestedName: String,
    ) : BackupUiEvent()

    /**
     * Request file picker for import.
     */
    data class RequestImportPicker(
        val targetDevice: DeviceType,
    ) : BackupUiEvent()

    /**
     * Show copy-to destination dialog.
     */
    data class ShowCopyToDialog(
        val item: BackupItem,
    ) : BackupUiEvent()

    /**
     * Dismiss any active dialog.
     */
    object DismissDialog : BackupUiEvent()
}
