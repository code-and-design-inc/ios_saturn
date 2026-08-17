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

import android.net.Uri
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class AddGameBottomSheetFragmentTest {
    // UT-BS01: Fragment can be created via newInstance
    @Test
    fun `newInstance creates non-null fragment`() {
        val fragment = AddGameBottomSheetFragment.newInstance()
        assertNotNull(fragment)
    }

    // UT-BS02: TAG constant is set correctly
    @Test
    fun `TAG constant is AddGameBottomSheet`() {
        assertEquals("AddGameBottomSheet", AddGameBottomSheetFragment.TAG)
    }

    // UT-BS03: Listener can be set without exception
    @Test
    fun `setListener does not throw`() {
        val fragment = AddGameBottomSheetFragment.newInstance()
        val listener = object : AddGameBottomSheetFragment.Listener {
            override fun onFileSelected(uri: Uri) {}

            override fun onFolderAdded(path: String) {}

            override fun onFolderRemoved(path: String) {}

            override fun onScanFolder(path: String) {}

            override fun onScanAllFolders() {}
        }
        fragment.setListener(listener)
    }

    // UT-BS04: updateFolderList before onCreateView does not crash
    @Test
    fun `updateFolderList before view created does not crash`() {
        val fragment = AddGameBottomSheetFragment.newInstance()
        fragment.updateFolderList(
            listOf(
                ScanFolderItem(path = "content://path1", fileCount = 5),
            ),
        )
    }

    // UT-BS05: updateFolderList with empty list does not crash
    @Test
    fun `updateFolderList with empty list does not crash`() {
        val fragment = AddGameBottomSheetFragment.newInstance()
        fragment.updateFolderList(emptyList())
    }

    // UT-BS06: Listener interface has all required methods
    @Test
    fun `Listener interface defines all Phase 3 callbacks`() {
        var fileSelectedCalled = false
        var folderAddedCalled = false
        var folderRemovedCalled = false
        var scanFolderCalled = false
        var scanAllCalled = false

        val listener = object : AddGameBottomSheetFragment.Listener {
            override fun onFileSelected(uri: Uri) {
                fileSelectedCalled = true
            }

            override fun onFolderAdded(path: String) {
                folderAddedCalled = true
            }

            override fun onFolderRemoved(path: String) {
                folderRemovedCalled = true
            }

            override fun onScanFolder(path: String) {
                scanFolderCalled = true
            }

            override fun onScanAllFolders() {
                scanAllCalled = true
            }
        }

        listener.onFileSelected(Uri.parse("content://test"))
        listener.onFolderAdded("content://folder")
        listener.onFolderRemoved("content://folder")
        listener.onScanFolder("content://folder")
        listener.onScanAllFolders()

        assertEquals(true, fileSelectedCalled)
        assertEquals(true, folderAddedCalled)
        assertEquals(true, folderRemovedCalled)
        assertEquals(true, scanFolderCalled)
        assertEquals(true, scanAllCalled)
    }
}
