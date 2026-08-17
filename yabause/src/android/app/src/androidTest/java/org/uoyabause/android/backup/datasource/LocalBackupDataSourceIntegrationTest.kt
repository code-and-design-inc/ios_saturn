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
package org.uoyabause.android.backup.datasource

import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.uoyabause.android.backup.model.DeviceType
import java.io.File

/**
 * Integration tests for LocalBackupDataSource
 * Test cases: IT-D01 ~ IT-D10
 *
 * These tests verify the DataSource works correctly with real backup files,
 * including interleaved format files from Yabause internal memory.
 */
@RunWith(AndroidJUnit4::class)
class LocalBackupDataSourceIntegrationTest {
    private lateinit var context: Context
    private lateinit var dataSource: LocalBackupDataSource
    private lateinit var testFilesDir: File

    @Before
    fun setUp() {
        context = InstrumentationRegistry.getInstrumentation().targetContext
        dataSource = LocalBackupDataSource(context)
        testFilesDir = context.filesDir
    }

    // ==============================
    // IT-D01 ~ IT-D03: Standard format tests
    // ==============================

    // IT-D01: Parse standard format backup file
    @Test
    fun parseStandardFormatBackupFile() = runBlocking {
        // Create a standard format backup file for testing
        val testFile = createStandardBackupFile("standard_test.ram")

        val items = dataSource.parseBackupFile(testFile.absolutePath, DeviceType.INTERNAL)

        assertEquals(1, items.size)
        assertEquals("TESTSAVE", items[0].filename)
    }

    // IT-D02: Get storage status from standard format
    @Test
    fun getStorageStatusFromStandardFormat() = runBlocking {
        val testFile = createStandardBackupFile("standard_storage.ram")

        val (totalSize, freeSize) = dataSource.getStorageStatusFromFile(testFile.absolutePath)

        assertTrue(totalSize > 0)
        assertTrue(freeSize >= 0)
        assertTrue(freeSize <= totalSize)
    }

    // IT-D03: Handle empty standard format file
    @Test
    fun handleEmptyStandardFormatFile() = runBlocking {
        val testFile = createEmptyBackupFile("empty_standard.ram")

        val items = dataSource.parseBackupFile(testFile.absolutePath, DeviceType.INTERNAL)

        assertTrue(items.isEmpty())
    }

    // ==============================
    // IT-D04 ~ IT-D06: Interleaved format tests
    // ==============================

    // IT-D04: Parse interleaved format backup file
    @Test
    fun parseInterleavedFormatBackupFile() = runBlocking {
        val testFile = createInterleavedBackupFile("interleaved_test.ram")

        val items = dataSource.parseBackupFile(testFile.absolutePath, DeviceType.INTERNAL)

        assertEquals(1, items.size)
        assertEquals("TESTSAVE", items[0].filename)
    }

    // IT-D05: Get storage status from interleaved format
    @Test
    fun getStorageStatusFromInterleavedFormat() = runBlocking {
        val testFile = createInterleavedBackupFile("interleaved_storage.ram")

        val (totalSize, freeSize) = dataSource.getStorageStatusFromFile(testFile.absolutePath)

        // For interleaved format, totalSize should be the logical size (after de-interleaving)
        assertTrue(totalSize > 0)
        assertTrue(freeSize >= 0)
    }

    // IT-D06: Interleaved format detection
    @Test
    fun detectInterleavedFormat() {
        val standardFile = createStandardBackupFile("detect_standard.ram")
        val interleavedFile = createInterleavedBackupFile("detect_interleaved.ram")

        assertFalse(LocalBackupDataSource.isInterleaved(standardFile))
        assertTrue(LocalBackupDataSource.isInterleaved(interleavedFile))
    }

    // ==============================
    // IT-D07 ~ IT-D08: Error handling tests
    // ==============================

    // IT-D07: Handle non-existent file
    @Test
    fun handleNonExistentFile() = runBlocking {
        val items = dataSource.parseBackupFile("/nonexistent/path/backup.ram", DeviceType.INTERNAL)

        assertTrue(items.isEmpty())
    }

    // IT-D08: Handle corrupted file
    @Test
    fun handleCorruptedFile() = runBlocking {
        val corruptedFile = File(testFilesDir, "corrupted.ram")
        corruptedFile.writeBytes(byteArrayOf(0x00, 0x01, 0x02, 0x03))

        val items = dataSource.parseBackupFile(corruptedFile.absolutePath, DeviceType.INTERNAL)

        assertTrue(items.isEmpty())
    }

