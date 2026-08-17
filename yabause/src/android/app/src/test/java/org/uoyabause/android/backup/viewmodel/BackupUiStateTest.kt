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

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.uoyabause.android.backup.model.BackupItem
import org.uoyabause.android.backup.model.DeviceType
import org.uoyabause.android.backup.model.SharedBackupItem
import org.uoyabause.android.backup.repository.BackupLimits
import org.uoyabause.android.backup.repository.StorageStatus

/**
 * Unit tests for BackupUiState and related classes
 * Test cases: UT-V01 ~ UT-V15
 */
class BackupUiStateTest {
    // ==============================
    // UT-V01 ~ UT-V05: BackupUiState tests
    // ==============================

    // UT-V01: Test BackupUiState.Initial
    @Test
    fun `BackupUiState Initial is singleton`() {
        val state1 = BackupUiState.Initial
        val state2 = BackupUiState.Initial

        assertEquals(state1, state2)
        assertTrue(state1 === state2)
    }

    // UT-V02: Test BackupUiState.Loading
    @Test
    fun `BackupUiState Loading is singleton`() {
        val state1 = BackupUiState.Loading
        val state2 = BackupUiState.Loading

        assertTrue(state1 === state2)
    }

    // UT-V03: Test BackupUiState.Success
    @Test
    fun `BackupUiState Success contains items`() {
        val items = listOf(
            BackupItem(id = "1", filename = "save1.bin"),
            BackupItem(id = "2", filename = "save2.bin"),
        )

        val state = BackupUiState.Success(items = items)

        assertEquals(2, state.items.size)
        assertEquals("save1.bin", state.items[0].filename)
    }

    @Test
    fun `BackupUiState Success can include storage status`() {
        val items = listOf(BackupItem(id = "1", filename = "save.bin"))
        val status = StorageStatus(totalSize = 1000, freeSize = 500, usedSize = 500)

        val state = BackupUiState.Success(items = items, storageStatus = status)

        assertEquals(1000, state.storageStatus?.totalSize)
        assertEquals(500, state.storageStatus?.freeSize)
    }

    @Test
    fun `BackupUiState Success can include backup limits`() {
        val items = listOf(BackupItem(id = "1", filename = "save.bin"))
        val limits = BackupLimits(maxCount = 10, currentCount = 3)

        val state = BackupUiState.Success(items = items, backupLimits = limits)

        assertEquals(10, state.backupLimits?.maxCount)
        assertEquals(3, state.backupLimits?.currentCount)
    }

    // UT-V04: Test BackupUiState.Error
    @Test
    fun `BackupUiState Error contains message`() {
        val state = BackupUiState.Error(message = "Network error")

        assertEquals("Network error", state.message)
        assertNull(state.exception)
    }

    @Test
    fun `BackupUiState Error can contain exception`() {
        val exception = RuntimeException("Test error")
        val state = BackupUiState.Error(message = "Error occurred", exception = exception)

        assertEquals("Error occurred", state.message)
        assertEquals(exception, state.exception)
    }

    // UT-V05: Test BackupUiState.Empty
    @Test
    fun `BackupUiState Empty has default message`() {
        val state = BackupUiState.Empty()

        assertEquals("No backups found", state.message)
    }

    @Test
    fun `BackupUiState Empty can have custom message`() {
        val state = BackupUiState.Empty(message = "No internal backups")

        assertEquals("No internal backups", state.message)
    }

    // UT-V06: Test BackupUiState.RequiresAuth
    @Test
    fun `BackupUiState RequiresAuth is singleton`() {
        val state1 = BackupUiState.RequiresAuth
        val state2 = BackupUiState.RequiresAuth

        assertTrue(state1 === state2)
    }

    // ==============================
    // UT-V07 ~ UT-V09: SharedBackupUiState tests
    // ==============================

    // UT-V07: Test SharedBackupUiState.Success
    @Test
    fun `SharedBackupUiState Success contains shared items`() {
        val items = listOf(
            SharedBackupItem(id = "1", gameTitle = "Sonic"),
            SharedBackupItem(id = "2", gameTitle = "Virtua Fighter"),
        )

        val state = SharedBackupUiState.Success(items = items)

        assertEquals(2, state.items.size)
        assertNull(state.searchQuery)
    }

    @Test
    fun `SharedBackupUiState Success can include search query`() {
        val items = listOf(SharedBackupItem(id = "1", gameTitle = "Sonic"))

        val state = SharedBackupUiState.Success(items = items, searchQuery = "Sonic")

        assertEquals("Sonic", state.searchQuery)
    }

    // UT-V08: Test SharedBackupUiState.Empty
    @Test
    fun `SharedBackupUiState Empty can have search query`() {
        val state = SharedBackupUiState.Empty(searchQuery = "NonExistent")

        assertEquals("NonExistent", state.searchQuery)
    }

    // UT-V09: Test SharedBackupUiState type checking
    @Test
    fun `SharedBackupUiState types are distinguishable`() {
        val initial: SharedBackupUiState = SharedBackupUiState.Initial
        val loading: SharedBackupUiState = SharedBackupUiState.Loading
        val success: SharedBackupUiState = SharedBackupUiState.Success(items = emptyList())
        val error: SharedBackupUiState = SharedBackupUiState.Error(message = "Error")
        val empty: SharedBackupUiState = SharedBackupUiState.Empty()
        val requiresAuth: SharedBackupUiState = SharedBackupUiState.RequiresAuth

        assertTrue(initial is SharedBackupUiState.Initial)
        assertTrue(loading is SharedBackupUiState.Loading)
        assertTrue(success is SharedBackupUiState.Success)
        assertTrue(error is SharedBackupUiState.Error)
        assertTrue(empty is SharedBackupUiState.Empty)
        assertTrue(requiresAuth is SharedBackupUiState.RequiresAuth)
    }

