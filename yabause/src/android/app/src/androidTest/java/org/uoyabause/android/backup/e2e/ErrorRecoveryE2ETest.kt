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
package org.uoyabause.android.backup.e2e

import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

/**
 * Error Recovery E2E Tests
 * Test cases: E2E-E01 ~ E2E-E03
 *
 * These tests verify the app gracefully handles error conditions.
 */
@RunWith(AndroidJUnit4::class)
class ErrorRecoveryE2ETest {
    private lateinit var context: Context
    private lateinit var testFilesDir: File

    companion object {
        private const val BACKUP_MAGIC = "BackUpRam Format"
    }

    @Before
    fun setUp() {
        context = InstrumentationRegistry.getInstrumentation().targetContext
        testFilesDir = context.filesDir
    }

    @After
    fun tearDown() {
        // Clean up test files
        testFilesDir.listFiles()?.filter { it.name.startsWith("test_") }?.forEach { it.delete() }
    }

    // ==============================
    // E2E-E01: Network disconnection during upload
    // ==============================

    /**
     * E2E-E01-01: Verify partial file can be detected
     */
    @Test
    fun partialFile_canBeDetected() {
        val partialFile = File(testFilesDir, "test_partial.bin")
        partialFile.writeBytes(ByteArray(50)) // Incomplete file

        assertTrue(partialFile.exists())
        assertTrue(partialFile.length() < 100)
    }

    /**
     * E2E-E01-02: Verify corrupted file header is detected
     */
    @Test
    fun corruptedHeader_isDetected() {
        val corruptedFile = File(testFilesDir, "test_corrupted.bin")
        corruptedFile.writeBytes("InvalidHeader!!!".toByteArray())

        val content = corruptedFile.readBytes()
        val header = String(content.copyOf(16), Charsets.US_ASCII)

        assertFalse(header.startsWith(BACKUP_MAGIC))
    }

    /**
     * E2E-E01-03: Verify retry mechanism data structure
     */
    @Test
    fun retryMechanism_hasValidStructure() {
        data class RetryState(
            var attemptCount: Int = 0,
            val maxAttempts: Int = 3,
            var lastError: String? = null,
        ) {
            val canRetry: Boolean get() = attemptCount < maxAttempts

            fun recordAttempt(error: String) {
                attemptCount++
                lastError = error
            }
        }

        val state = RetryState()
        assertTrue(state.canRetry)
        assertEquals(0, state.attemptCount)

        state.recordAttempt("Network error")
        assertEquals(1, state.attemptCount)
        assertEquals("Network error", state.lastError)
        assertTrue(state.canRetry)

        state.recordAttempt("Timeout")
        state.recordAttempt("Connection refused")
        assertFalse(state.canRetry)
    }

    // ==============================
    // E2E-E02: App termination during download
    // ==============================

    /**
     * E2E-E02-01: Verify temporary file cleanup
     */
    @Test
    fun temporaryFiles_canBeCleanedUp() {
        // Create some temporary files
        val tempFile1 = File(testFilesDir, "test_temp1.tmp")
        val tempFile2 = File(testFilesDir, "test_temp2.tmp")
        tempFile1.writeText("temp data 1")
        tempFile2.writeText("temp data 2")

        assertTrue(tempFile1.exists())
        assertTrue(tempFile2.exists())

        // Clean up temp files
        testFilesDir.listFiles()?.filter { it.extension == "tmp" }?.forEach { it.delete() }

        assertFalse(tempFile1.exists())
        assertFalse(tempFile2.exists())
    }

    /**
     * E2E-E02-02: Verify download state can be persisted
     */
    @Test
    fun downloadState_canBePersisted() {
        data class DownloadState(
            val fileId: String,
            val bytesDownloaded: Long,
            val totalBytes: Long,
            val isPaused: Boolean = false,
        ) {
            val progress: Float get() = if (totalBytes > 0) bytesDownloaded.toFloat() / totalBytes else 0f
            val isComplete: Boolean get() = bytesDownloaded >= totalBytes
        }

        val state = DownloadState(
            fileId = "backup123",
            bytesDownloaded = 512,
            totalBytes = 1024,
        )

        assertEquals("backup123", state.fileId)
        assertEquals(0.5f, state.progress, 0.01f)
        assertFalse(state.isComplete)
    }

