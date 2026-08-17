package org.uoyabause.android.backup.viewmodel

import org.uoyabause.android.backup.model.BackupItem
import org.uoyabause.android.backup.model.BackupSortMode

/**
 * Testable wrapper that exposes applySortAndFilter() without requiring AndroidViewModel.
 * This avoids the need for Robolectric/Application context in pure logic tests.
 */
class TestableBackupManagerViewModel {
    /**
     * Apply search filter and sort to a list of backup items.
     * Same logic as BackupManagerViewModel.applySortAndFilter().
     */
    fun applySortAndFilter(
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
}
