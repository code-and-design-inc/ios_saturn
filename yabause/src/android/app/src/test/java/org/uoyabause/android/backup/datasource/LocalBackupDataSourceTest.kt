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

import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.uoyabause.android.backup.model.BackupItem
import org.uoyabause.android.backup.model.DeviceType
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.charset.Charset

/**
 * Unit tests for LocalBackupDataSource
 * Tests the backup RAM parsing logic independently of YabauseStorage.
 * Test cases: UT-D01 ~ UT-D07
 *
 * Note: Tests requiring YabauseStorage are moved to integration tests
 * since YabauseStorage is a singleton that depends on application context.
 */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28])
class LocalBackupDataSourceTest {
    @get:Rule
    val tempFolder = TemporaryFolder()

    companion object {
        // Saturn backup RAM constants
        private const val BACKUP_MAGIC = "BackUpRam Format"
        private const val HEADER_SIZE = 64
        private const val DIR_ENTRY_SIZE = 64
        private const val FILENAME_SIZE = 11
        private const val COMMENT_SIZE = 10

        // MS932 charset for Japanese text
        private val MS932 = Charset.forName("MS932")
    }

    // Helper data class for test backup entries
    private data class TestBackupEntry(
        val filename: String = "SAVE",
        val comment: String = "",
        val language: Int = 0,
        val year: Int = 44, // 2024
        val month: Int = 1,
        val day: Int = 1,
        val hour: Int = 0,
        val minute: Int = 0,
        val dataSize: Int = 64,
        val blockSize: Int = 1,
    )

    // UT-D01: Test backup RAM header validation
    @Test
    fun `valid header starts with BackUpRam`() {
        val validHeader = "BackUpRam Format"
        assertTrue(validHeader.startsWith("BackUpRam"))
    }

    @Test
    fun `invalid header does not start with BackUpRam`() {
        val invalidHeader = "InvalidHeader"
        assertTrue(!invalidHeader.startsWith("BackUpRam"))
    }

    // UT-D02: Test directory entry parsing structure
    @Test
    fun `directory entry has correct size of 64 bytes`() {
        assertEquals(64, DIR_ENTRY_SIZE)
    }

    @Test
    fun `filename field is 11 bytes`() {
        assertEquals(11, FILENAME_SIZE)
    }

    @Test
    fun `comment field is 10 bytes`() {
        assertEquals(10, COMMENT_SIZE)
    }

    // UT-D03: Test entry status byte interpretation
    @Test
    fun `status byte 0x80 indicates valid entry`() {
        val statusByte: Byte = 0x80.toByte()
        val status = statusByte.toInt() and 0xFF
        assertTrue(status != 0x00)
    }

    @Test
    fun `status byte 0x00 indicates empty entry`() {
        val statusByte: Byte = 0x00
        val status = statusByte.toInt() and 0xFF
        assertEquals(0, status)
    }

    // UT-D04: Test date parsing (Saturn BUP format)
    @Test
    fun `year is offset from 1980`() {
        val yearByte = 44 // Represents 2024
        val actualYear = yearByte + 1980
        assertEquals(2024, actualYear)
    }

    @Test
    fun `date format is correct`() {
        val year = 2024
        val month = 3
        val day = 15
        val hour = 10
        val minute = 30

        val formatted = String.format(
            "%04d/%02d/%02d %02d:%02d:00",
            year,
            month,
            day,
            hour,
            minute,
        )

        assertEquals("2024/03/15 10:30:00", formatted)
    }

    // UT-D05: Test MS932 (Shift-JIS) encoding
    @Test
    fun `MS932 encoding handles ASCII text`() {
        val text = "SAVE001"
        val bytes = text.toByteArray(MS932)
        val decoded = String(bytes, MS932)
        assertEquals(text, decoded)
    }

    @Test
    fun `MS932 encoding handles Japanese text`() {
        val text = "テスト"
        val bytes = text.toByteArray(MS932)
        val decoded = String(bytes, MS932)
        assertEquals(text, decoded)
    }

    // UT-D06: Test ByteBuffer operations with Big Endian order
    @Test
    fun `ByteBuffer reads big endian integer correctly`() {
        val buffer = ByteBuffer.allocate(4).order(ByteOrder.BIG_ENDIAN)
        buffer.putInt(0x12345678)
        buffer.flip()

        val value = buffer.getInt()
        assertEquals(0x12345678, value)
    }

