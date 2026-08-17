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
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.mockito.junit.MockitoJUnitRunner

@RunWith(MockitoJUnitRunner.Silent::class)
class GameDirectoryManagerTest {
    @Mock
    private lateinit var mockPrefs: SharedPreferences

    @Mock
    private lateinit var mockEditor: SharedPreferences.Editor

    private lateinit var manager: GameDirectoryManager

    @Before
    fun setUp() {
        `when`(mockPrefs.edit()).thenReturn(mockEditor)
        `when`(mockEditor.putString(org.mockito.ArgumentMatchers.anyString(), org.mockito.ArgumentMatchers.anyString())).thenReturn(mockEditor)
        manager = GameDirectoryManager(mockPrefs, defaultGamePath = "/default/games")
    }

    // UT-005: ディレクトリリスト読み込み
    @Test
    fun `loadDirectoryList parses semicolon separated paths`() {
        `when`(mockPrefs.getString(GameDirectoryManager.KEY_GAME_DIRECTORY, "err"))
            .thenReturn("content://path1;content://path2;")
        val result = manager.loadDirectoryList()
        assertEquals(2, result.size)
        assertEquals("content://path1", result[0])
        assertEquals("content://path2", result[1])
    }

    // UT-006: ディレクトリリスト保存
    @Test
    fun `saveDirectoryList saves semicolon separated paths`() {
        val dirs = listOf("content://path1", "content://path2")
        manager.saveDirectoryList(dirs)
        verify(mockEditor).putString(
            GameDirectoryManager.KEY_GAME_DIRECTORY,
            "content://path1;content://path2;",
        )
        verify(mockEditor).apply()
    }

    // UT-007: ディレクトリ追加
    @Test
    fun `addDirectory appends to existing list`() {
        `when`(mockPrefs.getString(GameDirectoryManager.KEY_GAME_DIRECTORY, "err"))
            .thenReturn("content://path1;")
        manager.addDirectory("content://path2")
        verify(mockEditor).putString(
            GameDirectoryManager.KEY_GAME_DIRECTORY,
            "content://path1;content://path2;",
        )
    }

    // UT-008: ディレクトリ削除
    @Test
    fun `removeDirectory removes by index`() {
        `when`(mockPrefs.getString(GameDirectoryManager.KEY_GAME_DIRECTORY, "err"))
            .thenReturn("content://path1;content://path2;content://path3;")
        manager.removeDirectory(1)
        verify(mockEditor).putString(
            GameDirectoryManager.KEY_GAME_DIRECTORY,
            "content://path1;content://path3;",
        )
    }

    // UT-009: メタ情報読み込み
    @Test
    fun `loadMetaList parses JSON array`() {
        val json = """[{"path":"content://path1","fileCount":12,"lastScan":1738368000000},{"path":"content://path2","fileCount":3,"lastScan":1738281600000}]"""
        `when`(mockPrefs.getString(GameDirectoryManager.KEY_GAME_DIRECTORY_META, null))
            .thenReturn(json)
        val result = manager.loadMetaList()
        assertEquals(2, result.size)
        assertEquals("content://path1", result[0].path)
        assertEquals(12, result[0].fileCount)
        assertEquals(1738368000000L, result[0].lastScanTimestamp)
        assertEquals("content://path2", result[1].path)
        assertEquals(3, result[1].fileCount)
    }

    // UT-010: メタ情報保存
    @Test
    fun `saveMetaList writes JSON array`() {
        val items = listOf(
            ScanFolderItem("content://path1", 12, 1738368000000L),
            ScanFolderItem("content://path2", 3, 1738281600000L),
        )
        manager.saveMetaList(items)
        verify(mockEditor).putString(
            org.mockito.ArgumentMatchers.eq(GameDirectoryManager.KEY_GAME_DIRECTORY_META),
            org.mockito.ArgumentMatchers.anyString(),
        )
        verify(mockEditor).apply()
    }

    // UT-E01: 空のディレクトリリスト読み込み（"err"）
    @Test
    fun `loadDirectoryList returns default path when data is err`() {
        `when`(mockPrefs.getString(GameDirectoryManager.KEY_GAME_DIRECTORY, "err"))
            .thenReturn("err")
        val result = manager.loadDirectoryList()
        assertEquals(1, result.size)
        assertEquals("/default/games", result[0])
    }

    // UT-E02: 空文字のディレクトリリスト
    @Test
    fun `loadDirectoryList returns empty list for blank string`() {
        `when`(mockPrefs.getString(GameDirectoryManager.KEY_GAME_DIRECTORY, "err"))
            .thenReturn("")
        val result = manager.loadDirectoryList()
        assertTrue(result.isEmpty())
    }

    // UT-E03: 不正なメタ情報JSON
    @Test
    fun `loadMetaList returns empty list for invalid JSON`() {
        `when`(mockPrefs.getString(GameDirectoryManager.KEY_GAME_DIRECTORY_META, null))
            .thenReturn("{broken json")
        val result = manager.loadMetaList()
        assertTrue(result.isEmpty())
    }

    @Test
    fun `loadMetaList returns empty list when no meta data exists`() {
        `when`(mockPrefs.getString(GameDirectoryManager.KEY_GAME_DIRECTORY_META, null))
            .thenReturn(null)
        val result = manager.loadMetaList()
        assertTrue(result.isEmpty())
    }

    @Test
    fun `removeDirectory does nothing for out of bounds index`() {
        `when`(mockPrefs.getString(GameDirectoryManager.KEY_GAME_DIRECTORY, "err"))
            .thenReturn("content://path1;")
        manager.removeDirectory(5)
        // No putString call should be made for invalid index
    }

    @Test
    fun `saveDirectoryList with empty list saves empty string`() {
        manager.saveDirectoryList(emptyList())
        verify(mockEditor).putString(GameDirectoryManager.KEY_GAME_DIRECTORY, "")
        verify(mockEditor).apply()
    }
}
