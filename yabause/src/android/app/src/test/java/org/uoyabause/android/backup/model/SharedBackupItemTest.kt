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
package org.uoyabause.android.backup.model

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.Date

/**
 * Unit tests for SharedBackupItem data class
 * Test cases: UT-M11 ~ UT-M15
 */
class SharedBackupItemTest {
    // UT-M11: Test displaySize calculation
    @Test
    fun `displaySize returns bytes for small sizes`() {
        val item = SharedBackupItem(dataSize = 500)
        assertEquals("500 B", item.displaySize)
    }

    @Test
    fun `displaySize returns bytes for zero size`() {
        val item = SharedBackupItem(dataSize = 0)
        assertEquals("0 B", item.displaySize)
    }

    @Test
    fun `displaySize returns KB for kilobyte sizes`() {
        val item = SharedBackupItem(dataSize = 1024)
        assertEquals("1.0 KB", item.displaySize)
    }

    @Test
    fun `displaySize returns MB for megabyte sizes`() {
        val item = SharedBackupItem(dataSize = 1024 * 1024)
        assertEquals("1.0 MB", item.displaySize)
    }

    // UT-M12: Test displaySharedDate
    @Test
    fun `displaySharedDate returns formatted date when sharedAt is set`() {
        // Create a specific date: 2024/03/15
        @Suppress("DEPRECATION")
        val date = Date(124, 2, 15) // Year 2024, March, 15th
        val item = SharedBackupItem(sharedAt = date)
        assertEquals("2024/03/15", item.displaySharedDate)
    }

    @Test
    fun `displaySharedDate returns empty string when sharedAt is null`() {
        val item = SharedBackupItem(sharedAt = null)
        assertEquals("", item.displaySharedDate)
    }

    // UT-M13: Test displayRating
    @Test
    fun `displayRating returns rating with count when ratings exist`() {
        val item = SharedBackupItem(averageRating = 4.5f, ratingCount = 10)
        assertEquals("4.5 (10)", item.displayRating)
    }

    @Test
    fun `displayRating returns no ratings when count is zero`() {
        val item = SharedBackupItem(averageRating = 0f, ratingCount = 0)
        assertEquals("No ratings", item.displayRating)
    }

    @Test
    fun `displayRating returns no ratings when count is zero even with non-zero average`() {
        val item = SharedBackupItem(averageRating = 3.0f, ratingCount = 0)
        assertEquals("No ratings", item.displayRating)
    }

    // UT-M14: Test isOwnedBy
    @Test
    fun `isOwnedBy returns true when uid matches ownerId`() {
        val item = SharedBackupItem(ownerId = "user123")
        assertTrue(item.isOwnedBy("user123"))
    }

    @Test
    fun `isOwnedBy returns false when uid does not match ownerId`() {
        val item = SharedBackupItem(ownerId = "user123")
        assertFalse(item.isOwnedBy("user456"))
    }

    @Test
    fun `isOwnedBy returns false when uid is null`() {
        val item = SharedBackupItem(ownerId = "user123")
        assertFalse(item.isOwnedBy(null))
    }

    @Test
    fun `isOwnedBy returns false when uid is empty string`() {
        val item = SharedBackupItem(ownerId = "user123")
        assertFalse(item.isOwnedBy(""))
    }