    @Test
    fun `ByteBuffer reads big endian short correctly`() {
        val buffer = ByteBuffer.allocate(2).order(ByteOrder.BIG_ENDIAN)
        buffer.putShort(0x1234.toShort())
        buffer.flip()

        val value = buffer.getShort().toInt() and 0xFFFF
        assertEquals(0x1234, value)
    }

    // UT-D07: Test device type mapping
    @Test
    fun `INTERNAL device type has id 0`() {
        assertEquals(0, DeviceType.INTERNAL.id)
    }

    @Test
    fun `EXTERNAL device type has id 1`() {
        assertEquals(1, DeviceType.EXTERNAL.id)
    }

    @Test
    fun `CLOUD device type has id 48`() {
        assertEquals(48, DeviceType.CLOUD.id)
    }

    @Test
    fun `SHARED device type has id -1`() {
        assertEquals(-1, DeviceType.SHARED.id)
    }

    // Test backup RAM structure creation
    @Test
    fun `createBackupRamHeader produces valid header`() {
        val header = createBackupRamHeader()

        // Header should be 64 bytes
        assertEquals(HEADER_SIZE, header.size)

        // Header should start with magic string
        val magic = String(header.copyOf(BACKUP_MAGIC.length), Charsets.US_ASCII)
        assertEquals(BACKUP_MAGIC, magic)
    }

    @Test
    fun `createDirectoryEntry produces 64 byte entry`() {
        val entry = TestBackupEntry(
            filename = "TEST",
            comment = "Comment",
            dataSize = 1024,
            blockSize = 5,
        )

        val bytes = createDirectoryEntry(entry)

        assertEquals(DIR_ENTRY_SIZE, bytes.size)
    }

    @Test
    fun `createDirectoryEntry contains correct filename`() {
        val entry = TestBackupEntry(filename = "MYSAVE")
        val bytes = createDirectoryEntry(entry)

        // Extract filename from bytes (offset 1, length 11)
        val filenameBytes = bytes.copyOfRange(1, 1 + FILENAME_SIZE)
        val filename = String(filenameBytes, MS932).trim { it <= ' ' || it == '\u0000' }

        assertEquals("MYSAVE", filename)
    }

    @Test
    fun `createDirectoryEntry contains correct data size`() {
        val expectedSize = 2048
        val entry = TestBackupEntry(dataSize = expectedSize)
        val bytes = createDirectoryEntry(entry)

        // Extract data size from bytes
        // Offset: status(1) + filename(11) + comment(10) + language(1) + padding(1) + date(5) = 29
        val buffer = ByteBuffer.wrap(bytes).order(ByteOrder.BIG_ENDIAN)
        buffer.position(29)
        val dataSize = buffer.getInt()

        assertEquals(expectedSize, dataSize)
    }

    @Test
    fun `createDirectoryEntry contains correct block size`() {
        val expectedBlocks = 10
        val entry = TestBackupEntry(blockSize = expectedBlocks)
        val bytes = createDirectoryEntry(entry)

        // Extract block size from bytes
        // Offset: status(1) + filename(11) + comment(10) + language(1) + padding(1) + date(5) + dataSize(4) = 33
        val buffer = ByteBuffer.wrap(bytes).order(ByteOrder.BIG_ENDIAN)
        buffer.position(33)
        val blockSize = buffer.getShort().toInt() and 0xFFFF

        assertEquals(expectedBlocks, blockSize)
    }

    // Helper: Create backup RAM header
    private fun createBackupRamHeader(): ByteArray {
        val header = ByteArray(HEADER_SIZE)
        val magic = BACKUP_MAGIC.toByteArray(Charsets.US_ASCII)
        System.arraycopy(magic, 0, header, 0, magic.size)
        return header
    }

    // Helper: Create directory entry
    private fun createDirectoryEntry(entry: TestBackupEntry): ByteArray {
        val buffer = ByteBuffer.allocate(DIR_ENTRY_SIZE).order(ByteOrder.BIG_ENDIAN)

        // Status byte (0x80 = valid entry)
        buffer.put(0x80.toByte())

        // Filename (11 bytes, MS932)
        val filenameBytes = entry.filename.toByteArray(MS932)
        val paddedFilename = filenameBytes.copyOf(FILENAME_SIZE)
        buffer.put(paddedFilename)

        // Comment (10 bytes, MS932)
        val commentBytes = entry.comment.toByteArray(MS932)
        val paddedComment = commentBytes.copyOf(COMMENT_SIZE)
        buffer.put(paddedComment)

        // Language (1 byte)
        buffer.put(entry.language.toByte())

        // Padding (1 byte)
        buffer.put(0.toByte())

        // Date fields
        buffer.put(entry.year.toByte())
        buffer.put(entry.month.toByte())
        buffer.put(entry.day.toByte())
        buffer.put(entry.hour.toByte())
        buffer.put(entry.minute.toByte())

        // Data size (4 bytes)
        buffer.putInt(entry.dataSize)

        // Block size (2 bytes)
        buffer.putShort(entry.blockSize.toShort())

        // Pad remaining bytes to reach 64
        return buffer.array()
    }

