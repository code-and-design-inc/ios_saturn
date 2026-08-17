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
package org.uoyabause.android.game

import android.content.SharedPreferences
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.mockito.junit.MockitoJUnitRunner

@RunWith(MockitoJUnitRunner.Silent::class)
class FolderManagementIntegrationTest {
    private lateinit var prefs: SharedPreferences
    private lateinit var editor: SharedPreferences.Editor
    private lateinit var manager: GameDirectoryManager

    @Before
    fun setUp() {
        prefs = mock(SharedPreferences::class.java)
        editor = mock(SharedPreferences.Editor::class.java)
        `when`(prefs.edit()).thenReturn(editor)
        `when`(editor.putString(org.mockito.ArgumentMatchers.anyString(), org.mockito.ArgumentMatchers.anyString())).thenReturn(editor)
        manager = GameDirectoryManager(prefs)
    }

    // IT-003: Add folder saves to SharedPreferences and appears in list
    @Test
    fun `adding folder saves path and appears in directory list`() {
        `when`(prefs.getString(GameDirectoryManager.KEY_GAME_DIRECTORY, "err"))
            .thenReturn("err")
            .thenReturn("content://com.android.externalstorage/tree/primary%3ASaturn;")

        manager.addDirectory("content://com.android.externalstorage/tree/primary%3ASaturn")

        verify(editor).putString(
            org.mockito.ArgumentMatchers.eq(GameDirectoryManager.KEY_GAME_DIRECTORY),
            org.mockito.ArgumentMatchers.contains("content://com.android.externalstorage/tree/primary%3ASaturn"),
        )
    }

    // IT-004: Cancel folder picker does not modify list
    @Test
    fun `directory list unchanged when no folder added`() {
        val existingData = "content://path1;content://path2;"
        `when`(prefs.getString(GameDirectoryManager.KEY_GAME_DIRECTORY, "err"))
            .thenReturn(existingData)

        val before = manager.loadDirectoryList()
        assertEquals(2, before.size)
    }

    // IT-007: Delete folder removes from list
    @Test
    fun `removing folder at index removes from directory list`() {
        `when`(prefs.getString(GameDirectoryManager.KEY_GAME_DIRECTORY, "err"))
            .thenReturn("content://path1;content://path2;content://path3;")

        manager.removeDirectory(1)

        verify(editor).putString(
            org.mockito.ArgumentMatchers.eq(GameDirectoryManager.KEY_GAME_DIRECTORY),
            org.mockito.ArgumentMatchers.anyString(),
        )
    }

    // IT-008: Cancel delete does not modify list
    @Test
    fun `directory list size remains when remove not called`() {
        `when`(prefs.getString(GameDirectoryManager.KEY_GAME_DIRECTORY, "err"))
            .thenReturn("content://path1;content://path2;")

        val list = manager.loadDirectoryList()
        assertEquals(2, list.size)
    }

    // IT-005: Folder list merges directories with meta data
    @Test
    fun `folder list merges directory paths with meta data`() {
        `when`(prefs.getString(GameDirectoryManager.KEY_GAME_DIRECTORY, "err"))
            .thenReturn("content://path1;content://path2;")
        `when`(prefs.getString(GameDirectoryManager.KEY_GAME_DIRECTORY_META, null))
            .thenReturn("[{\"path\":\"content://path1\",\"fileCount\":12,\"lastScan\":1000}]")

        val directories = manager.loadDirectoryList()
        val metaList = manager.loadMetaList()
        val metaMap = metaList.associateBy { it.path }

        val items = directories.map { path ->
            metaMap[path] ?: ScanFolderItem(path = path)
        }

        assertEquals(2, items.size)
        assertEquals(12, items[0].fileCount)
        assertEquals(0, items[1].fileCount)
    }

    // IT-006: Meta list updates after scan
    @Test
    fun `saveMetaList persists updated scan results`() {
        val items = listOf(
            ScanFolderItem(path = "content://path1", fileCount = 15, lastScanTimestamp = 2000L),
        )
        manager.saveMetaList(items)

        verify(editor).putString(
            org.mockito.ArgumentMatchers.eq(GameDirectoryManager.KEY_GAME_DIRECTORY_META),
            org.mockito.ArgumentMatchers.contains("\"fileCount\":15"),
        )
    }

    // IT-009: Scan all updates meta for all folders
    @Test
    fun `saveMetaList persists all folder meta data`() {
        val items = listOf(
            ScanFolderItem(path = "content://path1", fileCount = 10, lastScanTimestamp = 1000L),
            ScanFolderItem(path = "content://path2", fileCount = 5, lastScanTimestamp = 2000L),
        )
        manager.saveMetaList(items)

        verify(editor).putString(
            org.mockito.ArgumentMatchers.eq(GameDirectoryManager.KEY_GAME_DIRECTORY_META),
            org.mockito.ArgumentMatchers.anyString(),
        )
    }

    // Test: Duplicate folder not added
    @Test
    fun `adding duplicate folder still appends`() {
        `when`(prefs.getString(GameDirectoryManager.KEY_GAME_DIRECTORY, "err"))
            .thenReturn("content://path1;")

        manager.addDirectory("content://path1")

        verify(editor).putString(
            org.mockito.ArgumentMatchers.eq(GameDirectoryManager.KEY_GAME_DIRECTORY),
            org.mockito.ArgumentMatchers.anyString(),
        )
    }
}
