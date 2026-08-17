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
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.uoyabause.android.backup.model.BackupItem
import org.uoyabause.android.backup.model.DeviceType
import org.uoyabause.android.backup.model.SharedBackupItem

/**
 * Unit tests for BackupUiEvent sealed class
 * Test cases: UT-V16 ~ UT-V25
 */
class BackupUiEventTest {
    // UT-V16: Test BackupUiEvent.ShowToast
    @Test
    fun `ShowToast contains message`() {
        val event = BackupUiEvent.ShowToast(message = "Backup saved")

        assertEquals("Backup saved", event.message)
    }

    // UT-V17: Test BackupUiEvent.ShowSnackbar
    @Test
    fun `ShowSnackbar contains message`() {
        val event = BackupUiEvent.ShowSnackbar(message = "Deleted")

        assertEquals("Deleted", event.message)
        assertNull(event.actionLabel)
        assertNull(event.action)
    }

    @Test
    fun `ShowSnackbar can have action`() {
        var actionCalled = false
        val event = BackupUiEvent.ShowSnackbar(
            message = "Deleted",
            actionLabel = "Undo",
            action = { actionCalled = true },
        )

        assertEquals("Deleted", event.message)
        assertEquals("Undo", event.actionLabel)

        event.action?.invoke()
        assertTrue(actionCalled)
    }

    // UT-V18: Test BackupUiEvent.Navigate
    @Test
    fun `Navigate contains destination`() {
        val event = BackupUiEvent.Navigate(destination = "settings")

        assertEquals("settings", event.destination)
    }

    // UT-V19: Test BackupUiEvent.ShowConfirmation
    @Test
    fun `ShowConfirmation contains title and message`() {
        var confirmed = false
        val event = BackupUiEvent.ShowConfirmation(
            title = "Delete Backup",
            message = "Are you sure?",
            confirmAction = { confirmed = true },
        )

        assertEquals("Delete Backup", event.title)
        assertEquals("Are you sure?", event.message)

        event.confirmAction()
        assertTrue(confirmed)
    }

    // UT-V20: Test BackupUiEvent.ShowShareDialog
    @Test
    fun `ShowShareDialog contains backup item`() {
        val item = BackupItem(id = "1", filename = "save.bin", gameTitle = "Sonic")
        val event = BackupUiEvent.ShowShareDialog(item = item)

        assertEquals("save.bin", event.item.filename)
        assertEquals("Sonic", event.item.gameTitle)
    }

    // UT-V21: Test BackupUiEvent.ShowRatingDialog
    @Test
    fun `ShowRatingDialog contains shared item and current rating`() {
        val item = SharedBackupItem(id = "1", gameTitle = "Sonic")
        val event = BackupUiEvent.ShowRatingDialog(item = item, currentRating = 4)

        assertEquals("Sonic", event.item.gameTitle)
        assertEquals(4, event.currentRating)
    }

    @Test
    fun `ShowRatingDialog can have null current rating`() {
        val item = SharedBackupItem(id = "1", gameTitle = "Sonic")
        val event = BackupUiEvent.ShowRatingDialog(item = item, currentRating = null)

        assertNull(event.currentRating)
    }

    // UT-V22: Test BackupUiEvent.RequestExportPicker
    @Test
    fun `RequestExportPicker contains item and suggested name`() {
        val item = BackupItem(id = "1", filename = "SAVE001")
        val event = BackupUiEvent.RequestExportPicker(item = item, suggestedName = "SAVE001.bin")

        assertEquals("SAVE001", event.item.filename)
        assertEquals("SAVE001.bin", event.suggestedName)
    }

    // UT-V23: Test BackupUiEvent.RequestImportPicker
    @Test
    fun `RequestImportPicker contains target device`() {
        val event = BackupUiEvent.RequestImportPicker(targetDevice = DeviceType.INTERNAL)

        assertEquals(DeviceType.INTERNAL, event.targetDevice)
    }

    @Test
    fun `RequestImportPicker works for different device types`() {
        val internalEvent = BackupUiEvent.RequestImportPicker(targetDevice = DeviceType.INTERNAL)
        val externalEvent = BackupUiEvent.RequestImportPicker(targetDevice = DeviceType.EXTERNAL)

        assertEquals(DeviceType.INTERNAL, internalEvent.targetDevice)
        assertEquals(DeviceType.EXTERNAL, externalEvent.targetDevice)
    }

    // UT-V24: Test BackupUiEvent.DismissDialog
    @Test
    fun `DismissDialog is singleton`() {
        val event1 = BackupUiEvent.DismissDialog
        val event2 = BackupUiEvent.DismissDialog

        assertTrue(event1 === event2)
    }

    // UT-V25: Test type distinguishability
    @Test
    fun `BackupUiEvent types are distinguishable`() {
        val toast: BackupUiEvent = BackupUiEvent.ShowToast("msg")
        val snackbar: BackupUiEvent = BackupUiEvent.ShowSnackbar("msg")
        val navigate: BackupUiEvent = BackupUiEvent.Navigate("dest")
        val confirm: BackupUiEvent = BackupUiEvent.ShowConfirmation("t", "m") {}
        val share: BackupUiEvent = BackupUiEvent.ShowShareDialog(BackupItem())
        val rating: BackupUiEvent = BackupUiEvent.ShowRatingDialog(SharedBackupItem(), null)
        val export: BackupUiEvent = BackupUiEvent.RequestExportPicker(BackupItem(), "name")
        val import: BackupUiEvent = BackupUiEvent.RequestImportPicker(DeviceType.INTERNAL)
        val dismiss: BackupUiEvent = BackupUiEvent.DismissDialog

        assertTrue(toast is BackupUiEvent.ShowToast)
        assertTrue(snackbar is BackupUiEvent.ShowSnackbar)
        assertTrue(navigate is BackupUiEvent.Navigate)
        assertTrue(confirm is BackupUiEvent.ShowConfirmation)
        assertTrue(share is BackupUiEvent.ShowShareDialog)
        assertTrue(rating is BackupUiEvent.ShowRatingDialog)
        assertTrue(export is BackupUiEvent.RequestExportPicker)
        assertTrue(import is BackupUiEvent.RequestImportPicker)
        assertTrue(dismiss is BackupUiEvent.DismissDialog)
    }

    // Test event equality
    @Test
    fun `ShowToast equality works correctly`() {
        val event1 = BackupUiEvent.ShowToast("message")
        val event2 = BackupUiEvent.ShowToast("message")

        assertEquals(event1, event2)
    }

    @Test
    fun `Navigate equality works correctly`() {
        val event1 = BackupUiEvent.Navigate("settings")
        val event2 = BackupUiEvent.Navigate("settings")

        assertEquals(event1, event2)
    }

    // Test event copy
    @Test
    fun `ShowSnackbar copy works correctly`() {
        val original = BackupUiEvent.ShowSnackbar(message = "Original")
        val copied = original.copy(message = "Copied")

        assertEquals("Copied", copied.message)
    }

    // Test with Japanese text
    @Test
    fun `ShowToast handles Japanese text`() {
        val event = BackupUiEvent.ShowToast(message = "バックアップを保存しました")

        assertEquals("バックアップを保存しました", event.message)
    }

    @Test
    fun `ShowConfirmation handles Japanese text`() {
        val event = BackupUiEvent.ShowConfirmation(
            title = "削除確認",
            message = "本当に削除しますか？",
            confirmAction = {},
        )

        assertEquals("削除確認", event.title)
        assertEquals("本当に削除しますか？", event.message)
    }
}