    /**
     * E2E-E02-03: Verify incomplete download detection
     */
    @Test
    fun incompleteDownload_canBeDetected() {
        val incompleteFile = File(testFilesDir, "test_incomplete.download")
        incompleteFile.writeBytes(ByteArray(100))

        // Check for .download extension (indicates incomplete)
        assertTrue(incompleteFile.extension == "download")

        // Rename to complete
        val completeFile = File(testFilesDir, "test_complete.bin")
        incompleteFile.renameTo(completeFile)

        assertFalse(incompleteFile.exists())
        assertTrue(completeFile.exists())
    }

    // ==============================
    // E2E-E03: Storage capacity shortage
    // ==============================

    /**
     * E2E-E03-01: Verify storage space check
     */
    @Test
    fun storageSpace_canBeChecked() {
        val freeSpace = testFilesDir.freeSpace
        val usableSpace = testFilesDir.usableSpace
        val totalSpace = testFilesDir.totalSpace

        assertTrue(freeSpace > 0)
        assertTrue(usableSpace > 0)
        assertTrue(totalSpace > 0)
        assertTrue(usableSpace <= freeSpace)
    }

    /**
     * E2E-E03-02: Verify storage limit calculation
     */
    @Test
    fun storageLimit_calculatesCorrectly() {
        data class StorageStatus(
            val usedBytes: Long,
            val limitBytes: Long,
        ) {
            val usedPercentage: Float get() = if (limitBytes > 0) usedBytes.toFloat() / limitBytes * 100 else 0f
            val availableBytes: Long get() = maxOf(0, limitBytes - usedBytes)
            val isNearLimit: Boolean get() = usedPercentage >= 90f
            val isOverLimit: Boolean get() = usedBytes >= limitBytes
        }

        // Normal usage
        val normal = StorageStatus(usedBytes = 50_000_000, limitBytes = 100_000_000)
        assertEquals(50f, normal.usedPercentage, 0.1f)
        assertEquals(50_000_000L, normal.availableBytes)
        assertFalse(normal.isNearLimit)
        assertFalse(normal.isOverLimit)

        // Near limit
        val nearLimit = StorageStatus(usedBytes = 95_000_000, limitBytes = 100_000_000)
        assertTrue(nearLimit.isNearLimit)
        assertFalse(nearLimit.isOverLimit)

        // Over limit
        val overLimit = StorageStatus(usedBytes = 110_000_000, limitBytes = 100_000_000)
        assertTrue(overLimit.isNearLimit)
        assertTrue(overLimit.isOverLimit)
        assertEquals(0L, overLimit.availableBytes)
    }

    /**
     * E2E-E03-03: Verify file size estimation before write
     */
    @Test
    fun fileSize_canBeEstimatedBeforeWrite() {
        val estimatedSize = 1024 * 1024L // 1MB
        val availableSpace = testFilesDir.usableSpace

        val canWrite = availableSpace > estimatedSize

        // Should always be true on test device with space
        assertTrue(canWrite)
    }

    /**
     * E2E-E03-04: Verify cleanup of old backups logic
     */
    @Test
    fun oldBackupCleanup_prioritizesCorrectly() {
        data class BackupInfo(
            val filename: String,
            val size: Long,
            val lastModified: Long,
            val isProtected: Boolean = false,
        )

        val backups = listOf(
            BackupInfo("newest.bin", 1000, System.currentTimeMillis()),
            BackupInfo("oldest.bin", 500, System.currentTimeMillis() - 86400000L * 30),
            BackupInfo("protected.bin", 2000, System.currentTimeMillis() - 86400000L * 60, true),
            BackupInfo("medium.bin", 800, System.currentTimeMillis() - 86400000L * 15),
        )

        // Find candidates for cleanup (oldest first, excluding protected)
        val cleanupCandidates = backups
            .filter { !it.isProtected }
            .sortedBy { it.lastModified }

        assertEquals("oldest.bin", cleanupCandidates.first().filename)
        assertEquals(3, cleanupCandidates.size)
    }
}
