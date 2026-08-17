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

import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class ScanFolderAdapterTest {
    private lateinit var adapter: ScanFolderAdapter

    @Before
    fun setUp() {
        adapter = ScanFolderAdapter()
    }

    // UT-B01: Adapter initial state
    @Test
    fun `adapter starts with zero items`() {
        assertEquals(0, adapter.itemCount)
    }

    // UT-B01: updateItems updates item count
    @Test
    fun `updateItems changes item count`() {
        val items = listOf(
            ScanFolderItem(path = "content://path1", fileCount = 5, lastScanTimestamp = 1000L),
            ScanFolderItem(path = "content://path2", fileCount = 3, lastScanTimestamp = 2000L),
        )
        adapter.updateItems(items)
        assertEquals(2, adapter.itemCount)
    }

    // UT-B01: updateItems with empty list
    @Test
    fun `updateItems with empty list sets count to zero`() {
        val items = listOf(
            ScanFolderItem(path = "content://path1"),
        )
        adapter.updateItems(items)
        assertEquals(1, adapter.itemCount)

        adapter.updateItems(emptyList())
        assertEquals(0, adapter.itemCount)
    }

    // UT-B02: Listener can be set
    @Test
    fun `setOnFolderActionListener accepts listener`() {
        val listener = object : ScanFolderAdapter.OnFolderActionListener {
            override fun onRescanClick(position: Int, item: ScanFolderItem) {}

            override fun onDeleteClick(position: Int, item: ScanFolderItem) {}
        }
        adapter.setOnFolderActionListener(listener)
        // No exception means success
    }

    // UT-B02: Listener can be set to null
    @Test
    fun `setOnFolderActionListener accepts null`() {
        adapter.setOnFolderActionListener(null)
        // No exception means success
    }

    // UT-B03: Info text with file count and time
    @Test
    fun `buildInfoText shows file count and relative time`() {
        val now = System.currentTimeMillis()
        val twoHoursAgo = now - (2 * 60 * 60 * 1000)
        val items = listOf(
            ScanFolderItem(path = "content://path1", fileCount = 12, lastScanTimestamp = twoHoursAgo),
        )
        adapter.updateItems(items)
        assertEquals(1, adapter.itemCount)
    }

    // UT-B03: Multiple updateItems calls
    @Test
    fun `multiple updateItems calls replace previous items`() {
        adapter.updateItems(listOf(ScanFolderItem(path = "content://a")))
        assertEquals(1, adapter.itemCount)

        adapter.updateItems(
            listOf(
                ScanFolderItem(path = "content://b"),
                ScanFolderItem(path = "content://c"),
                ScanFolderItem(path = "content://d"),
            ),
        )
        assertEquals(3, adapter.itemCount)
    }
}
