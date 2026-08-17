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
import android.net.Uri
import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.uoyabause.android.YabauseStorage
import org.uoyabause.android.backup.model.BackupItem
import org.uoyabause.android.backup.model.DeviceType
import java.io.File
import java.io.FileInputStream
import java.nio.charset.Charset

/**
 * Data source for accessing local backup files (internal and external storage).
 * Parses Saturn backup RAM format directly without requiring the emulator to be running.
 *
 * Saturn Backup RAM Format (Interleaved):
 * - Data is stored with 0xFF marker before each actual byte (0xFF, data, 0xFF, data...)
 * - Block size varies by device:
 *   - Internal (device 0): 64 bytes logical
 *   - External (device 1): 512 bytes logical (or 1024 for 32Mbit cartridge)
 * - First 2 blocks: Header "BackUpRam Format"
 * - Save detection: Block's 2nd byte (offset +1 in raw data) has bit 7 set (>= 0x80)
 *
 * Save entry layout (raw offsets from block start):
 * - 0x09: filename (11 bytes, every 2 bytes)
 * - 0x1F: language (1 byte)
 * - 0x21: comment (10 bytes, every 2 bytes)
 * - 0x35, 0x37, 0x39, 0x3B: date (4 bytes, big-endian)
 * - 0x3D, 0x3F, 0x41, 0x43: data size (4 bytes, big-endian)
 */
