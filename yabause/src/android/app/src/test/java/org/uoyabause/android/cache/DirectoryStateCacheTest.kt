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
package org.uoyabause.android.cache

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class DirectoryStateCacheTest {
    @get:Rule
    val tempFolder = TemporaryFolder()

    private lateinit var directoryStateCache: DirectoryStateCache

    @Before
    fun setup() {
        directoryStateCache = DirectoryStateCache()
    }

    // T008: Unit test for captureSnapshot()
    @Test
    fun `captureSnapshot returns valid snapshot for directory with files`() {
        val gameDir = tempFolder.newFolder("games")
        File(gameDir, "game1.cue").writeText("dummy content")
        File(gameDir, "game2.iso").writeText("dummy content 2")

        val snapshot = directoryStateCache.captureSnapshot(gameDir.absolutePath)

        assertNotNull(snapshot)
        assertEquals(gameDir.absolutePath, snapshot?.directoryPath)
        assertEquals(2, snapshot?.fileCount)
        assertNotNull(snapshot?.fileListHash)
        assertTrue(snapshot?.capturedAt ?: 0 > 0)
    }

    @Test
    fun `captureSnapshot returns snapshot with zero files for empty directory`() {
        val emptyDir = tempFolder.newFolder("empty")

        val snapshot = directoryStateCache.captureSnapshot(emptyDir.absolutePath)

        assertNotNull(snapshot)
        assertEquals(0, snapshot?.fileCount)
    }

    @Test
    fun `captureSnapshot returns null for non-existent directory`() {
        val snapshot = directoryStateCache.captureSnapshot("/non/existent/path")

        assertEquals(null, snapshot)
    }

    @Test
    fun `calculateFileListHash produces consistent hash for same files`() {
        val entries =
            listOf(
                FileEntry("game1.cue", 100, 1000),
                FileEntry("game2.iso", 200, 2000),
            )

        val hash1 = directoryStateCache.calculateFileListHash(entries)
        val hash2 = directoryStateCache.calculateFileListHash(entries)

        assertEquals(hash1, hash2)
    }

    @Test
    fun `calculateFileListHash produces different hash for different files`() {
        val entries1 = listOf(FileEntry("game1.cue", 100, 1000))
        val entries2 = listOf(FileEntry("game2.cue", 100, 1000))

        val hash1 = directoryStateCache.calculateFileListHash(entries1)
        val hash2 = directoryStateCache.calculateFileListHash(entries2)

        assertNotEquals(hash1, hash2)
    }

    @Test
    fun `calculateFileListHash is order-independent`() {
        val entries1 =
            listOf(
                FileEntry("game1.cue", 100, 1000),
                FileEntry("game2.iso", 200, 2000),
            )
        val entries2 =
            listOf(
                FileEntry("game2.iso", 200, 2000),
                FileEntry("game1.cue", 100, 1000),
            )

        val hash1 = directoryStateCache.calculateFileListHash(entries1)
        val hash2 = directoryStateCache.calculateFileListHash(entries2)

        assertEquals(hash1, hash2)
    }

    // T014: Unit test for hasChanged()
    @Test
    fun `hasChanged returns false when directory unchanged`() {
        val gameDir = tempFolder.newFolder("games")
        File(gameDir, "game1.cue").writeText("content")

        val snapshot = directoryStateCache.captureSnapshot(gameDir.absolutePath)

        val hasChanged = directoryStateCache.hasChanged(gameDir.absolutePath, snapshot!!)

        assertFalse(hasChanged)
    }

    // T015: Unit test for file addition/deletion/modification detection
    @Test
    fun `hasChanged returns true when file added`() {
        val gameDir = tempFolder.newFolder("games")
        File(gameDir, "game1.cue").writeText("content")

        val snapshot = directoryStateCache.captureSnapshot(gameDir.absolutePath)

        // Add a new file
        File(gameDir, "game2.iso").writeText("new content")

        val hasChanged = directoryStateCache.hasChanged(gameDir.absolutePath, snapshot!!)

        assertTrue(hasChanged)
    }

    @Test
    fun `hasChanged returns true when file deleted`() {
        val gameDir = tempFolder.newFolder("games")
        val file1 = File(gameDir, "game1.cue").apply { writeText("content") }
        File(gameDir, "game2.iso").writeText("content2")

        val snapshot = directoryStateCache.captureSnapshot(gameDir.absolutePath)

        // Delete a file
        file1.delete()

        val hasChanged = directoryStateCache.hasChanged(gameDir.absolutePath, snapshot!!)

        assertTrue(hasChanged)
    }

    @Test
    fun `hasChanged returns true when file modified`() {
        val gameDir = tempFolder.newFolder("games")
        val file1 = File(gameDir, "game1.cue").apply { writeText("content") }

        val snapshot = directoryStateCache.captureSnapshot(gameDir.absolutePath)

        // Modify file (change size and timestamp)
        Thread.sleep(100) // Ensure timestamp changes
        file1.writeText("modified content with more data")

        val hasChanged = directoryStateCache.hasChanged(gameDir.absolutePath, snapshot!!)

        assertTrue(hasChanged)
    }

    @Test
    fun `hasChanged returns true when file renamed`() {
        val gameDir = tempFolder.newFolder("games")
        val file1 = File(gameDir, "game1.cue").apply { writeText("content") }

        val snapshot = directoryStateCache.captureSnapshot(gameDir.absolutePath)

        // Rename file
        file1.renameTo(File(gameDir, "game_renamed.cue"))

        val hasChanged = directoryStateCache.hasChanged(gameDir.absolutePath, snapshot!!)

        assertTrue(hasChanged)
    }
}