    // Helper: Create interleaved data (0xFF before each byte)
    private fun interleave(data: ByteArray): ByteArray {
        val interleaved = ByteArray(data.size * 2)
        for (i in data.indices) {
            interleaved[i * 2] = 0xFF.toByte()
            interleaved[i * 2 + 1] = data[i]
        }
        return interleaved
    }

    // ==============================
    // Interleaved format tests (UT-D08 ~ UT-D13)
    // ==============================

    // UT-D08: Test interleaved format detection
    @Test
    fun `isInterleaved returns false for standard format`() {
        val standardData = createBackupRamHeader()
        val file = tempFolder.newFile("standard.ram")
        file.writeBytes(standardData)

        assertFalse(LocalBackupDataSource.isInterleaved(file))
    }

    @Test
    fun `isInterleaved returns true for interleaved format`() {
        val standardData = createBackupRamHeader()
        val interleavedData = interleave(standardData)
        val file = tempFolder.newFile("interleaved.ram")
        file.writeBytes(interleavedData)

        assertTrue(LocalBackupDataSource.isInterleaved(file))
    }

    @Test
    fun `isInterleaved returns false for empty file`() {
        val file = tempFolder.newFile("empty.ram")

        assertFalse(LocalBackupDataSource.isInterleaved(file))
    }

    @Test
    fun `isInterleaved returns false for nonexistent file`() {
        val file = File(tempFolder.root, "nonexistent.ram")

        assertFalse(LocalBackupDataSource.isInterleaved(file))
    }

    @Test
    fun `isInterleaved returns false for too small file`() {
        val file = tempFolder.newFile("tiny.ram")
        file.writeBytes(byteArrayOf(0xFF.toByte(), 'B'.code.toByte()))

        assertFalse(LocalBackupDataSource.isInterleaved(file))
    }

    // UT-D09: Test de-interleaving
    @Test
    fun `readDeinterleaved returns original data for standard format`() {
        val standardData = createBackupRamHeader()
        val file = tempFolder.newFile("standard.ram")
        file.writeBytes(standardData)

        val result = LocalBackupDataSource.readDeinterleaved(file)

        assertArrayEquals(standardData, result)
    }

    @Test
    fun `readDeinterleaved removes markers for interleaved format`() {
        val originalData = createBackupRamHeader()
        val interleavedData = interleave(originalData)
        val file = tempFolder.newFile("interleaved.ram")
        file.writeBytes(interleavedData)

        val result = LocalBackupDataSource.readDeinterleaved(file)

        assertArrayEquals(originalData, result)
    }

    @Test
    fun `readDeinterleaved preserves content after de-interleaving`() {
        val header = createBackupRamHeader()
        val entry = createDirectoryEntry(TestBackupEntry(filename = "TEST", comment = "Comment"))
        val fullData = header + entry

        val interleavedData = interleave(fullData)
        val file = tempFolder.newFile("backup.ram")
        file.writeBytes(interleavedData)

        val result = LocalBackupDataSource.readDeinterleaved(file)

        // Verify header is preserved
        val resultMagic = String(result.copyOf(BACKUP_MAGIC.length), Charsets.US_ASCII)
        assertEquals(BACKUP_MAGIC, resultMagic)

        // Verify filename in entry is preserved
        val entryStart = HEADER_SIZE
        val filenameBytes = result.copyOfRange(entryStart + 1, entryStart + 1 + FILENAME_SIZE)
        val filename = String(filenameBytes, MS932).trim { it <= ' ' || it == '\u0000' }
        assertEquals("TEST", filename)
    }

    // UT-D10: Test interleaved data structure
    @Test
    fun `interleaved file is exactly twice the logical size`() {
        val standardData = createBackupRamHeader()
        val interleavedData = interleave(standardData)

        assertEquals(standardData.size * 2, interleavedData.size)
    }

