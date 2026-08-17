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

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

/**
 * Unit tests for BackupItem data class
 * Test cases: UT-M06 ~ UT-M10
 */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28])
class BackupItemTest {
    // UT-M06: Test displaySize calculation
    @Test
    fun `displaySize returns bytes for small sizes`() {
        val item = BackupItem(dataSize = 500)
        assertEquals("500 B", item.displaySize)
    }

    @Test
    fun `displaySize returns bytes for zero size`() {
        val item = BackupItem(dataSize = 0)
        assertEquals("0 B", item.displaySize)
    }

    @Test
    fun `displaySize returns KB for kilobyte sizes`() {
        val item = BackupItem(dataSize = 1024)
        assertEquals("1.0 KB", item.displaySize)
    }

    @Test
    fun `displaySize returns KB with decimal for larger kilobyte sizes`() {
        val item = BackupItem(dataSize = 2560) // 2.5 KB
        assertEquals("2.5 KB", item.displaySize)
    }

    @Test
    fun `displaySize returns MB for megabyte sizes`() {
        val item = BackupItem(dataSize = 1024 * 1024)
        assertEquals("1.0 MB", item.displaySize)
    }

    @Test
    fun `displaySize returns MB with decimal for larger sizes`() {
        val item = BackupItem(dataSize = (1.5 * 1024 * 1024).toInt())
        assertEquals("1.5 MB", item.displaySize)
    }

    // UT-M07: Test displayBlocks
    @Test
    fun `displayBlocks returns correct format`() {
        val item = BackupItem(blockSize = 10)
        assertEquals("10 blocks", item.displayBlocks)
    }

    @Test
    fun `displayBlocks returns zero blocks`() {
        val item = BackupItem(blockSize = 0)
        assertEquals("0 blocks", item.displayBlocks)
    }

    // UT-M08: Test isCloudBackup property
    @Test
    fun `isCloudBackup returns true for CLOUD deviceType`() {
        val item = BackupItem(deviceType = DeviceType.CLOUD)
        assertTrue(item.isCloudBackup)
    }

    @Test
    fun `isCloudBackup returns true when downloadUrl is present`() {
        val item = BackupItem(
            deviceType = DeviceType.INTERNAL,
            downloadUrl = "https://example.com/backup.bin",
        )
        assertTrue(item.isCloudBackup)
    }

    @Test
    fun `isCloudBackup returns false for local backup without downloadUrl`() {
        val item = BackupItem(deviceType = DeviceType.INTERNAL)
        assertFalse(item.isCloudBackup)
    }

    @Test
    fun `isCloudBackup returns false for external backup without downloadUrl`() {
        val item = BackupItem(deviceType = DeviceType.EXTERNAL)
        assertFalse(item.isCloudBackup)
    }

    // UT-M09: Test fromJson parsing
    @Test
    fun `fromJson parses basic fields correctly`() {
        val json = JSONObject().apply {
            put("filename", "") // Empty, not base64 encoded
            put("comment", "")
            put("language", 1)
            put("savedate", "2024/01/15")
            put("datasize", 1024)
            put("blocksize", 5)
        }

        val item = BackupItem.fromJson(0, json, DeviceType.INTERNAL)

        assertEquals("0", item.id)
        assertEquals("", item.filename)
        assertEquals("", item.comment)
        assertEquals(1, item.language)
        assertEquals("2024/01/15", item.saveDate)
        assertEquals(1024, item.dataSize)
        assertEquals(5, item.blockSize)
        assertEquals(DeviceType.INTERNAL, item.deviceType)
    }

    @Test
    fun `fromJson parses base64 encoded filename`() {
        // "TEST" in MS932/Shift-JIS, Base64 encoded
        val base64Encoded = android.util.Base64.encodeToString(
            "TEST".toByteArray(charset("MS932")),
            android.util.Base64.DEFAULT,
        )
        val json = JSONObject().apply {
            put("filename", base64Encoded)
            put("comment", "")
            put("language", 0)
            put("savedate", "")
            put("datasize", 0)
            put("blocksize", 0)
        }

        val item = BackupItem.fromJson(1, json, DeviceType.EXTERNAL)

        assertEquals("TEST", item.filename)
    }

    @Test
    fun `fromJson handles missing fields with defaults`() {
        val json = JSONObject()

        val item = BackupItem.fromJson(2, json, DeviceType.INTERNAL)

        assertEquals("2", item.id)
        assertEquals("", item.filename)
        assertEquals("", item.comment)
        assertEquals(0, item.language)
        assertEquals("", item.saveDate)
        assertEquals(0, item.dataSize)
        assertEquals(0, item.blockSize)
    }

    // UT-M10: Test data class equality
    @Test
    fun `BackupItem equality works correctly`() {
        val item1 = BackupItem(
            id = "1",
            filename = "save.bin",
            comment = "Test save",
            dataSize = 1024,
        )
        val item2 = BackupItem(
            id = "1",
            filename = "save.bin",
            comment = "Test save",
            dataSize = 1024,
        )

        assertEquals(item1, item2)
    }

    @Test
    fun `BackupItem copy works correctly`() {
        val original = BackupItem(id = "1", filename = "save.bin")
        val copied = original.copy(filename = "new_save.bin")

        assertEquals("1", copied.id)
        assertEquals("new_save.bin", copied.filename)
    }
}