    // UT-M15: Test toMap
    @Test
    fun `toMap contains all required fields`() {
        val item = SharedBackupItem(
            ownerId = "user123",
            ownerName = "Test User",
            ownerPhotoUrl = "https://example.com/photo.jpg",
            gameTitle = "Sonic",
            productNumber = "T-12345",
            filename = "save.bin",
            comment = "100% completion",
            saveDate = "2024/01/15",
            downloadUrl = "https://storage.example.com/save.bin",
            downloadCount = 5,
            isPublic = true,
            averageRating = 4.5f,
            ratingCount = 10,
            dataSize = 1024,
            blockSize = 5,
            screenshotUrl = "https://example.com/screenshot.png",
        )

        val map = item.toMap()

        assertEquals("user123", map["ownerId"])
        assertEquals("Test User", map["ownerName"])
        assertEquals("https://example.com/photo.jpg", map["ownerPhotoUrl"])
        assertEquals("Sonic", map["gameTitle"])
        assertEquals("T-12345", map["productNumber"])
        assertEquals("save.bin", map["filename"])
        assertEquals("100% completion", map["comment"])
        assertEquals("2024/01/15", map["saveDate"])
        assertEquals("https://storage.example.com/save.bin", map["downloadUrl"])
        assertEquals(5, map["downloadCount"])
        assertEquals(true, map["isPublic"])
        assertEquals(1024, map["dataSize"])
        assertEquals(5, map["blockSize"])
        assertEquals("https://example.com/screenshot.png", map["screenshotUrl"])

        // Verify ratings map
        @Suppress("UNCHECKED_CAST")
        val ratings = map["ratings"] as Map<String, Any>
        assertEquals(4.5f, ratings["average"])
        assertEquals(10, ratings["count"])

        // Verify sharedAt is a Timestamp (not null)
        assertNotNull(map["sharedAt"])
    }

    @Test
    fun `toMap handles null optional fields`() {
        val item = SharedBackupItem(
            ownerId = "user123",
            ownerName = "Test User",
            ownerPhotoUrl = null,
            screenshotUrl = null,
        )

        val map = item.toMap()

        assertNull(map["ownerPhotoUrl"])
        assertNull(map["screenshotUrl"])
    }

    // Test fromBackupItem factory method
    @Test
    fun `fromBackupItem creates SharedBackupItem correctly`() {
        val backup = BackupItem(
            id = "1",
            filename = "save.bin",
            comment = "Test save",
            saveDate = "2024/01/15",
            dataSize = 1024,
            blockSize = 5,
            gameTitle = "Sonic",
            productNumber = "T-12345",
            screenshotUrl = "https://example.com/screenshot.png",
        )

        val shared = SharedBackupItem.fromBackupItem(
            backup = backup,
            ownerId = "user123",
            ownerName = "Test User",
            ownerPhotoUrl = "https://example.com/photo.jpg",
            downloadUrl = "https://storage.example.com/save.bin",
        )

        assertEquals("user123", shared.ownerId)
        assertEquals("Test User", shared.ownerName)
        assertEquals("https://example.com/photo.jpg", shared.ownerPhotoUrl)
        assertEquals("Sonic", shared.gameTitle)
        assertEquals("T-12345", shared.productNumber)
        assertEquals("save.bin", shared.filename)
        assertEquals("Test save", shared.comment)
        assertEquals("2024/01/15", shared.saveDate)
        assertEquals("https://storage.example.com/save.bin", shared.downloadUrl)
        assertEquals(0, shared.downloadCount)
        assertTrue(shared.isPublic)
        assertEquals(0f, shared.averageRating, 0.001f)
        assertEquals(0, shared.ratingCount)
        assertEquals(1024, shared.dataSize)
        assertEquals(5, shared.blockSize)
        assertEquals("https://example.com/screenshot.png", shared.screenshotUrl)
        assertNotNull(shared.sharedAt)
    }

    @Test
    fun `fromBackupItem handles null optional fields in BackupItem`() {
        val backup = BackupItem(
            id = "1",
            filename = "save.bin",
            gameTitle = null,
            productNumber = null,
            screenshotUrl = null,
        )

        val shared = SharedBackupItem.fromBackupItem(
            backup = backup,
            ownerId = "user123",
            ownerName = "Test User",
            ownerPhotoUrl = null,
            downloadUrl = "https://storage.example.com/save.bin",
        )

        assertEquals("", shared.gameTitle)
        assertEquals("", shared.productNumber)
        assertNull(shared.ownerPhotoUrl)
        assertNull(shared.screenshotUrl)
    }

    // Test data class equality
    @Test
    fun `SharedBackupItem equality works correctly`() {
        val item1 = SharedBackupItem(
            id = "1",
            ownerId = "user123",
            filename = "save.bin",
        )
        val item2 = SharedBackupItem(
            id = "1",
            ownerId = "user123",
            filename = "save.bin",
        )

        assertEquals(item1, item2)
    }
}