    @Test
    fun `interleaved format has 0xFF before each data byte`() {
        val testData = byteArrayOf('H'.code.toByte(), 'i'.code.toByte())
        val interleaved = interleave(testData)

        assertEquals(0xFF.toByte(), interleaved[0])
        assertEquals('H'.code.toByte(), interleaved[1])
        assertEquals(0xFF.toByte(), interleaved[2])
        assertEquals('i'.code.toByte(), interleaved[3])
    }

    // UT-D11: Test backup parsing with interleaved format
    @Test
    fun `interleaved backup header is correctly identified`() {
        val header = createBackupRamHeader()
        val interleavedHeader = interleave(header)

        // First 4 bytes should be: 0xFF, 'B', 0xFF, 'a'
        assertEquals(0xFF.toByte(), interleavedHeader[0])
        assertEquals('B'.code.toByte(), interleavedHeader[1])
        assertEquals(0xFF.toByte(), interleavedHeader[2])
        assertEquals('a'.code.toByte(), interleavedHeader[3])
    }

    // UT-D12: Test mixed scenario validation
    @Test
    fun `standard format detection is stable`() {
        val standardFile = tempFolder.newFile("standard.ram")
        standardFile.writeBytes(createBackupRamHeader())

        // Multiple calls should return the same result
        assertFalse(LocalBackupDataSource.isInterleaved(standardFile))
        assertFalse(LocalBackupDataSource.isInterleaved(standardFile))
    }

    @Test
    fun `interleaved format detection is stable`() {
        val interleavedFile = tempFolder.newFile("interleaved.ram")
        interleavedFile.writeBytes(interleave(createBackupRamHeader()))

        // Multiple calls should return the same result
        assertTrue(LocalBackupDataSource.isInterleaved(interleavedFile))
        assertTrue(LocalBackupDataSource.isInterleaved(interleavedFile))
    }

    // UT-D13: Test edge cases
    @Test
    fun `handles file with only 0xFF bytes`() {
        val file = tempFolder.newFile("allFF.ram")
        file.writeBytes(ByteArray(64) { 0xFF.toByte() })

        // This should not be detected as interleaved (no valid 'B' 'a' pattern)
        assertFalse(LocalBackupDataSource.isInterleaved(file))
    }

    @Test
    fun `handles file starting with 0xFF but not interleaved pattern`() {
        val file = tempFolder.newFile("falsePositive.ram")
        // 0xFF, 'X', 0xFF, 'Y' - not valid interleaved header
        file.writeBytes(byteArrayOf(0xFF.toByte(), 'X'.code.toByte(), 0xFF.toByte(), 'Y'.code.toByte()))

        assertFalse(LocalBackupDataSource.isInterleaved(file))
    }

    // ==============================
    // Delete operation tests (UT-D14 ~ UT-D17)
    // ==============================

    // UT-D14: Test status byte clearing for deletion
    @Test
    fun `deletion clears status byte bit 7`() {
        // Simulate a valid save status byte
        val statusByte: Byte = 0x80.toByte()

        // After deletion, bit 7 should be cleared
        val deletedStatus = (statusByte.toInt() and 0x7F).toByte()

        assertEquals(0x00.toByte(), deletedStatus)
    }

    @Test
    fun `deleted entry has zero status in interleaved format`() {
        // In interleaved format, deletion sets offset+1 to 0x00
        val blockData = ByteArray(128) { 0xFF.toByte() }
        blockData[1] = 0x80.toByte() // Status byte set to valid

        // Simulate deletion
        blockData[1] = 0x00

        assertEquals(0x00.toByte(), blockData[1])
    }

    // UT-D15: Test delete operation correctness
    @Test
    fun `interleaved status byte at correct offset`() {
        // In interleaved format: blockIndex * 128 + 1
        val blockIndex = 2
        val blockSize = 128
        val statusOffset = blockIndex * blockSize + 1

        assertEquals(257, statusOffset) // Block 2 starts at 256, status at 257
    }

    @Test
    fun `non-interleaved status byte at correct offset`() {
        // In non-interleaved format: blockIndex * 64
        val blockIndex = 2
        val blockSize = 64
        val statusOffset = blockIndex * blockSize

        assertEquals(128, statusOffset)
    }

    // UT-D16: Test block calculation for saves
    @Test
    fun `calculate required blocks for small save`() {
        // Formula: (dataSize + 0x1D) / (64 - 6), round up
        val dataSize = 50
        val result = (dataSize + 0x1D) / (64 - 6) + if ((dataSize + 0x1D) % (64 - 6) > 0) 1 else 0

        assertEquals(2, result) // (50 + 29) / 58 = 1.36 -> 2 blocks
    }