    // ==============================
    // IT-D09 ~ IT-D10: Multiple entries tests
    // ==============================

    // IT-D09: Parse file with multiple backup entries
    @Test
    fun parseMultipleEntriesStandardFormat() = runBlocking {
        val testFile = createMultiEntryBackupFile("multi_standard.ram", interleaved = false)

        val items = dataSource.parseBackupFile(testFile.absolutePath, DeviceType.INTERNAL)

        assertEquals(3, items.size)
        assertEquals("SAVE001", items[0].filename)
        assertEquals("SAVE002", items[1].filename)
        assertEquals("SAVE003", items[2].filename)
    }

    // IT-D10: Parse interleaved file with multiple entries
    @Test
    fun parseMultipleEntriesInterleavedFormat() = runBlocking {
        val testFile = createMultiEntryBackupFile("multi_interleaved.ram", interleaved = true)

        val items = dataSource.parseBackupFile(testFile.absolutePath, DeviceType.INTERNAL)

        assertEquals(3, items.size)
        assertEquals("SAVE001", items[0].filename)
        assertEquals("SAVE002", items[1].filename)
        assertEquals("SAVE003", items[2].filename)
    }

    // ==============================
    // Helper methods
    // ==============================

    private fun createStandardBackupFile(filename: String): File {
        val file = File(testFilesDir, filename)
        val data = createBackupData(listOf("TESTSAVE"))
        file.writeBytes(data)
        return file
    }

    private fun createInterleavedBackupFile(filename: String): File {
        val file = File(testFilesDir, filename)
        val standardData = createBackupData(listOf("TESTSAVE"))
        val interleavedData = interleave(standardData)
        file.writeBytes(interleavedData)
        return file
    }

    private fun createEmptyBackupFile(filename: String): File {
        val file = File(testFilesDir, filename)
        val header = createHeader()
        file.writeBytes(header)
        return file
    }

    private fun createMultiEntryBackupFile(filename: String, interleaved: Boolean): File {
        val file = File(testFilesDir, filename)
        val data = createBackupData(listOf("SAVE001", "SAVE002", "SAVE003"))
        if (interleaved) {
            file.writeBytes(interleave(data))
        } else {
            file.writeBytes(data)
        }
        return file
    }

    private fun createHeader(): ByteArray {
        val header = ByteArray(64)
        val magic = "BackUpRam Format".toByteArray(Charsets.US_ASCII)
        System.arraycopy(magic, 0, header, 0, magic.size)
        return header
    }

    private fun createBackupData(filenames: List<String>): ByteArray {
        val header = createHeader()
        val entries = filenames.map { createDirectoryEntry(it) }
        return header + entries.reduce { acc, bytes -> acc + bytes }
    }

    private fun createDirectoryEntry(filename: String): ByteArray {
        val entry = ByteArray(64)
        // Status byte (0x80 = valid)
        entry[0] = 0x80.toByte()

        // Filename (11 bytes)
        val filenameBytes = filename.toByteArray(Charsets.US_ASCII)
        System.arraycopy(filenameBytes, 0, entry, 1, minOf(filenameBytes.size, 11))

        // Comment (10 bytes) - start at offset 12
        // Language (1 byte) at offset 22
        // Padding (1 byte) at offset 23

        // Date fields (5 bytes) starting at offset 24
        entry[24] = 44 // Year (2024 - 1980)
        entry[25] = 1 // Month
        entry[26] = 15 // Day
        entry[27] = 10 // Hour
        entry[28] = 30 // Minute

        // Data size (4 bytes, big-endian) at offset 29
        entry[29] = 0
        entry[30] = 0
        entry[31] = 1
        entry[32] = 0 // 256 bytes

        // Block size (2 bytes, big-endian) at offset 33
        entry[33] = 0
        entry[34] = 4 // 4 blocks

        return entry
    }

    private fun interleave(data: ByteArray): ByteArray {
        val interleaved = ByteArray(data.size * 2)
        for (i in data.indices) {
            interleaved[i * 2] = 0xFF.toByte()
            interleaved[i * 2 + 1] = data[i]
        }
        return interleaved
    }
}