class LocalBackupDataSource(
    private val context: Context,
) {
    companion object {
        private const val TAG = "LocalBackupDataSource"

        // Saturn backup RAM constants
        private const val BACKUP_MAGIC = "BackUpRam Format"
        private const val INTERNAL_BLOCK_SIZE = 64 // Internal memory block size (device 0)
        private const val EXTERNAL_BLOCK_SIZE = 512 // External cartridge block size (device 1, default)
        private const val EXTERNAL_BLOCK_SIZE_32MBIT = 1024 // 32Mbit cartridge (cartid 0x24)
        private const val FILENAME_SIZE = 11
        private const val COMMENT_SIZE = 10

        // File names for internal/external backup RAM
        private const val INTERNAL_BACKUP_FILE = "memory.ram"
        private const val EXTERNAL_BACKUP_FILE = "external.ram"

        // MS932 charset for Japanese text
        private val MS932 = Charset.forName("MS932")

        // Continuation block data offset (where data starts in non-header blocks)
        // In interleaved format: offset 9 (raw bytes from block start)
        // In non-interleaved format: offset 4
        private const val CONTINUATION_DATA_OFFSET_INTERLEAVED = 9
        private const val CONTINUATION_DATA_OFFSET_NON_INTERLEAVED = 4

        // Maximum expected backup RAM file size (32Mbit cartridge interleaved = 8MB)
        // Any file larger than this is corrupted or invalid
        private const val MAX_BACKUP_FILE_SIZE = 16L * 1024 * 1024 // 16MB generous limit

        // Interleaved format marker byte
        private const val INTERLEAVE_MARKER: Byte = 0xFF.toByte()

        /**
         * Check if the file is in interleaved format.
         * Interleaved format has 0xFF before each data byte (Yabause internal memory format).
         * File size will be exactly 2x the logical size.
         */
        fun isInterleaved(file: File): Boolean {
            if (!file.exists() || file.length() < 4) {
                return false
            }

            FileInputStream(file).use { fis ->
                val header = ByteArray(4)
                fis.read(header)
                // In interleaved format: ff42 ff61 = "Ba" from "BackUpRam Format"
                // Pattern: 0xFF, 'B', 0xFF, 'a'
                return header[0] == INTERLEAVE_MARKER &&
                    header[1] == 'B'.code.toByte() &&
                    header[2] == INTERLEAVE_MARKER &&
                    header[3] == 'a'.code.toByte()
            }
        }

        /**
         * Read file and de-interleave if necessary.
         * @param file The backup RAM file
         * @return De-interleaved byte array (logical data only, 0xFF markers removed)
         */
        fun readDeinterleaved(file: File): ByteArray {
            if (file.length() > MAX_BACKUP_FILE_SIZE) {
                throw IllegalArgumentException("Backup file too large: ${file.length()} bytes (max $MAX_BACKUP_FILE_SIZE). File may be corrupted: ${file.name}")
            }
            val rawData = file.readBytes()

            if (!isInterleaved(file)) {
                return rawData
            }

            // De-interleave: take every second byte (skip 0xFF markers)
            val logicalSize = rawData.size / 2
            val result = ByteArray(logicalSize)

            for (i in 0 until logicalSize) {
                result[i] = rawData[i * 2 + 1]
            }

            return result
        }

        /**
         * Detect the block size for a backup file based on device type and file size.
         * Internal backup uses 64-byte blocks.
         * External backup (cartridge) uses 512-byte blocks (default) or 1024 for 32Mbit type.
         *
         * Based on bios.c GetDeviceStats() line 489:
         *   device 0: blocksize = 0x40 (64)
         *   device 1: blocksize = 0x200 (512) or 0x400 (1024) for cartid 0x24 (32Mbit)
         *
         * Cartridge logical sizes (from bios.c: size = 0x40000 << (cartid & 0x0F)):
         *   4Mbit (cartid 0x21):  0x80000  (512KB)  -> blocksize 512
         *   8Mbit (cartid 0x22):  0x100000 (1MB)    -> blocksize 512
         *   16Mbit (cartid 0x23): 0x200000 (2MB)    -> blocksize 512
         *   32Mbit (cartid 0x24): 0x400000 (4MB)    -> blocksize 1024
         */
        fun getBlockSize(deviceType: DeviceType, file: File): Int = when (deviceType) {
            DeviceType.INTERNAL -> INTERNAL_BLOCK_SIZE
            DeviceType.EXTERNAL -> {
                // Prefer file name to determine cartridge type (robust against truncated files)
                val name = file.nameWithoutExtension.lowercase()
                if (name.endsWith("32")) {
                    EXTERNAL_BLOCK_SIZE_32MBIT
                } else if (name.endsWith("4") || name.endsWith("8") || name.endsWith("16")) {
                    EXTERNAL_BLOCK_SIZE
                } else {
                    // Fallback: infer from logical file size
                    val logicalSize = if (isInterleaved(file)) file.length() / 2 else file.length()
                    if (logicalSize >= 0x400000L) {
                        EXTERNAL_BLOCK_SIZE_32MBIT
                    } else {
                        EXTERNAL_BLOCK_SIZE
                    }
                }
            }
            else -> INTERNAL_BLOCK_SIZE
        }
    }

    /**
     * Get the list of backup items from the specified device type.
     * @param deviceType The device type (INTERNAL or EXTERNAL)
     * @return List of BackupItem objects, or empty list if none found
     */
    suspend fun getBackupItems(deviceType: DeviceType): List<BackupItem> = withContext(Dispatchers.IO) {
        val backupFile = getBackupFile(deviceType) ?: return@withContext emptyList()

        if (!backupFile.exists()) {
            Log.d(TAG, "Backup file does not exist: ${backupFile.absolutePath}")
            return@withContext emptyList()
        }

        try {
            val blockSize = getBlockSize(deviceType, backupFile)
            parseBackupRam(backupFile, deviceType, blockSize)
        } catch (e: Exception) {
            Log.e(TAG, "Error parsing backup RAM: ${e.message}", e)
            emptyList()
        }
    }

    /**
     * Get storage status (total size, free size) for the specified device.
     * @param deviceType The device type
     * @param biosSize The logical size from GetDeviceStats
     * @param biosBlockSize The block size from GetDeviceStats
     * @return Pair of (totalSize, freeSize) in bytes
     */
    suspend fun getStorageStatus(deviceType: DeviceType, biosSize: Int, biosBlockSize: Int): Pair<Int, Int> =
        withContext(Dispatchers.IO) {
            val backupFile = getBackupFile(deviceType) ?: return@withContext Pair(0, 0)

            if (!backupFile.exists()) {
                return@withContext Pair(0, 0)
            }

            try {
                parseStorageStatus(backupFile, biosSize, biosBlockSize)
            } catch (e: Exception) {
                Log.e(TAG, "Error getting storage status: ${e.message}", e)
                Pair(0, 0)
            }
        }

    /**
     * Export a backup item to an external URI (for sharing).
     * @param item The backup item to export
     * @param destinationUri The destination URI
     * @return true if successful, false otherwise
     */
    suspend fun exportBackup(item: BackupItem, destinationUri: Uri): Boolean = withContext(Dispatchers.IO) {
        try {
            val backupFile = getBackupFile(item.deviceType) ?: return@withContext false
            val blockSize = getBlockSize(item.deviceType, backupFile)
            val backupData = extractBackupData(backupFile, item, blockSize)

            if (backupData == null) {
                Log.e(TAG, "Failed to extract backup data for: ${item.filename}")
                return@withContext false
            }

            context.contentResolver.openOutputStream(destinationUri)?.use { output ->
                output.write(backupData)
            }
            true
        } catch (e: Exception) {
            Log.e(TAG, "Error exporting backup: ${e.message}", e)
            false
        }
    }

    /**
     * Import a backup from an external URI.
     * @param sourceUri The source URI
     * @param targetDeviceType The target device type
     * @return true if successful, false otherwise
     */
    suspend fun importBackup(sourceUri: Uri, targetDeviceType: DeviceType): Boolean = withContext(Dispatchers.IO) {
        try {
            val backupData = context.contentResolver.openInputStream(sourceUri)?.use { input ->
                input.readBytes()
            } ?: return@withContext false

            // TODO: Implement backup import logic
            // This requires writing to the Saturn backup RAM format
            Log.w(TAG, "Import backup not fully implemented yet")
            false
        } catch (e: Exception) {
            Log.e(TAG, "Error importing backup: ${e.message}", e)
            false
        }
    }

    /**
     * Delete a backup item from the Saturn backup RAM.
     * Based on bios.c DeleteSave function - simply sets the status byte to 0x00.
     *
     * @param item The backup item to delete
     * @return true if successful, false otherwise
     */
    suspend fun deleteBackup(item: BackupItem): Boolean = withContext(Dispatchers.IO) {
        val backupFile = getBackupFile(item.deviceType) ?: return@withContext false
        if (!backupFile.exists()) return@withContext false

        try {
            val rawData = backupFile.readBytes()
            val isInterleavedFile = isInterleaved(backupFile)
            val blockSize = getBlockSize(item.deviceType, backupFile)
            val interleavedBlockSize = blockSize * 2

            // Find the block index for this save
            val blockIndex = findSaveBlockIndex(rawData, item.filename, isInterleavedFile, blockSize)
            if (blockIndex == null) {
                Log.e(TAG, "Could not find save block for: ${item.filename}")
                return@withContext false
            }

            Log.d(TAG, "Deleting backup '${item.filename}' at block $blockIndex")

            // Delete the save by setting status byte to 0x00
            // Based on bios.c line 668: MappedMemoryWriteByte(addr + (blockoffset * blocksize * 2) + 0x1, 0x00, NULL)
            if (isInterleavedFile) {
                // Interleaved format: status byte is at blockIndex * interleavedBlockSize + 1
                val statusByteOffset = blockIndex * interleavedBlockSize + 1
                rawData[statusByteOffset] = 0x00
            } else {
                // Non-interleaved format: status byte is at blockIndex * blockSize
                // Clear bit 7 of the status byte
                val statusByteOffset = blockIndex * blockSize
                rawData[statusByteOffset] = (rawData[statusByteOffset].toInt() and 0x7F).toByte()
            }

            // Write back to file
            backupFile.writeBytes(rawData)
            Log.d(TAG, "Successfully deleted backup: ${item.filename}")

            // Delete associated screenshot file
            deleteScreenshotFile(item.filename, item.backupFileKey)

            true
        } catch (e: Exception) {
            Log.e(TAG, "Error deleting backup: ${e.message}", e)
            false
        }
    }

    /**
     * Find the block index for a save by filename.
     * Based on bios.c FindSave2 function.
     *
     * @param rawData The raw backup RAM data
     * @param filename The filename to search for
     * @param isInterleaved Whether the data is in interleaved format
     * @param blockSize The logical block size for this device
     * @return Block index if found, null otherwise
     */
    private fun findSaveBlockIndex(
        rawData: ByteArray,
        filename: String,
        isInterleaved: Boolean,
        blockSize: Int,
    ): Int? {
        val interleavedBlockSize = blockSize * 2
        val logicalSize = if (isInterleaved) rawData.size / 2 else rawData.size
        val totalBlocks = logicalSize / blockSize

        // Start from block 2 (first 2 blocks are header)
        for (blockNum in 2 until totalBlocks) {
            val blockOffset = if (isInterleaved) {
                blockNum * interleavedBlockSize
            } else {
                blockNum * blockSize
            }

            if (blockOffset + 1 >= rawData.size) break

            // Check if this block starts a save (status byte has bit 7 set)
            // Based on bios.c line 634: if ( ((s8)MappedMemoryReadByte(addr + i + 1, NULL)) < 0)
            val statusByteOffset = if (isInterleaved) blockOffset + 1 else blockOffset
            val statusByte = rawData[statusByteOffset].toInt()

            if (statusByte and 0x80 != 0) {
                // This block contains a save, check the filename
                val savedFilename = readFilenameAtBlock(rawData, blockOffset, isInterleaved)
                if (savedFilename == filename) {
                    return blockNum
                }
            }
        }
        return null
    }

    /**
     * Read the filename from a save block.
     *
     * @param rawData The raw backup RAM data
     * @param blockOffset The offset to the start of the block in raw data
     * @param isInterleaved Whether the data is in interleaved format
     * @return The filename as a string
     */
    private fun readFilenameAtBlock(rawData: ByteArray, blockOffset: Int, isInterleaved: Boolean): String {
        val filenameBytes = ByteArray(FILENAME_SIZE)

        if (isInterleaved) {
            // Interleaved format: filename at offset 0x09, every 2 bytes
            for (i in 0 until FILENAME_SIZE) {
                val byteOffset = blockOffset + 0x09 + (i * 2)
                if (byteOffset < rawData.size) {
                    filenameBytes[i] = rawData[byteOffset]
                }
            }
        } else {
            // Non-interleaved format: filename at offset 0x04
            for (i in 0 until FILENAME_SIZE) {
                val byteOffset = blockOffset + 0x04 + i
                if (byteOffset < rawData.size) {
                    filenameBytes[i] = rawData[byteOffset]
                }
            }
        }

        return decodeMs932(filenameBytes).trim { it <= ' ' || it == '\u0000' }
    }

    /**
     * State for cursor-based block traversal.
     * Tracks the current raw byte offset and the index into the block table
     * for determining the next continuation block.
     */
    private data class CursorState(
        val cursor: Int,
        val blocksRead: Int,
    )

    /**
     * Advance cursor by [step] bytes and check for block boundary crossing.
     * If the cursor crosses a block boundary, jump to the next continuation block.
     *
     * Based on bios.c pattern:
     *   workaddr += step;
     *   if (((workaddr-1) & ((blocksize << 1) - 1)) == 0) {
     *       workaddr = addr + blocktbl[blocksread] * blocksize * 2 + 9;
     *       blocksread++;
     *   }
     *
     * @param cursor Current raw byte offset
     * @param step Number of raw bytes to advance (e.g. 2 for interleaved, 4 for block table entry)
     * @param blockTable List of continuation block numbers
     * @param blocksRead Index of next block to read from blockTable
     * @param isInterleaved Whether the format is interleaved
     * @param blockSize The logical block size for this device
     * @return Updated CursorState
     */
    private fun advanceCursor(
        cursor: Int,
        step: Int,
        blockTable: List<Int>,
        blocksRead: Int,
        isInterleaved: Boolean,
        blockSize: Int,
    ): CursorState {
        val interleavedBlockSize = blockSize * 2
        val newCursor = cursor + step
        val blockBoundaryMask = if (isInterleaved) {
            interleavedBlockSize - 1
        } else {
            blockSize - 1
        }

        return if (((newCursor - 1) and blockBoundaryMask) == 0 && blocksRead < blockTable.size) {
            val nextBlock = blockTable[blocksRead]
            val nextCursor = if (isInterleaved) {
                nextBlock * interleavedBlockSize + CONTINUATION_DATA_OFFSET_INTERLEAVED
            } else {
                nextBlock * blockSize + CONTINUATION_DATA_OFFSET_NON_INTERLEAVED
            }
            CursorState(nextCursor, blocksRead + 1)
        } else {
            CursorState(newCursor, blocksRead)
        }
    }

    /**
     * Find the raw offset of the header block for a save with the given filename.
     *
     * @param rawData The raw backup RAM data
     * @param filename The filename to search for
     * @param isInterleaved Whether the data is in interleaved format
     * @param blockSize The logical block size for this device
     * @return The raw byte offset of the header block, or null if not found
     */
    private fun findSaveHeaderBlockOffset(
        rawData: ByteArray,
        filename: String,
        isInterleaved: Boolean,
        blockSize: Int,
    ): Int? {
        val interleavedBlockSize = blockSize * 2
        val logicalSize = if (isInterleaved) rawData.size / 2 else rawData.size
        val totalBlocks = logicalSize / blockSize

        for (blockNum in 2 until totalBlocks) {
            val blockOffset = if (isInterleaved) {
                blockNum * interleavedBlockSize
            } else {
                blockNum * blockSize
            }

            if (blockOffset + 1 >= rawData.size) break

            val statusByteOffset = if (isInterleaved) blockOffset + 1 else blockOffset
            val statusByte = rawData[statusByteOffset].toInt()

            if (statusByte and 0x80 != 0) {
                val savedFilename = readFilenameAtBlock(rawData, blockOffset, isInterleaved)
                if (savedFilename == filename) {
                    return blockOffset
                }
            }
        }
        return null
    }

    /**
     * Copy a backup item to another device.
     *
     * @param item The backup item to copy
     * @param targetDevice The target device type
     * @return true if successful, false otherwise
     */
    suspend fun copyBackup(item: BackupItem, targetDevice: DeviceType): Boolean = withContext(Dispatchers.IO) {
        if (item.deviceType == targetDevice) {
            Log.w(TAG, "Cannot copy to the same device")
            return@withContext false
        }

        try {
            // 1. Extract the backup data from source using source device's block size
            val sourceFile = getBackupFile(item.deviceType) ?: return@withContext false
            val sourceBlockSize = getBlockSize(item.deviceType, sourceFile)
            val sourceData = extractBackupData(sourceFile, item, sourceBlockSize)
            if (sourceData == null) {
                Log.e(TAG, "Failed to extract backup data for: ${item.filename}")
                return@withContext false
            }

            // 2. Parse metadata from the item
            val dateRaw = encodeSaturnDate(item.saveDate)

            // 3. Import to target device using target device's block size
            val targetFile = getBackupFile(targetDevice) ?: return@withContext false
            val targetBlockSize = getBlockSize(targetDevice, targetFile)
            importBackupData(
                targetDevice = targetDevice,
                filename = item.filename,
                comment = item.comment,
                language = item.language,
                dateRaw = dateRaw,
                data = sourceData,
                blockSize = targetBlockSize,
            )
        } catch (e: Exception) {
            Log.e(TAG, "Error copying backup: ${e.message}", e)
            false
        }
    }

    /**
     * Import backup data to a device using a date string.
     * Converts the date string to Saturn format and delegates to the raw version.
     *
     * @param targetDevice The target device type
     * @param filename The save filename (max 11 chars)
     * @param comment The save comment (max 10 chars)
     * @param language The language code
     * @param saveDate The save date in "YYYY/MM/DD HH:mm:ss" format
     * @param data The save data bytes
     * @return true if successful, false otherwise
     */
    suspend fun importBackupData(
        targetDevice: DeviceType,
        filename: String,
        comment: String,
        language: Int,
        saveDate: String,
        data: ByteArray,
    ): Boolean {
        val dateRaw = encodeSaturnDate(saveDate)
        val backupFile = getBackupFile(targetDevice) ?: return false
        val blockSize = getBlockSize(targetDevice, backupFile)
        return importBackupData(targetDevice, filename, comment, language, dateRaw, data, blockSize)
    }

    /**
     * Import backup data to a device.
     * Based on bios.c BiosBUPImport function.
     *
     * @param targetDevice The target device type
     * @param filename The save filename (max 11 chars)
     * @param comment The save comment (max 10 chars)
     * @param language The language code
     * @param dateRaw The Saturn format date value
     * @param data The save data bytes
     * @param blockSize The logical block size for the target device
     * @return true if successful, false otherwise
     */
    suspend fun importBackupData(
        targetDevice: DeviceType,
        filename: String,
        comment: String,
        language: Int,
        dateRaw: Int,
        data: ByteArray,
        blockSize: Int,
    ): Boolean = withContext(Dispatchers.IO) {
        val backupFile = getBackupFile(targetDevice) ?: return@withContext false

        try {
            // Read existing backup RAM or create new one
            val rawData = if (backupFile.exists()) {
                backupFile.readBytes().copyOf()
            } else {
                Log.e(TAG, "Backup file does not exist: ${backupFile.absolutePath}")
                return@withContext false
            }

            val isInterleavedFile = isInterleaved(backupFile)

            // 1. Delete existing save with the same filename
            val existingBlock = findSaveBlockIndex(rawData, filename, isInterleavedFile, blockSize)
            if (existingBlock != null) {
                Log.d(TAG, "Deleting existing save '$filename' at block $existingBlock")
                deleteSaveAtBlock(rawData, existingBlock, isInterleavedFile, blockSize)
            }

            // 2. Calculate required blocks
            val dataSize = data.size
            val saveSize = calculateRequiredBlocks(dataSize, blockSize)

            // 3. Get free blocks
            val freeBlocks = getFreeBlocks(rawData, saveSize, isInterleavedFile, blockSize)
            if (freeBlocks == null) {
                Log.e(TAG, "Not enough free space for save: need $saveSize blocks")
                return@withContext false
            }

            Log.d(TAG, "Writing save '$filename' to blocks: ${freeBlocks.joinToString()}")

            // 4. Write header to first block
            writeHeader(
                rawData,
                freeBlocks[0],
                filename,
                comment,
                language,
                dateRaw,
                dataSize,
                isInterleavedFile,
                blockSize,
            )

            // 5. Write block table
            writeBlockTable(rawData, freeBlocks, isInterleavedFile, blockSize)

            // 6. Write data
            writeData(rawData, freeBlocks, data, isInterleavedFile, blockSize)

            // 7. Write back to file
            backupFile.writeBytes(rawData)
            Log.d(TAG, "Successfully imported backup: $filename")
            true
        } catch (e: Exception) {
            Log.e(TAG, "Error importing backup data: ${e.message}", e)
            false
        }
    }

    /**
     * Delete a save at a specific block by setting status byte to 0x00.
     */
    private fun deleteSaveAtBlock(
        rawData: ByteArray,
        blockIndex: Int,
        isInterleaved: Boolean,
        blockSize: Int,
    ) {
        val interleavedBlockSize = blockSize * 2
        if (isInterleaved) {
            rawData[blockIndex * interleavedBlockSize + 1] = 0x00
        } else {
            val offset = blockIndex * blockSize
            rawData[offset] = (rawData[offset].toInt() and 0x7F).toByte()
        }
    }

    /**
     * Calculate the number of blocks required for a save.
     * Based on bios.c: savesize = (datasize + 0x1D) / (blocksize - 6)
     */
    private fun calculateRequiredBlocks(dataSize: Int, blockSize: Int): Int {
        val blocksNeeded = (dataSize + 0x1D) / (blockSize - 6)
        val remainder = (dataSize + 0x1D) % (blockSize - 6)
        return if (remainder > 0) blocksNeeded + 1 else blocksNeeded.coerceAtLeast(1)
    }

    /**
     * Get a list of free blocks.
     * Based on bios.c GetFreeBlocks function.
     *
     * @param rawData The raw backup RAM data
     * @param requiredBlocks Number of blocks needed
     * @param isInterleaved Whether the data is in interleaved format
     * @param blockSize The logical block size for this device
     * @return List of free block indices, or null if not enough space
     */
    private fun getFreeBlocks(
        rawData: ByteArray,
        requiredBlocks: Int,
        isInterleaved: Boolean,
        blockSize: Int,
    ): List<Int>? {
        val interleavedBlockSize = blockSize * 2
        val logicalSize = if (isInterleaved) rawData.size / 2 else rawData.size
        val totalBlocks = logicalSize / blockSize

        // Build used blocks set by scanning saves and their block tables
        val usedBlocks = mutableSetOf<Int>()

        // First 2 blocks are always reserved for header
        usedBlocks.add(0)
        usedBlocks.add(1)

        // Scan for saves and mark their blocks as used
        for (blockNum in 2 until totalBlocks) {
            val blockOffset = if (isInterleaved) {
                blockNum * interleavedBlockSize
            } else {
                blockNum * blockSize
            }

            if (blockOffset + 1 >= rawData.size) break

            val statusByteOffset = if (isInterleaved) blockOffset + 1 else blockOffset
            val statusByte = rawData[statusByteOffset].toInt()

            if (statusByte and 0x80 != 0) {
                // This is a save header block
                usedBlocks.add(blockNum)

                // Read the block table to mark all blocks used by this save
                val saveBlocks = readBlockTableForSave(rawData, blockOffset, isInterleaved, blockSize)
                usedBlocks.addAll(saveBlocks)
            }
        }

        // Find free blocks
        val freeBlocks = mutableListOf<Int>()
        for (blockNum in 2 until totalBlocks) {
            if (blockNum !in usedBlocks) {
                freeBlocks.add(blockNum)
                if (freeBlocks.size >= requiredBlocks) {
                    return freeBlocks
                }
            }
        }

        return if (freeBlocks.size >= requiredBlocks) freeBlocks.take(requiredBlocks) else null
    }

    /**
     * Read the block table for a save to get all blocks it uses.
     * Returns just the list of continuation block numbers (not including the header block).
     */
    private fun readBlockTableForSave(
        rawData: ByteArray,
        headerBlockOffset: Int,
        isInterleaved: Boolean,
        blockSize: Int,
    ): List<Int> {
        val interleavedBlockSize = blockSize * 2
        val blocks = mutableListOf<Int>()

        // Block table starts at offset 0x45 (interleaved) or 0x22 (non-interleaved)
        var cursor = if (isInterleaved) headerBlockOffset + 0x45 else headerBlockOffset + 0x22
        var blocksRead = 0

        // C code (bios.c ReadBlockTable) uses for(;;) with no upper limit.
        // Safety limit based on total blocks in the file.
        val logicalSize = if (isInterleaved) rawData.size / 2 else rawData.size
        val maxBlocks = logicalSize / blockSize
        var count = 0

        while (count < maxBlocks) {
            val blockNum: Int
            if (isInterleaved) {
                if (cursor + 2 >= rawData.size) break
                val high = rawData[cursor].toInt() and 0xFF
                val low = rawData[cursor + 2].toInt() and 0xFF
                blockNum = (high shl 8) or low
            } else {
                if (cursor + 1 >= rawData.size) break
                val high = rawData[cursor].toInt() and 0xFF
                val low = rawData[cursor + 1].toInt() and 0xFF
                blockNum = (high shl 8) or low
            }

            if (blockNum == 0) break // End of block table

            blocks.add(blockNum)
            count++

            // Advance cursor past this entry and check block boundary
            val step = if (isInterleaved) 4 else 2
            val state = advanceCursor(cursor, step, blocks, blocksRead, isInterleaved, blockSize)
            cursor = state.cursor
            blocksRead = state.blocksRead
        }

        return blocks
    }

    /**
     * Read the block table for a save and return the cursor position
     * pointing to the first data byte (after block table + terminator).
     * Based on bios.c ReadBlockTable function.
     *
     * @param rawData The raw backup RAM data
     * @param headerBlockOffset Raw byte offset of the header block
     * @param isInterleaved Whether the format is interleaved
     * @param blockSize The logical block size for this device
     * @return Pair of (block list, CursorState pointing to first data byte), or null on error
     */
    private fun readBlockTableWithCursor(
        rawData: ByteArray,
        headerBlockOffset: Int,
        isInterleaved: Boolean,
        blockSize: Int,
    ): Pair<List<Int>, CursorState>? {
        val blocks = mutableListOf<Int>()

        // Block table starts at offset 0x45 (interleaved) or 0x22 (non-interleaved)
        var cursor = if (isInterleaved) headerBlockOffset + 0x45 else headerBlockOffset + 0x22
        var blocksRead = 0

        // C code (bios.c ReadBlockTable) uses for(;;) with no upper limit.
        // Safety limit based on total blocks in the file.
        val logicalSize = if (isInterleaved) rawData.size / 2 else rawData.size
        val maxBlocks = logicalSize / blockSize
        var count = 0

        // Read block table entries
        while (count < maxBlocks) {
            val blockNum: Int
            if (isInterleaved) {
                if (cursor + 2 >= rawData.size) return null
                val high = rawData[cursor].toInt() and 0xFF
                val low = rawData[cursor + 2].toInt() and 0xFF
                blockNum = (high shl 8) or low
            } else {
                if (cursor + 1 >= rawData.size) return null
                val high = rawData[cursor].toInt() and 0xFF
                val low = rawData[cursor + 1].toInt() and 0xFF
                blockNum = (high shl 8) or low
            }

            if (blockNum == 0) break

            blocks.add(blockNum)
            count++

            // Advance cursor past this entry, check block boundary
            val step = if (isInterleaved) 4 else 2
            val state = advanceCursor(cursor, step, blocks, blocksRead, isInterleaved, blockSize)
            cursor = state.cursor
            blocksRead = state.blocksRead
        }

        // Skip the 0x0000 terminator (4 bytes interleaved, 2 bytes non-interleaved)
        val terminatorStep = if (isInterleaved) 4 else 2
        val afterTerminator = advanceCursor(cursor, terminatorStep, blocks, blocksRead, isInterleaved, blockSize)

        return Pair(blocks, afterTerminator)
    }

    /**
     * Write the save header to a block.
     * Based on bios.c BiosBUPImport around lines 2241-2290.
     */
    private fun writeHeader(
        rawData: ByteArray,
        blockIndex: Int,
        filename: String,
        comment: String,
        language: Int,
        dateRaw: Int,
        dataSize: Int,
        isInterleaved: Boolean,
        blockSize: Int,
    ) {
        val interleavedBlockSize = blockSize * 2
        val blockOffset = if (isInterleaved) {
            blockIndex * interleavedBlockSize
        } else {
            blockIndex * blockSize
        }

        if (isInterleaved) {
            // Set status byte to 0x80 (save is valid)
            rawData[blockOffset + 1] = 0x80.toByte()

            // Write filename (11 bytes at 2-byte intervals starting at 0x09)
            val filenameBytes = encodeMs932(filename, FILENAME_SIZE)
            for (i in 0 until FILENAME_SIZE) {
                rawData[blockOffset + 0x09 + (i * 2)] = filenameBytes[i]
            }

            // Write language at 0x1F
            rawData[blockOffset + 0x1F] = language.toByte()

            // Write comment (10 bytes at 2-byte intervals starting at 0x21)
            val commentBytes = encodeMs932(comment, COMMENT_SIZE)
            for (i in 0 until COMMENT_SIZE) {
                rawData[blockOffset + 0x21 + (i * 2)] = commentBytes[i]
            }

            // Write date (4 bytes big-endian at 0x35, 0x37, 0x39, 0x3B)
            rawData[blockOffset + 0x35] = ((dateRaw shr 24) and 0xFF).toByte()
            rawData[blockOffset + 0x37] = ((dateRaw shr 16) and 0xFF).toByte()
            rawData[blockOffset + 0x39] = ((dateRaw shr 8) and 0xFF).toByte()
            rawData[blockOffset + 0x3B] = (dateRaw and 0xFF).toByte()

            // Write data size (4 bytes big-endian at 0x3D, 0x3F, 0x41, 0x43)
            rawData[blockOffset + 0x3D] = ((dataSize shr 24) and 0xFF).toByte()
            rawData[blockOffset + 0x3F] = ((dataSize shr 16) and 0xFF).toByte()
            rawData[blockOffset + 0x41] = ((dataSize shr 8) and 0xFF).toByte()
            rawData[blockOffset + 0x43] = (dataSize and 0xFF).toByte()
        } else {
            // Non-interleaved format
            rawData[blockOffset] = (rawData[blockOffset].toInt() or 0x80).toByte()

            // Write filename at 0x04
            val filenameBytes = encodeMs932(filename, FILENAME_SIZE)
            for (i in 0 until FILENAME_SIZE) {
                rawData[blockOffset + 0x04 + i] = filenameBytes[i]
            }

            // Write language at 0x0F
            rawData[blockOffset + 0x0F] = language.toByte()

            // Write comment at 0x10
            val commentBytes = encodeMs932(comment, COMMENT_SIZE)
            for (i in 0 until COMMENT_SIZE) {
                rawData[blockOffset + 0x10 + i] = commentBytes[i]
            }

            // Write date (4 bytes contiguous at 0x1A)
            rawData[blockOffset + 0x1A] = ((dateRaw shr 24) and 0xFF).toByte()
            rawData[blockOffset + 0x1B] = ((dateRaw shr 16) and 0xFF).toByte()
            rawData[blockOffset + 0x1C] = ((dateRaw shr 8) and 0xFF).toByte()
            rawData[blockOffset + 0x1D] = (dateRaw and 0xFF).toByte()

            // Write data size (4 bytes contiguous at 0x1E)
            rawData[blockOffset + 0x1E] = ((dataSize shr 24) and 0xFF).toByte()
            rawData[blockOffset + 0x1F] = ((dataSize shr 16) and 0xFF).toByte()
            rawData[blockOffset + 0x20] = ((dataSize shr 8) and 0xFF).toByte()
            rawData[blockOffset + 0x21] = (dataSize and 0xFF).toByte()
        }
    }

    /**
     * Write the block table and data for a multi-block save using cursor-based
     * block chain traversal. Mirrors the bios.c BiosBUPImport algorithm exactly.
     *
     * The continuation block list (blocks[1..]) is written as the block table,
     * followed by a 0x0000 terminator, then the actual save data.
     * Block boundary checks are performed after each write to jump to the
     * next continuation block when needed.
     */
    private fun writeBlockTable(
        rawData: ByteArray,
        blocks: List<Int>,
        isInterleaved: Boolean,
        blockSize: Int,
    ) {
        if (blocks.isEmpty()) return

        val interleavedBlockSize = blockSize * 2
        val headerBlockOffset = if (isInterleaved) {
            blocks[0] * interleavedBlockSize
        } else {
            blocks[0] * blockSize
        }

        // Block table starts at offset 0x45 (interleaved) or 0x22 (non-interleaved)
        var cursor = if (isInterleaved) headerBlockOffset + 0x45 else headerBlockOffset + 0x22
        // blocksWritten tracks which continuation block we jump to next.
        // In bios.c, blockswritten starts at 0 and is incremented BEFORE use:
        //   blockswritten++; workaddr = blocktbl[blockswritten]
        // Our advanceCursor uses blockTable[blocksRead] THEN increments.
        // To get the same effect, we start at 1 (skipping header block at index 0).
        var blocksWritten = 1

        // Write block numbers for continuation blocks (skip first block which is header)
        if (isInterleaved) {
            for (i in 1 until blocks.size) {
                val blockNum = blocks[i]
                rawData[cursor] = ((blockNum shr 8) and 0xFF).toByte()
                rawData[cursor + 2] = (blockNum and 0xFF).toByte()
                val state = advanceCursor(cursor, 4, blocks, blocksWritten, isInterleaved, blockSize)
                cursor = state.cursor
                blocksWritten = state.blocksRead
            }
            // Write end marker (0x0000) - two zero bytes with boundary check between
            rawData[cursor] = 0x00
            val state1 = advanceCursor(cursor, 2, blocks, blocksWritten, isInterleaved, blockSize)
            cursor = state1.cursor
            blocksWritten = state1.blocksRead

            rawData[cursor] = 0x00
            val state2 = advanceCursor(cursor, 2, blocks, blocksWritten, isInterleaved, blockSize)
            cursor = state2.cursor
            blocksWritten = state2.blocksRead
        } else {
            for (i in 1 until blocks.size) {
                val blockNum = blocks[i]
                rawData[cursor] = ((blockNum shr 8) and 0xFF).toByte()
                rawData[cursor + 1] = (blockNum and 0xFF).toByte()
                val state = advanceCursor(cursor, 2, blocks, blocksWritten, isInterleaved, blockSize)
                cursor = state.cursor
                blocksWritten = state.blocksRead
            }
            // Write end marker (0x0000)
            rawData[cursor] = 0x00
            rawData[cursor + 1] = 0x00
            val state = advanceCursor(cursor, 2, blocks, blocksWritten, isInterleaved, blockSize)
            cursor = state.cursor
            blocksWritten = state.blocksRead
        }
    }

    /**
     * Write the save data across blocks using cursor-based block chain traversal.
     * Based on bios.c BiosBUPImport data write loop (lines 2317-2336).
     *
     * This function computes the data start position by replaying the block table
     * write to advance the cursor past block table + terminator, then writes data
     * with block boundary checks.
     */
    private fun writeData(
        rawData: ByteArray,
        blocks: List<Int>,
        data: ByteArray,
        isInterleaved: Boolean,
        blockSize: Int,
    ) {
        if (blocks.isEmpty() || data.isEmpty()) return

        val interleavedBlockSize = blockSize * 2
        val headerBlockOffset = if (isInterleaved) {
            blocks[0] * interleavedBlockSize
        } else {
            blocks[0] * blockSize
        }

        // Replay block table traversal to find the data start cursor
        var cursor = if (isInterleaved) headerBlockOffset + 0x45 else headerBlockOffset + 0x22
        // Start at 1 to skip header block (see writeBlockTable comment for explanation)
        var blocksWritten = 1

        // Advance past block table entries
        for (i in 1 until blocks.size) {
            val step = if (isInterleaved) 4 else 2
            val state = advanceCursor(cursor, step, blocks, blocksWritten, isInterleaved, blockSize)
            cursor = state.cursor
            blocksWritten = state.blocksRead
        }

        // Advance past terminator
        if (isInterleaved) {
            // Two separate 2-byte advances (matching bios.c writing two zero bytes)
            val state1 = advanceCursor(cursor, 2, blocks, blocksWritten, isInterleaved, blockSize)
            cursor = state1.cursor
            blocksWritten = state1.blocksRead

            val state2 = advanceCursor(cursor, 2, blocks, blocksWritten, isInterleaved, blockSize)
            cursor = state2.cursor
            blocksWritten = state2.blocksRead
        } else {
            val state = advanceCursor(cursor, 2, blocks, blocksWritten, isInterleaved, blockSize)
            cursor = state.cursor
            blocksWritten = state.blocksRead
        }

        // Write data bytes with block boundary checks
        val step = if (isInterleaved) 2 else 1
        for (i in data.indices) {
            if (cursor < rawData.size) {
                rawData[cursor] = data[i]
            }

            // Advance cursor for next byte
            if (i < data.size - 1) {
                val state = advanceCursor(cursor, step, blocks, blocksWritten, isInterleaved, blockSize)
                cursor = state.cursor
                blocksWritten = state.blocksRead
            }
        }
    }

    /**
     * Encode a Saturn date from a formatted string.
     * Inverse of decodeSaturnDate.
     */
    private fun encodeSaturnDate(saveDate: String): Int = try {
        // Parse "YYYY/MM/DD HH:mm:ss" format
        val parts = saveDate.split(" ")
        val dateParts = parts[0].split("/")
        val timeParts = if (parts.size > 1) parts[1].split(":") else listOf("0", "0", "0")

        val year = dateParts[0].toIntOrNull() ?: 1980
        val month = dateParts[1].toIntOrNull() ?: 1
        val day = dateParts[2].toIntOrNull() ?: 1
        val hour = timeParts[0].toIntOrNull() ?: 0
        val minute = timeParts[1].toIntOrNull() ?: 0

        // Calculate Saturn date format
        // This is a simplified encoding - may need adjustment based on bios.c
        val yearsSince1980 = (year - 1980).coerceAtLeast(0)
        val fourYearCycles = yearsSince1980 / 4
        val yearInCycle = yearsSince1980 % 4

        // Days in cycle (4 years = 1461 days)
        var totalDays = fourYearCycles * 0x5B5 // 1461 days per 4-year cycle

        // Add days for years within cycle
        when (yearInCycle) {
            1 -> totalDays += 366 // After leap year
            2 -> totalDays += 366 + 365
            3 -> totalDays += 366 + 365 + 365
        }

        // Add days for months
        val isLeapYear = yearInCycle == 0
        val monthDays = intArrayOf(0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334)
        totalDays += monthDays[(month - 1).coerceIn(0, 11)]
        if (isLeapYear && month > 2) totalDays++

        // Add days
        totalDays += day

        // Convert to Saturn format: totalDays * 0x5A0 + hour * 0x3C + minute
        totalDays * 0x5A0 + hour * 0x3C + minute
    } catch (e: Exception) {
        Log.e(TAG, "Error encoding Saturn date: ${e.message}")
        0
    }

    /**
     * Encode a string to MS932 bytes, padded/truncated to the specified length.
     */
    private fun encodeMs932(text: String, length: Int): ByteArray {
        val result = ByteArray(length)
        try {
            val encoded = text.toByteArray(MS932)
            val copyLength = minOf(encoded.size, length)
            System.arraycopy(encoded, 0, result, 0, copyLength)
        } catch (e: Exception) {
            val encoded = text.toByteArray(Charsets.US_ASCII)
            val copyLength = minOf(encoded.size, length)
            System.arraycopy(encoded, 0, result, 0, copyLength)
        }
        return result
    }

    /**
     * Get the raw backup data for a specific item.
     * Used for cloud upload.
     * @param item The backup item
     * @return ByteArray of the backup data, or null if not found
     */
    suspend fun getBackupData(item: BackupItem): ByteArray? = withContext(Dispatchers.IO) {
        val backupFile = getBackupFile(item.deviceType) ?: return@withContext null
        val blockSize = getBlockSize(item.deviceType, backupFile)
        extractBackupData(backupFile, item, blockSize)
    }

    /**
     * Delete the screenshot file associated with a backup item.
     */
    private fun deleteScreenshotFile(filename: String, backupFileKey: String) {
        try {
            val screenshotFile = File("${YabauseStorage.storage.screenshotPath}$backupFileKey/backup_$filename.png")
            if (screenshotFile.exists()) {
                screenshotFile.delete()
                Log.d(TAG, "Deleted screenshot: ${screenshotFile.absolutePath}")
            }
        } catch (e: Exception) {
            Log.w(TAG, "Failed to delete screenshot for $filename: ${e.message}")
        }
    }

    /**
     * Get the backup file path for the specified device type.
     */
    private fun getBackupFile(deviceType: DeviceType): File? = when (deviceType) {
        DeviceType.INTERNAL -> {
            val memoryPath = YabauseStorage.storage.getMemoryPath(INTERNAL_BACKUP_FILE)
            File(memoryPath)
        }
        DeviceType.EXTERNAL -> {
            val memoryPath = YabauseStorage.storage.getMemoryPath(EXTERNAL_BACKUP_FILE)
            File(memoryPath)
        }
        else -> null
    }

    /**
     * Parse the Saturn backup RAM file and extract backup items.
     * Uses raw interleaved format parsing as per bios.c implementation.
     *
     * Saturn backup RAM uses interleaved format where every byte is preceded by 0xFF.
     * Saves are located by scanning blocks and checking if byte at offset +1 has bit 7 set.
     */
    private fun parseBackupRam(file: File, deviceType: DeviceType, blockSize: Int): List<BackupItem> {
        val items = mutableListOf<BackupItem>()
        if (file.length() > MAX_BACKUP_FILE_SIZE) {
            Log.e(TAG, "Backup file too large: ${file.length()} bytes (max $MAX_BACKUP_FILE_SIZE). File may be corrupted: ${file.name}")
            return items
        }
        val rawData = file.readBytes()
        val isInterleavedFile = isInterleaved(file)
        val interleavedBlockSize = blockSize * 2
        val backupFileKey = file.nameWithoutExtension

        Log.d(TAG, "Parsing backup RAM: ${file.name}, interleaved=$isInterleavedFile, raw size=${rawData.size}, blockSize=$blockSize")

        if (!isInterleavedFile) {
            // Non-interleaved format is not common for Yabause, but handle it
            Log.d(TAG, "Non-interleaved format, using de-interleaved parsing")
            return parseNonInterleavedBackupRam(rawData, deviceType, blockSize, backupFileKey)
        }

        // For interleaved format, file size must be at least 2 header blocks
        val minSize = 2 * interleavedBlockSize
        if (rawData.size < minSize) {
            Log.w(TAG, "Backup file too small: ${rawData.size}")
            return emptyList()
        }

        // Verify header (interleaved: 0xFF, 'B', 0xFF, 'a', ... for "BackUpRam Format")
        val headerCheck = StringBuilder()
        for (i in BACKUP_MAGIC.indices) {
            headerCheck.append(rawData[i * 2 + 1].toInt().toChar())
        }
        if (!headerCheck.toString().startsWith("BackUpRam")) {
            Log.w(TAG, "Invalid backup RAM header: $headerCheck")
            return emptyList()
        }

        // Calculate logical size and total blocks
        val logicalSize = rawData.size / 2
        val totalBlocks = logicalSize / blockSize

        Log.d(TAG, "Logical size: $logicalSize, total blocks: $totalBlocks")

        var index = 0

        // Start from block 2 (first 2 blocks are header)
        for (blockNum in 2 until totalBlocks) {
            val blockOffset = blockNum * interleavedBlockSize // Raw offset in interleaved data

            // Check if this block starts a save
            // In bios.c: if (((s8)MappedMemoryReadByte(addr + i + 1, NULL)) < 0)
            // This checks if the byte at offset +1 (2nd byte of block) has bit 7 set
            val statusByte = rawData[blockOffset + 1].toInt()
            if (statusByte and 0x80 != 0) {
                // This is the start of a save entry
                val item = parseInterleavedEntry(rawData, blockOffset, index, deviceType, blockSize, backupFileKey)
                if (item != null) {
                    items.add(item)
                    Log.d(TAG, "Found save entry: ${item.filename}, comment=${item.comment}")
                    index++
                }
            }
        }

        Log.d(TAG, "Found ${items.size} backup items")
        return items
    }

    /**
     * Parse a save entry from interleaved raw data.
     * Offsets are as per bios.c (raw interleaved offsets from block start):
     * - 0x09: filename (11 bytes, every 2 bytes)
     * - 0x1F: language
     * - 0x21: comment (10 bytes, every 2 bytes)
     * - 0x35, 0x37, 0x39, 0x3B: date (big-endian)
     * - 0x3D, 0x3F, 0x41, 0x43: data size (big-endian)
     */
    private fun parseInterleavedEntry(
        rawData: ByteArray,
        blockOffset: Int,
        index: Int,
        deviceType: DeviceType,
        blockSize: Int,
        backupFileKey: String,
    ): BackupItem? {
        try {
            // Read filename (offset 0x09, 11 bytes at 2-byte intervals)
            val filenameBytes = ByteArray(FILENAME_SIZE)
            for (i in 0 until FILENAME_SIZE) {
                val byteOffset = blockOffset + 0x09 + (i * 2)
                if (byteOffset < rawData.size) {
                    filenameBytes[i] = rawData[byteOffset]
                }
            }
            val filename = decodeMs932(filenameBytes).trim { it <= ' ' || it == '\u0000' }

            if (filename.isEmpty()) {
                return null
            }

            // Read language (offset 0x1F)
            val language = if (blockOffset + 0x1F < rawData.size) {
                rawData[blockOffset + 0x1F].toInt() and 0xFF
            } else {
                0
            }

            // Read comment (offset 0x21, 10 bytes at 2-byte intervals)
            val commentBytes = ByteArray(COMMENT_SIZE)
            for (i in 0 until COMMENT_SIZE) {
                val byteOffset = blockOffset + 0x21 + (i * 2)
                if (byteOffset < rawData.size) {
                    commentBytes[i] = rawData[byteOffset]
                }
            }
            val comment = decodeMs932(commentBytes).trim { it <= ' ' || it == '\u0000' }

            // Read date (4 bytes at offsets 0x35, 0x37, 0x39, 0x3B - big-endian)
            val dateRaw = readBigEndianInt(rawData, blockOffset, intArrayOf(0x35, 0x37, 0x39, 0x3B))
            val (year, month, day, hour, minute) = decodeSaturnDate(dateRaw)

            val saveDate = String.format(
                "%04d/%02d/%02d %02d:%02d:00",
                year,
                month,
                day,
                hour,
                minute,
            )

            // Read data size (4 bytes at offsets 0x3D, 0x3F, 0x41, 0x43 - big-endian)
            val dataSize = readBigEndianInt(rawData, blockOffset, intArrayOf(0x3D, 0x3F, 0x41, 0x43))

            // Calculate block size: (dataSize + 0x1D) / (blockSize - 6) + 1
            // Based on bios.c CalcSaveSize logic
            val saveBlockCount = if (dataSize > 0) {
                (dataSize + 0x1D) / (blockSize - 6) + 1
            } else {
                1
            }

            val screenshotFile = File("${YabauseStorage.storage.screenshotPath}$backupFileKey/backup_$filename.png")
            val screenshotPath = if (screenshotFile.exists()) screenshotFile.absolutePath else null

            val (gameTitle, productNumber) = lookupGameInfo(filename, backupFileKey)

            return BackupItem(
                id = index.toString(),
                filename = filename,
                comment = comment,
                language = language,
                saveDate = saveDate,
                dataSize = dataSize,
                blockSize = saveBlockCount,
                deviceType = deviceType,
                screenshotUrl = screenshotPath,
                gameTitle = gameTitle,
                productNumber = productNumber,
                backupFileKey = backupFileKey,
            )
        } catch (e: Exception) {
            Log.e(TAG, "Error parsing interleaved entry at offset $blockOffset: ${e.message}", e)
            return null
        }
    }

    /**
     * Read a big-endian 32-bit integer from non-contiguous offsets in raw data.
     */
    private fun readBigEndianInt(data: ByteArray, baseOffset: Int, offsets: IntArray): Int {
        var result = 0
        for (i in offsets.indices) {
            val byteOffset = baseOffset + offsets[i]
            val byteValue = if (byteOffset < data.size) data[byteOffset].toInt() and 0xFF else 0
            result = result or (byteValue shl (24 - i * 8))
        }
        return result
    }

    /**
     * Look up game title and product number from SharedPreferences.
     * These are saved by Yabause.onBackupWrite() when a game writes to backup RAM.
     */
    private fun lookupGameInfo(filename: String, backupFileKey: String): Pair<String?, String?> {
        val prefs = context.getSharedPreferences("backup_game_info", Context.MODE_PRIVATE)
        val title = prefs.getString("title_${backupFileKey}_$filename", null)
        val product = prefs.getString("product_${backupFileKey}_$filename", null)
        return Pair(title, product)
    }

    /**
     * Decode Saturn date format to year, month, day, hour, minute.
     * Based on bios.c lines 1946-1973.
     */
    private fun decodeSaturnDate(dateRaw: Int): DateParts {
        // Time calculation
        val hour = (dateRaw % 0x5A0) / 0x3C
        val minute = dateRaw % 0x3C

        // Date calculation
        val div = dateRaw / 0x5A0
        val yearRemainder = div % 0x5B5

        val (yearOffset, dayOfYear) = if (yearRemainder > 0x16E) {
            val offset = (yearRemainder - 1) / 0x16D
            val doy = (yearRemainder - 1) % 0x16D
            Pair(offset, doy)
        } else {
            Pair(0, if (yearRemainder > 0) yearRemainder - 1 else 0)
        }

        val year = 1980 + ((div / 0x5B5) * 4) + yearOffset
        val isLeapYear = yearOffset == 0
        val (month, day) = convertMonthAndDay(dayOfYear, isLeapYear)

        return DateParts(year, month, day, hour, minute)
    }

    /**
     * Convert day-of-year to month and day.
     * Based on ConvertMonthAndDayMem in bios.c.
     */
    private fun convertMonthAndDay(dayOfYear: Int, isLeapYear: Boolean): Pair<Int, Int> {
        // Month boundary table (cumulative days, standard year)
        val monthTable = intArrayOf(
            31, // Jan
            31 + 28, // Feb (non-leap)
            31 + 28 + 31, // Mar
            31 + 28 + 31 + 30, // Apr
            31 + 28 + 31 + 30 + 31, // May
            31 + 28 + 31 + 30 + 31 + 30, // Jun
            31 + 28 + 31 + 30 + 31 + 30 + 31, // Jul
            31 + 28 + 31 + 30 + 31 + 30 + 31 + 31, // Aug
            31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30, // Sep
            31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31, // Oct
            31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30, // Nov
        )

        // January
        if (dayOfYear < monthTable[0]) {
            return Pair(1, dayOfYear + 1)
        }

        // Find month
        var monthIndex = 1
        for (i in 1 until 11) {
            if (dayOfYear <= monthTable[i]) {
                break
            }
            monthIndex = i + 1
        }

        val month = monthIndex + 1
        val day = if (isLeapYear && month == 2) {
            // Leap year February adjustment
            dayOfYear - monthTable[monthIndex - 1] + 1
        } else {
            dayOfYear - monthTable[monthIndex - 1] + 1
        }

        return Pair(month, day.coerceAtLeast(1))
    }

    /**
     * Parse non-interleaved backup RAM (fallback for non-standard format).
     */
    private fun parseNonInterleavedBackupRam(
        data: ByteArray,
        deviceType: DeviceType,
        blockSize: Int,
        backupFileKey: String,
    ): List<BackupItem> {
        val items = mutableListOf<BackupItem>()

        if (data.size < blockSize * 2) {
            return emptyList()
        }

        // Verify header
        val headerBytes = data.copyOfRange(0, BACKUP_MAGIC.length)
        val header = String(headerBytes, Charsets.US_ASCII)

        if (!header.startsWith("BackUpRam")) {
            Log.w(TAG, "Invalid non-interleaved backup RAM header: $header")
            return emptyList()
        }

        // For non-interleaved, process blocks directly
        val totalBlocks = data.size / blockSize
        var index = 0

        for (blockNum in 2 until totalBlocks) {
            val blockOffset = blockNum * blockSize

            // Check if block starts a save (first byte has bit 7 set)
            val statusByte = data[blockOffset].toInt()
            if (statusByte and 0x80 != 0) {
                val item = parseNonInterleavedEntry(data, blockOffset, index, deviceType, blockSize, backupFileKey)
                if (item != null) {
                    items.add(item)
                    index++
                }
            }
        }

        return items
    }

    /**
     * Parse a save entry from non-interleaved data.
     */
    private fun parseNonInterleavedEntry(
        data: ByteArray,
        blockOffset: Int,
        index: Int,
        deviceType: DeviceType,
        blockSize: Int,
        backupFileKey: String,
    ): BackupItem? {
        try {
            // Non-interleaved offsets are half of interleaved offsets
            // Filename: offset 0x04 (11 bytes contiguous)
            val filenameBytes = ByteArray(FILENAME_SIZE)
            for (i in 0 until FILENAME_SIZE) {
                val byteOffset = blockOffset + 0x04 + i
                if (byteOffset < data.size) {
                    filenameBytes[i] = data[byteOffset]
                }
            }
            val filename = decodeMs932(filenameBytes).trim { it <= ' ' || it == '\u0000' }

            if (filename.isEmpty()) {
                return null
            }

            // Language: offset 0x0F
            val language = if (blockOffset + 0x0F < data.size) {
                data[blockOffset + 0x0F].toInt() and 0xFF
            } else {
                0
            }

            // Comment: offset 0x10 (10 bytes contiguous)
            val commentBytes = ByteArray(COMMENT_SIZE)
            for (i in 0 until COMMENT_SIZE) {
                val byteOffset = blockOffset + 0x10 + i
                if (byteOffset < data.size) {
                    commentBytes[i] = data[byteOffset]
                }
            }
            val comment = decodeMs932(commentBytes).trim { it <= ' ' || it == '\u0000' }

            // Date: offset 0x1A (4 bytes contiguous)
            val dateRaw = if (blockOffset + 0x1D < data.size) {
                ((data[blockOffset + 0x1A].toInt() and 0xFF) shl 24) or
                    ((data[blockOffset + 0x1B].toInt() and 0xFF) shl 16) or
                    ((data[blockOffset + 0x1C].toInt() and 0xFF) shl 8) or
                    (data[blockOffset + 0x1D].toInt() and 0xFF)
            } else {
                0
            }
            val (year, month, day, hour, minute) = decodeSaturnDate(dateRaw)

            val saveDate = String.format(
                "%04d/%02d/%02d %02d:%02d:00",
                year,
                month,
                day,
                hour,
                minute,
            )

            // Data size: offset 0x1E (4 bytes contiguous)
            val dataSize = if (blockOffset + 0x21 < data.size) {
                ((data[blockOffset + 0x1E].toInt() and 0xFF) shl 24) or
                    ((data[blockOffset + 0x1F].toInt() and 0xFF) shl 16) or
                    ((data[blockOffset + 0x20].toInt() and 0xFF) shl 8) or
                    (data[blockOffset + 0x21].toInt() and 0xFF)
            } else {
                0
            }

            val saveBlockCount = if (dataSize > 0) {
                (dataSize + 0x1D) / (blockSize - 6) + 1
            } else {
                1
            }

            val screenshotFile = File("${YabauseStorage.storage.screenshotPath}$backupFileKey/backup_$filename.png")
            val screenshotPath = if (screenshotFile.exists()) screenshotFile.absolutePath else null

            val (gameTitle, productNumber) = lookupGameInfo(filename, backupFileKey)

            return BackupItem(
                id = index.toString(),
                filename = filename,
                comment = comment,
                language = language,
                saveDate = saveDate,
                dataSize = dataSize,
                blockSize = saveBlockCount,
                deviceType = deviceType,
                screenshotUrl = screenshotPath,
                gameTitle = gameTitle,
                productNumber = productNumber,
                backupFileKey = backupFileKey,
            )
        } catch (e: Exception) {
            Log.e(TAG, "Error parsing non-interleaved entry: ${e.message}", e)
            return null
        }
    }

    /**
     * Data class for decoded date parts.
     */
    private data class DateParts(
        val year: Int,
        val month: Int,
        val day: Int,
        val hour: Int,
        val minute: Int,
    )

    /**
     * Parse storage status from backup RAM file.
     * Uses bios.c GetDeviceStats / BiosBUPStatus logic for correct capacity.
     *
     * @param file The backup RAM file on disk
     * @param biosSize The logical size from GetDeviceStats (e.g. 0x8000 for MEMORY_FILEBIOS)
     * @param biosBlockSize The block size from GetDeviceStats (e.g. 0x40 for internal memory)
     * @return Pair of (totalSize, freeSize) in bytes, matching BiosBUPStatus formula
     */
    private fun parseStorageStatus(file: File, biosSize: Int, biosBlockSize: Int): Pair<Int, Int> {
        val rawData = file.readBytes()
        val isInterleavedFile = isInterleaved(file)

        // BiosBUPStatus formula from bios.c:918-923:
        //   totalsize  = size
        //   totalblock = size / blocksize
        //   freeblocks = totalblock - 2 - usedblocks
        //   freesize   = ((blocksize - 6) * freeblocks) - 30
        val totalSize = biosSize
        val totalBlocks = biosSize / biosBlockSize

        if (totalBlocks < 2) {
            return Pair(0, 0)
        }

        // Count used blocks by scanning the interleaved memory region.
        // GetFreeSpace in bios.c scans from block 2 to biosSize boundary (NOT file size).
        // Step size in raw bytes: biosBlockSize * 2 (interleaved) or biosBlockSize (non-interleaved)
        var usedBlocks = 0
        val scanBlocks = totalBlocks.coerceAtMost(
            if (isInterleavedFile) rawData.size / (biosBlockSize * 2) else rawData.size / biosBlockSize,
        )

        if (isInterleavedFile) {
            for (blockNum in 2 until scanBlocks) {
                val blockOffset = blockNum * biosBlockSize * 2
                if (blockOffset + 1 >= rawData.size) break

                val statusByte = rawData[blockOffset + 1].toInt()
                if (statusByte and 0x80 != 0) {
                    // Found a save header, read its data size to calculate blocks used
                    val dataSize = readBigEndianInt(rawData, blockOffset, intArrayOf(0x3D, 0x3F, 0x41, 0x43))
                    val saveBlockSize = if (dataSize > 0) {
                        (dataSize + 0x1D) / (biosBlockSize - 6) + 1
                    } else {
                        1
                    }
                    usedBlocks += saveBlockSize
                }
            }
        } else {
            for (blockNum in 2 until scanBlocks) {
                val blockOffset = blockNum * biosBlockSize
                if (blockOffset >= rawData.size) break

                val statusByte = rawData[blockOffset].toInt()
                if (statusByte and 0x80 != 0) {
                    val dataSize = if (blockOffset + 0x21 < rawData.size) {
                        ((rawData[blockOffset + 0x1E].toInt() and 0xFF) shl 24) or
                            ((rawData[blockOffset + 0x1F].toInt() and 0xFF) shl 16) or
                            ((rawData[blockOffset + 0x20].toInt() and 0xFF) shl 8) or
                            (rawData[blockOffset + 0x21].toInt() and 0xFF)
                    } else {
                        0
                    }
                    val saveBlockSize = if (dataSize > 0) {
                        (dataSize + 0x1D) / (biosBlockSize - 6) + 1
                    } else {
                        1
                    }
                    usedBlocks += saveBlockSize
                }
            }
        }

        val freeBlocks = (totalBlocks - 2 - usedBlocks).coerceAtLeast(0)
        // BiosBUPStatus formula: freesize = ((blocksize - 6) * freeblocks) - 30
        val freeSize = if (freeBlocks > 0) {
            ((biosBlockSize - 6) * freeBlocks - 30).coerceAtLeast(0)
        } else {
            0
        }

        return Pair(totalSize, freeSize)
    }

    /**
     * Extract raw backup data for a specific item.
     * Supports both standard and interleaved formats.
     *
     * Uses cursor-based block chain traversal matching the bios.c
     * BiosBUPExport algorithm to correctly handle multi-block saves.
     *
     * @param file The backup RAM file
     * @param item The backup item to extract
     * @param blockSize The logical block size for this device
     */
    private fun extractBackupData(file: File, item: BackupItem, blockSize: Int): ByteArray? {
        try {
            if (file.length() > MAX_BACKUP_FILE_SIZE) {
                Log.e(TAG, "Backup file too large: ${file.length()} bytes (max $MAX_BACKUP_FILE_SIZE). File may be corrupted: ${file.name}")
                return null
            }
            val rawData = file.readBytes()
            val isInterleavedFile = isInterleaved(file)

            // 1. Find the header block for this save
            val headerBlockOffset = findSaveHeaderBlockOffset(rawData, item.filename, isInterleavedFile, blockSize)
            if (headerBlockOffset == null) {
                Log.w(TAG, "Could not find save header for: ${item.filename}")
                return null
            }

            // 2. Read block table and get cursor pointing to first data byte
            val tableResult = readBlockTableWithCursor(rawData, headerBlockOffset, isInterleavedFile, blockSize)
            if (tableResult == null) {
                Log.e(TAG, "Failed to read block table for: ${item.filename}")
                return null
            }

            val (blockTable, dataCursorState) = tableResult
            var cursor = dataCursorState.cursor
            var blocksRead = dataCursorState.blocksRead

            // 3. Read data bytes using cursor with block boundary checks
            val result = ByteArray(item.dataSize)
            val step = if (isInterleavedFile) 2 else 1

            for (i in 0 until item.dataSize) {
                if (cursor >= rawData.size) {
                    Log.e(TAG, "Cursor out of bounds at byte $i/${item.dataSize} for: ${item.filename}")
                    return null
                }
                result[i] = rawData[cursor]

                // Advance cursor to next data byte
                if (i < item.dataSize - 1) {
                    val state = advanceCursor(cursor, step, blockTable, blocksRead, isInterleavedFile, blockSize)
                    cursor = state.cursor
                    blocksRead = state.blocksRead
                }
            }

            return result
        } catch (e: Exception) {
            Log.e(TAG, "Error extracting backup data: ${e.message}", e)
            return null
        }
    }

    /**
     * Decode MS932 (Shift-JIS) encoded bytes to String.
     */
    private fun decodeMs932(bytes: ByteArray): String = try {
        String(bytes, MS932)
    } catch (e: Exception) {
        String(bytes, Charsets.US_ASCII)
    }

    /**
     * Get all memory files from YabauseStorage.
     * @return List of memory file names
     */
    fun getMemoryFiles(): Array<String> = try {
        YabauseStorage.storage.memoryFiles
    } catch (e: Exception) {
        Log.e(TAG, "Error getting memory files: ${e.message}", e)
        emptyArray()
    }

    /**
     * Check if internal backup RAM file exists.
     */
    fun hasInternalBackup(): Boolean = getBackupFile(DeviceType.INTERNAL)?.exists() == true

    /**
     * Check if external backup RAM file exists.
     */
    fun hasExternalBackup(): Boolean = getBackupFile(DeviceType.EXTERNAL)?.exists() == true

    /**
     * Parse backup items from a file path directly.
     * This method is useful for testing with arbitrary backup files.
     * @param filePath The path to the backup RAM file
     * @param deviceType The device type to assign to the items
     * @return List of BackupItem objects, or empty list if parsing fails
     */
    suspend fun parseBackupFile(filePath: String, deviceType: DeviceType = DeviceType.INTERNAL): List<BackupItem> =
        withContext(Dispatchers.IO) {
            val file = File(filePath)
            if (!file.exists()) {
                Log.w(TAG, "Backup file does not exist: $filePath")
                return@withContext emptyList()
            }

            try {
                val blockSize = getBlockSize(deviceType, file)
                parseBackupRam(file, deviceType, blockSize)
            } catch (e: Exception) {
                Log.e(TAG, "Error parsing backup file: ${e.message}", e)
                emptyList()
            }
        }

    /**
     * Extract backup data from a file path directly.
     * This method is useful for testing with arbitrary backup files.
     * @param filePath The path to the backup RAM file
     * @param item The backup item to extract
     * @return ByteArray of the backup data, or null if not found
     */
    suspend fun extractBackupDataFromFile(filePath: String, item: BackupItem): ByteArray? =
        withContext(Dispatchers.IO) {
            val file = File(filePath)
            if (!file.exists()) {
                return@withContext null
            }

            try {
                val blockSize = getBlockSize(item.deviceType, file)
                extractBackupData(file, item, blockSize)
            } catch (e: Exception) {
                Log.e(TAG, "Error extracting backup data: ${e.message}", e)
                null
            }
        }

    /**
     * Check if a backup with the given filename already exists in a backup file.
     * @param filePath The path to the backup RAM file
     * @param filename The save filename to check
     * @param deviceType The device type (used to determine block size)
     * @return true if a backup with the same filename exists, false otherwise
     */
    suspend fun backupExistsInFile(filePath: String, filename: String, deviceType: DeviceType): Boolean =
        withContext(Dispatchers.IO) {
            val file = File(filePath)
            if (!file.exists()) return@withContext false

            try {
                val rawData = file.readBytes()
                val isInterleavedFile = isInterleaved(file)
                val blockSize = getBlockSize(deviceType, file)
                findSaveBlockIndex(rawData, filename, isInterleavedFile, blockSize) != null
            } catch (e: Exception) {
                Log.e(TAG, "Error checking backup existence: ${e.message}", e)
                false
            }
        }

    /**
     * Import backup data to a file path directly.
     * This method is useful for testing with arbitrary backup files.
     * @param filePath The path to the backup RAM file
     * @param filename The save filename
     * @param comment The save comment
     * @param language The language code
     * @param dateRaw The Saturn format date value
     * @param data The save data bytes
     * @param deviceType The device type (used to determine block size)
     * @return true if successful, false otherwise
     */
    suspend fun importBackupDataToFile(
        filePath: String,
        filename: String,
        comment: String,
        language: Int,
        dateRaw: Int,
        data: ByteArray,
        deviceType: DeviceType = DeviceType.INTERNAL,
    ): Boolean = withContext(Dispatchers.IO) {
        val file = File(filePath)
        if (!file.exists()) {
            return@withContext false
        }

        try {
            val rawData = file.readBytes().copyOf()
            val isInterleavedFile = isInterleaved(file)
            val blockSize = getBlockSize(deviceType, file)

            // Delete existing save with the same filename
            val existingBlock = findSaveBlockIndex(rawData, filename, isInterleavedFile, blockSize)
            if (existingBlock != null) {
                deleteSaveAtBlock(rawData, existingBlock, isInterleavedFile, blockSize)
            }

            val dataSize = data.size
            val saveSize = calculateRequiredBlocks(dataSize, blockSize)

            val freeBlocks = getFreeBlocks(rawData, saveSize, isInterleavedFile, blockSize)
            if (freeBlocks == null) {
                Log.e(TAG, "Not enough free space for save")
                return@withContext false
            }

            writeHeader(
                rawData,
                freeBlocks[0],
                filename,
                comment,
                language,
                dateRaw,
                dataSize,
                isInterleavedFile,
                blockSize,
            )
            writeBlockTable(rawData, freeBlocks, isInterleavedFile, blockSize)
            writeData(rawData, freeBlocks, data, isInterleavedFile, blockSize)

            file.writeBytes(rawData)
            true
        } catch (e: Exception) {
            Log.e(TAG, "Error importing backup data: ${e.message}", e)
            false
        }
    }

    /**
     * Delete a backup item from a specific file path.
     * @param filePath The path to the backup RAM file
     * @param item The backup item to delete
     * @return true if successful, false otherwise
     */
    suspend fun deleteBackupFromFile(filePath: String, item: BackupItem): Boolean = withContext(Dispatchers.IO) {
        val file = File(filePath)
        if (!file.exists()) return@withContext false

        try {
            val rawData = file.readBytes()
            val isInterleavedFile = isInterleaved(file)
            val blockSize = getBlockSize(item.deviceType, file)
            val interleavedBlockSize = blockSize * 2

            val blockIndex = findSaveBlockIndex(rawData, item.filename, isInterleavedFile, blockSize)
            if (blockIndex == null) {
                Log.e(TAG, "Could not find save block for: ${item.filename}")
                return@withContext false
            }

            Log.d(TAG, "Deleting backup '${item.filename}' at block $blockIndex from $filePath")

            if (isInterleavedFile) {
                val statusByteOffset = blockIndex * interleavedBlockSize + 1
                rawData[statusByteOffset] = 0x00
            } else {
                val statusByteOffset = blockIndex * blockSize
                rawData[statusByteOffset] = (rawData[statusByteOffset].toInt() and 0x7F).toByte()
            }

            file.writeBytes(rawData)
            Log.d(TAG, "Successfully deleted backup: ${item.filename}")

            // Delete associated screenshot file
            deleteScreenshotFile(item.filename, item.backupFileKey)

            true
        } catch (e: Exception) {
            Log.e(TAG, "Error deleting backup: ${e.message}", e)
            false
        }
    }

    /**
     * Export a backup item from a specific file path to an external URI.
     * @param filePath The path to the backup RAM file
     * @param item The backup item to export
     * @param destinationUri The destination URI
     * @return true if successful, false otherwise
     */
    suspend fun exportBackupFromFile(filePath: String, item: BackupItem, destinationUri: Uri): Boolean =
        withContext(Dispatchers.IO) {
            try {
                val file = File(filePath)
                if (!file.exists()) return@withContext false

                val blockSize = getBlockSize(item.deviceType, file)
                val backupData = extractBackupData(file, item, blockSize)
                if (backupData == null) {
                    Log.e(TAG, "Failed to extract backup data for: ${item.filename}")
                    return@withContext false
                }

                context.contentResolver.openOutputStream(destinationUri)?.use { output ->
                    output.write(backupData)
                }
                true
            } catch (e: Exception) {
                Log.e(TAG, "Error exporting backup: ${e.message}", e)
                false
            }
        }

    /**
     * Get storage status from a file path directly.
     * This method is useful for testing with arbitrary backup files.
     * @param filePath The path to the backup RAM file
     * @param biosSize The logical size from GetDeviceStats
     * @param biosBlockSize The block size from GetDeviceStats
     * @return Pair of (totalSize, freeSize) in bytes
     */
    suspend fun getStorageStatusFromFile(filePath: String, biosSize: Int, biosBlockSize: Int): Pair<Int, Int> =
        withContext(Dispatchers.IO) {
            val file = File(filePath)
            if (!file.exists()) {
                return@withContext Pair(0, 0)
            }

            try {
                parseStorageStatus(file, biosSize, biosBlockSize)
            } catch (e: Exception) {
                Log.e(TAG, "Error getting storage status: ${e.message}", e)
                Pair(0, 0)
            }
        }
}