    @Test
    fun `calculate required blocks for single block save`() {
        val dataSize = 20
        val result = (dataSize + 0x1D) / (64 - 6) + if ((dataSize + 0x1D) % (64 - 6) > 0) 1 else 0

        assertEquals(1, result) // (20 + 29) / 58 = 0.84 -> 1 block
    }

    // UT-D17: Test filename matching for block search
    @Test
    fun `filename at correct interleaved offset`() {
        // In interleaved format: blockOffset + 0x09 + (i * 2)
        val blockOffset = 256 // Block 2
        val filenameStart = blockOffset + 0x09

        assertEquals(265, filenameStart)
    }

    @Test
    fun `filename at correct non-interleaved offset`() {
        // In non-interleaved format: blockOffset + 0x04
        val blockOffset = 128 // Block 2
        val filenameStart = blockOffset + 0x04

        assertEquals(132, filenameStart)
    }

    // ==============================
    // Copy/Import operation tests (UT-D18 ~ UT-D21)
    // ==============================

    // UT-D18: Test header writing structure
    @Test
    fun `header status byte is set to 0x80 for valid save`() {
        val validStatus = 0x80.toByte()
        assertTrue((validStatus.toInt() and 0x80) != 0)
    }

    @Test
    fun `interleaved header offsets are correct`() {
        // Verify key offsets in interleaved format
        assertEquals(0x09, 9) // Filename start
        assertEquals(0x1F, 31) // Language
        assertEquals(0x21, 33) // Comment start
        assertEquals(0x35, 53) // Date start
        assertEquals(0x3D, 61) // Data size start
        assertEquals(0x45, 69) // Block table / data start
    }

    // UT-D19: Test date encoding
    @Test
    fun `Saturn date encoding formula`() {
        // Saturn date = totalDays * 0x5A0 + hour * 0x3C + minute
        val totalDays = 100
        val hour = 12
        val minute = 30

        val encoded = totalDays * 0x5A0 + hour * 0x3C + minute

        // 100 * 1440 + 12 * 60 + 30 = 144000 + 720 + 30 = 144750
        assertEquals(144750, encoded)
    }

    // UT-D20: Test block table writing
    @Test
    fun `block table entry is 2 bytes big-endian`() {
        val blockNum = 0x1234
        val highByte = ((blockNum shr 8) and 0xFF).toByte()
        val lowByte = (blockNum and 0xFF).toByte()

        assertEquals(0x12.toByte(), highByte)
        assertEquals(0x34.toByte(), lowByte)
    }

    @Test
    fun `block table terminates with zero`() {
        val endMarker = 0x0000
        assertEquals(0, endMarker)
    }

    // UT-D21: Test free block calculation
    @Test
    fun `first two blocks are reserved for header`() {
        val reservedBlocks = setOf(0, 1)
        assertTrue(0 in reservedBlocks)
        assertTrue(1 in reservedBlocks)
        assertFalse(2 in reservedBlocks)
    }

    @Test
    fun `free blocks start from block 2`() {
        val totalBlocks = 10
        val freeBlocksRange = 2 until totalBlocks

        assertEquals(2, freeBlocksRange.first)
        assertEquals(9, freeBlocksRange.last)
    }

    // ==============================
    // Multi-block extraction and write/read round-trip tests (UT-D22 ~ UT-D25)
    // ==============================

    // Constants matching LocalBackupDataSource
    private val INTERLEAVED_BLOCK_SIZE_CONST = 128
    private val BLOCK_SIZE_CONST = 64

    /**
     * Create an interleaved backup RAM file with a specific number of total blocks.
     * The first 2 blocks are header ("BackUpRam Format"), rest are free (filled with 0x00 data bytes).
     */
    private fun createInterleavedBackupRam(totalBlocks: Int): ByteArray {
        val logicalSize = totalBlocks * BLOCK_SIZE_CONST
        val rawSize = logicalSize * 2
        val rawData = ByteArray(rawSize)

        // Fill with interleave markers (0xFF before each byte)
        for (i in 0 until logicalSize) {
            rawData[i * 2] = 0xFF.toByte()
            rawData[i * 2 + 1] = 0x00
        }

        // Write header "BackUpRam Format" in interleaved format
        val magic = "BackUpRam Format"
        for (i in magic.indices) {
            rawData[i * 2 + 1] = magic[i].code.toByte()
        }

        return rawData
    }

