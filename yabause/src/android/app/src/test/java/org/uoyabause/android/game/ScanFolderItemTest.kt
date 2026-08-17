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
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class ScanFolderItemTest {
    // UT-001: ScanFolderItem生成
    @Test
    fun `create ScanFolderItem with correct properties`() {
        val item = ScanFolderItem(
            path = "content://com.android.externalstorage.documents/tree/primary%3ASaturn%20Games",
            fileCount = 12,
            lastScanTimestamp = 1738368000000L,
        )
        assertEquals("content://com.android.externalstorage.documents/tree/primary%3ASaturn%20Games", item.path)
        assertEquals(12, item.fileCount)
        assertEquals(1738368000000L, item.lastScanTimestamp)
    }

    // UT-002: ScanFolderItem表示名取得
    @Test
    fun `displayName extracts folder name from content URI`() {
        val item = ScanFolderItem(
            path = "content://com.android.externalstorage.documents/tree/primary%3ASaturn%20Games",
        )
        assertEquals("Saturn Games", item.displayName)
    }

    @Test
    fun `displayName extracts folder name with colon separator`() {
        val item = ScanFolderItem(
            path = "content://com.android.externalstorage.documents/tree/primary:Downloads",
        )
        assertEquals("Downloads", item.displayName)
    }

    @Test
    fun `displayName handles plain file path`() {
        val item = ScanFolderItem(path = "/storage/emulated/0/Saturn Games")
        assertEquals("Saturn Games", item.displayName)
    }

    @Test
    fun `displayName handles blank path`() {
        val item = ScanFolderItem(path = "")
        assertEquals("", item.displayName)
    }

    // UT-003: ScanFolderItem相対時間表示（時間単位）
    @Test
    fun `getRelativeTimeText returns hours ago`() {
        val now = 1738375200000L // base time
        val twoHoursAgo = now - (2 * 60 * 60 * 1000)
        val item = ScanFolderItem(path = "test", lastScanTimestamp = twoHoursAgo)
        assertEquals("2h ago", item.getRelativeTimeText(now))
    }

    @Test
    fun `getRelativeTimeText returns minutes ago`() {
        val now = 1738375200000L
        val thirtyMinAgo = now - (30 * 60 * 1000)
        val item = ScanFolderItem(path = "test", lastScanTimestamp = thirtyMinAgo)
        assertEquals("30m ago", item.getRelativeTimeText(now))
    }

    @Test
    fun `getRelativeTimeText returns just now for very recent`() {
        val now = 1738375200000L
        val tenSecsAgo = now - (10 * 1000)
        val item = ScanFolderItem(path = "test", lastScanTimestamp = tenSecsAgo)
        assertEquals("just now", item.getRelativeTimeText(now))
    }

    // UT-004: ScanFolderItem相対時間表示（日単位）
    @Test
    fun `getRelativeTimeText returns days ago`() {
        val now = 1738375200000L
        val oneDayAgo = now - (24 * 60 * 60 * 1000)
        val item = ScanFolderItem(path = "test", lastScanTimestamp = oneDayAgo)
        assertEquals("1d ago", item.getRelativeTimeText(now))
    }

    @Test
    fun `getRelativeTimeText returns empty for zero timestamp`() {
        val item = ScanFolderItem(path = "test", lastScanTimestamp = 0L)
        assertEquals("", item.getRelativeTimeText())
    }
}