    // ==============================
    // UT-V10 ~ UT-V12: BackupOperationResult tests
    // ==============================

    // UT-V10: Test BackupOperationResult.InProgress
    @Test
    fun `BackupOperationResult InProgress has message`() {
        val result = BackupOperationResult.InProgress(message = "Uploading...")

        assertEquals("Uploading...", result.message)
        assertEquals(-1, result.progress) // Indeterminate by default
    }

    @Test
    fun `BackupOperationResult InProgress can have progress`() {
        val result = BackupOperationResult.InProgress(message = "Uploading...", progress = 50)

        assertEquals(50, result.progress)
    }

    // UT-V11: Test BackupOperationResult.Success
    @Test
    fun `BackupOperationResult Success has message`() {
        val result = BackupOperationResult.Success(message = "Upload complete")

        assertEquals("Upload complete", result.message)
    }

    // UT-V12: Test BackupOperationResult.Failure
    @Test
    fun `BackupOperationResult Failure has message`() {
        val result = BackupOperationResult.Failure(message = "Upload failed")

        assertEquals("Upload failed", result.message)
        assertNull(result.exception)
    }

    @Test
    fun `BackupOperationResult Failure can have exception`() {
        val exception = RuntimeException("Network error")
        val result = BackupOperationResult.Failure(message = "Failed", exception = exception)

        assertEquals(exception, result.exception)
    }

    // UT-V13: Test BackupOperationResult.Idle
    @Test
    fun `BackupOperationResult Idle is singleton`() {
        val result1 = BackupOperationResult.Idle
        val result2 = BackupOperationResult.Idle

        assertTrue(result1 === result2)
    }

    // ==============================
    // UT-V14 ~ UT-V15: BackupManagerScreenState tests
    // ==============================

    // UT-V14: Test BackupManagerScreenState defaults
    @Test
    fun `BackupManagerScreenState has correct defaults`() {
        val state = BackupManagerScreenState()

        assertEquals(DeviceType.INTERNAL, state.currentTab)
        assertTrue(state.localState is BackupUiState.Initial)
        assertTrue(state.cloudState is BackupUiState.Initial)
        assertTrue(state.sharedState is SharedBackupUiState.Initial)
        assertTrue(state.operationResult is BackupOperationResult.Idle)
        assertNull(state.selectedBackup)
        assertFalse(state.isRefreshing)
    }

    // UT-V15: Test BackupManagerScreenState.currentState
    @Test
    fun `currentState returns localState when INTERNAL tab selected`() {
        val localState = BackupUiState.Success(items = listOf(BackupItem(id = "1")))
        val state = BackupManagerScreenState(
            currentTab = DeviceType.INTERNAL,
            localState = localState,
        )

        assertEquals(localState, state.currentState)
    }

    @Test
    fun `currentState returns localState when EXTERNAL tab selected`() {
        val localState = BackupUiState.Success(items = listOf(BackupItem(id = "2")))
        val state = BackupManagerScreenState(
            currentTab = DeviceType.EXTERNAL,
            localState = localState,
        )

        assertEquals(localState, state.currentState)
    }

    @Test
    fun `currentState returns cloudState when CLOUD tab selected`() {
        val cloudState = BackupUiState.RequiresAuth
        val state = BackupManagerScreenState(
            currentTab = DeviceType.CLOUD,
            cloudState = cloudState,
        )

        assertEquals(cloudState, state.currentState)
    }

    @Test
    fun `currentState returns sharedState when SHARED tab selected`() {
        val sharedState = SharedBackupUiState.Success(items = emptyList())
        val state = BackupManagerScreenState(
            currentTab = DeviceType.SHARED,
            sharedState = sharedState,
        )

        assertEquals(sharedState, state.currentState)
    }

    // Test BackupManagerScreenState copy
    @Test
    fun `BackupManagerScreenState copy works correctly`() {
        val original = BackupManagerScreenState()
        val copied = original.copy(
            currentTab = DeviceType.CLOUD,
            isRefreshing = true,
        )

        assertEquals(DeviceType.CLOUD, copied.currentTab)
        assertTrue(copied.isRefreshing)
        // Other fields unchanged
        assertTrue(copied.localState is BackupUiState.Initial)
    }

    // Test SelectedBackup
    @Test
    fun `SelectedBackup contains item and operation`() {
        val item = BackupItem(id = "1", filename = "save.bin")
        val selected = SelectedBackup(item = item, operation = BackupOperation.DELETE)

        assertEquals(item, selected.item)
        assertEquals(BackupOperation.DELETE, selected.operation)
    }

    // Test BackupOperation enum values
    @Test
    fun `BackupOperation has all expected values`() {
        val operations = BackupOperation.values()

        assertEquals(4, operations.size)
        assertTrue(operations.contains(BackupOperation.COPY_TO))
        assertTrue(operations.contains(BackupOperation.DELETE))
        assertTrue(operations.contains(BackupOperation.EXPORT))
        assertTrue(operations.contains(BackupOperation.SHARE))
    }
}