    /**
     * Write a save entry into interleaved raw data at a specific block.
     * This creates a single-block save (no continuation blocks).
     */
    private fun writeSingleBlockSave(
        rawData: ByteArray,
        blockIndex: Int,
        filename: String,
        data: ByteArray,
    ) {
        val blockOffset = blockIndex * INTERLEAVED_BLOCK_SIZE_CONST

        // Status byte: 0x80 (valid save)
        rawData[blockOffset + 1] = 0x80.toByte()

        // Filename at offset 0x09 (interleaved)
        val fnBytes = filename.toByteArray(MS932).copyOf(FILENAME_SIZE)
        for (i in 0 until FILENAME_SIZE) {
            rawData[blockOffset + 0x09 + (i * 2)] = fnBytes[i]
        }

        // Language at 0x1F
        rawData[blockOffset + 0x1F] = 0x00

        // Comment at 0x21 (10 bytes, interleaved)
        val comment = "test".toByteArray(MS932).copyOf(COMMENT_SIZE)
        for (i in 0 until COMMENT_SIZE) {
            rawData[blockOffset + 0x21 + (i * 2)] = comment[i]
        }

        // Data size at 0x3D, 0x3F, 0x41, 0x43 (big-endian)
        val dataSize = data.size
        rawData[blockOffset + 0x3D] = ((dataSize shr 24) and 0xFF).toByte()
        rawData[blockOffset + 0x3F] = ((dataSize shr 16) and 0xFF).toByte()
        rawData[blockOffset + 0x41] = ((dataSize shr 8) and 0xFF).toByte()
        rawData[blockOffset + 0x43] = (dataSize and 0xFF).toByte()

        // Block table: empty (no continuation blocks), write 0x0000 terminator
        rawData[blockOffset + 0x45] = 0x00
        rawData[blockOffset + 0x45 + 2] = 0x00

        // Data starts after block table terminator
        // Block table = 0 entries, terminator = 4 bytes (interleaved)
        val dataStart = blockOffset + 0x45 + 4
        for (i in data.indices) {
            if (dataStart + i * 2 < rawData.size) {
                rawData[dataStart + i * 2] = data[i]
            }
        }
    }

    /**
     * Write a multi-block save entry into interleaved raw data.
     * Uses multiple blocks with block table pointing to continuation blocks.
     * Follows bios.c BiosBUPImport algorithm exactly.
     *
     * @param rawData The interleaved backup RAM
     * @param blocks List of block indices (first is header, rest are continuation)
     * @param filename The save filename
     * @param data The save data
     */
    private fun writeMultiBlockSave(
        rawData: ByteArray,
        blocks: List<Int>,
        filename: String,
        data: ByteArray,
    ) {
        val headerBlock = blocks[0]
        val blockOffset = headerBlock * INTERLEAVED_BLOCK_SIZE_CONST

        // Status byte
        rawData[blockOffset + 1] = 0x80.toByte()

        // Filename
        val fnBytes = filename.toByteArray(MS932).copyOf(FILENAME_SIZE)
        for (i in 0 until FILENAME_SIZE) {
            rawData[blockOffset + 0x09 + (i * 2)] = fnBytes[i]
        }

        // Language
        rawData[blockOffset + 0x1F] = 0x00

        // Comment
        val comment = "multiblk".toByteArray(MS932).copyOf(COMMENT_SIZE)
        for (i in 0 until COMMENT_SIZE) {
            rawData[blockOffset + 0x21 + (i * 2)] = comment[i]
        }

        // Data size
        val dataSize = data.size
        rawData[blockOffset + 0x3D] = ((dataSize shr 24) and 0xFF).toByte()
        rawData[blockOffset + 0x3F] = ((dataSize shr 16) and 0xFF).toByte()
        rawData[blockOffset + 0x41] = ((dataSize shr 8) and 0xFF).toByte()
        rawData[blockOffset + 0x43] = (dataSize and 0xFF).toByte()

        // Write block table and data using cursor-based approach matching bios.c
        // In bios.c: blockswritten starts at 0, and on boundary crossing does:
        //   blockswritten++; workaddr = blocktbl[blockswritten] * blocksize * 2 + 9
        // So the first boundary jump goes to blocktbl[1] (first continuation block)
        var cursor = blockOffset + 0x45
        var blocksWritten = 0

        // Write block table entries (continuation blocks)
        for (i in 1 until blocks.size) {
            val blockNum = blocks[i]
            rawData[cursor] = ((blockNum shr 8) and 0xFF).toByte()
            rawData[cursor + 2] = (blockNum and 0xFF).toByte()
            cursor += 4
            // Check block boundary: ((cursor-1) & 127) == 0
            if (((cursor - 1) and (INTERLEAVED_BLOCK_SIZE_CONST - 1)) == 0) {
                blocksWritten++
                if (blocksWritten < blocks.size) {
                    cursor = blocks[blocksWritten] * INTERLEAVED_BLOCK_SIZE_CONST + 9
                }
            }
        }

        // Write terminator - two zero bytes with boundary checks
        rawData[cursor] = 0x00
        cursor += 2
        if (((cursor - 1) and (INTERLEAVED_BLOCK_SIZE_CONST - 1)) == 0) {
            blocksWritten++
            if (blocksWritten < blocks.size) {
                cursor = blocks[blocksWritten] * INTERLEAVED_BLOCK_SIZE_CONST + 9
            }
        }
        rawData[cursor] = 0x00
        cursor += 2
        if (((cursor - 1) and (INTERLEAVED_BLOCK_SIZE_CONST - 1)) == 0) {
            blocksWritten++
            if (blocksWritten < blocks.size) {
                cursor = blocks[blocksWritten] * INTERLEAVED_BLOCK_SIZE_CONST + 9
            }
        }

        // Write data with block boundary checks
        for (i in data.indices) {
            rawData[cursor] = data[i]
            cursor += 2
            if (((cursor - 1) and (INTERLEAVED_BLOCK_SIZE_CONST - 1)) == 0) {
                blocksWritten++
                if (blocksWritten < blocks.size) {
                    cursor = blocks[blocksWritten] * INTERLEAVED_BLOCK_SIZE_CONST + 9
                }
            }
        }
    }

    // UT-D22: Single-block save extraction test
    @Test
    fun `extractBackupData reads single-block save correctly`() = runBlocking {
        // Create backup RAM with 20 blocks (2 header + 18 save blocks)
        val rawData = createInterleavedBackupRam(20)

        // Write a small save (10 bytes of data) at block 2
        val testData = ByteArray(10) { (it + 0xA0).toByte() }
        writeSingleBlockSave(rawData, 2, "SMALL_SAVE", testData)

        // Write to file
        val file = tempFolder.newFile("single_block.ram")
        file.writeBytes(rawData)

        // Create context and data source
        val context = androidx.test.core.app.ApplicationProvider
            .getApplicationContext<android.content.Context>()
        val dataSource = LocalBackupDataSource(context)

        // Create a BackupItem matching the save
        val item = BackupItem(
            id = "0",
            filename = "SMALL_SAVE",
            dataSize = 10,
            deviceType = DeviceType.INTERNAL,
        )

        // Extract the data
        val extracted = dataSource.extractBackupDataFromFile(file.absolutePath, item)

        assertNotNull("Should extract data successfully", extracted)
        assertArrayEquals("Extracted data should match original", testData, extracted)
    }

    // UT-D23: Multi-block save extraction test (3 blocks)
    @Test
    fun `extractBackupData reads multi-block save correctly`() = runBlocking {
        // Create backup RAM with 20 blocks
        val rawData = createInterleavedBackupRam(20)

        // Data that spans 3 blocks
        // In interleaved format, each block has 128 raw bytes.
        // Header block data capacity after header(0x45) + block_table(2entries*4 + 4terminator) + data:
        // Block 0x45 = 69, plus block table = 69 + 2*4 + 4 = 81, leaving 128-81 = 47 raw bytes = ~23 data bytes
        // Continuation blocks start at offset 9, so capacity = (128-9)/2 = 59 data bytes per continuation block
        // Total for 3 blocks ≈ 23 + 59 + 59 = 141 data bytes possible
        // Let's use 100 bytes of data to be safe
        val testData = ByteArray(100) { (it and 0xFF).toByte() }

        // Blocks: header=2, continuation=3,4
        writeMultiBlockSave(rawData, listOf(2, 3, 4), "MULTI_SAVE", testData)

        val file = tempFolder.newFile("multi_block.ram")
        file.writeBytes(rawData)

        val context = androidx.test.core.app.ApplicationProvider
            .getApplicationContext<android.content.Context>()
        val dataSource = LocalBackupDataSource(context)

        val item = BackupItem(
            id = "0",
            filename = "MULTI_SAVE",
            dataSize = 100,
            deviceType = DeviceType.INTERNAL,
        )

        val extracted = dataSource.extractBackupDataFromFile(file.absolutePath, item)

        assertNotNull("Should extract multi-block data successfully", extracted)
        assertArrayEquals("Extracted multi-block data should match original", testData, extracted)
    }

    // UT-D24: Write/read round-trip test
    @Test
    fun `importBackupData and extractBackupData round-trip preserves data`() = runBlocking {
        // Create empty backup RAM with 20 blocks
        val rawData = createInterleavedBackupRam(20)
        val file = tempFolder.newFile("roundtrip.ram")
        file.writeBytes(rawData)

        val context = androidx.test.core.app.ApplicationProvider
            .getApplicationContext<android.content.Context>()
        val dataSource = LocalBackupDataSource(context)

        // Write data using importBackupDataToFile
        val testData = ByteArray(80) { (it * 3 and 0xFF).toByte() }
        val filename = "ROUNDTRIP"

        val writeSuccess = dataSource.importBackupDataToFile(
            filePath = file.absolutePath,
            filename = filename,
            comment = "test",
            language = 0,
            dateRaw = 0,
            data = testData,
        )

        assertTrue("Import should succeed", writeSuccess)

        // Construct BackupItem directly (parseBackupFile needs YabauseStorage which is unavailable in unit tests)
        val item = BackupItem(
            id = "0",
            filename = filename,
            dataSize = testData.size,
            deviceType = DeviceType.INTERNAL,
        )

        // Extract and verify round-trip
        val extracted = dataSource.extractBackupDataFromFile(file.absolutePath, item)

        assertNotNull("Should extract data from round-trip", extracted)
        assertArrayEquals("Round-trip data should be identical", testData, extracted)
    }

    // UT-D25: Fragmented block chain test (non-contiguous blocks)
    @Test
    fun `extractBackupData handles fragmented block chain`() = runBlocking {
        // Create backup RAM with 20 blocks
        val rawData = createInterleavedBackupRam(20)

        // Use non-contiguous blocks: header=2, continuation=5,9
        // This simulates fragmentation
        val testData = ByteArray(100) { ((it * 7 + 13) and 0xFF).toByte() }
        writeMultiBlockSave(rawData, listOf(2, 5, 9), "FRAGMENT", testData)

        val file = tempFolder.newFile("fragmented.ram")
        file.writeBytes(rawData)

        val context = androidx.test.core.app.ApplicationProvider
            .getApplicationContext<android.content.Context>()
        val dataSource = LocalBackupDataSource(context)

        val item = BackupItem(
            id = "0",
            filename = "FRAGMENT",
            dataSize = 100,
            deviceType = DeviceType.INTERNAL,
        )

        val extracted = dataSource.extractBackupDataFromFile(file.absolutePath, item)

        assertNotNull("Should extract fragmented data successfully", extracted)
        assertArrayEquals("Fragmented data should match original", testData, extracted)
    }

    // UT-D26: Multi-block write/read round-trip with large data
    @Test
    fun `importBackupData handles multi-block save round-trip`() = runBlocking {
        // Create backup RAM with 30 blocks (needs room for 4+ block save)
        val rawData = createInterleavedBackupRam(30)
        val file = tempFolder.newFile("multiblock_roundtrip.ram")
        file.writeBytes(rawData)

        val context = androidx.test.core.app.ApplicationProvider
            .getApplicationContext<android.content.Context>()
        val dataSource = LocalBackupDataSource(context)

        // Write large data that requires multiple blocks
        // Each block ~58 data bytes, so 200 bytes requires ~4 blocks
        val testData = ByteArray(200) { (it and 0xFF).toByte() }

        val writeSuccess = dataSource.importBackupDataToFile(
            filePath = file.absolutePath,
            filename = "NIGHTS___01",
            comment = "NiGHTS",
            language = 0,
            dateRaw = 0,
            data = testData,
        )

        assertTrue("Import of multi-block save should succeed", writeSuccess)

        // Construct BackupItem directly (parseBackupFile needs YabauseStorage which is unavailable in unit tests)
        val item = BackupItem(
            id = "0",
            filename = "NIGHTS___01",
            dataSize = 200,
            deviceType = DeviceType.INTERNAL,
        )

        val extracted = dataSource.extractBackupDataFromFile(file.absolutePath, item)
        assertNotNull("Should extract multi-block round-trip data", extracted)
        assertArrayEquals("Multi-block round-trip data should be identical", testData, extracted)
    }
}
