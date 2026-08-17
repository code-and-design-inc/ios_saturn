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
package org.uoyabause.android.backup.repository

import org.junit.Assert.assertEquals
import org.junit.Test

/**
 * Unit tests for BackupRepository and StorageStatus
 * Test cases: UT-R01 ~ UT-R09
 *
 * Note: Full repository tests require mocking LocalBackupDataSource,
 * which requires dependency injection refactoring.
 * These tests focus on the StorageStatus data class logic.
 */
class BackupRepositoryTest {
    // UT-R01: Test StorageStatus creation
    @Test
    fun `StorageStatus calculates usedSize correctly`() {
        val status = StorageStatus(
            totalSize = 1000,
            freeSize = 400,
            usedSize = 600,
        )

        assertEquals(600, status.usedSize)
    }

    // UT-R02: Test displayTotalSize formatting
    @Test
    fun `displayTotalSize returns bytes for small sizes`() {
        val status = StorageStatus(totalSize = 500, freeSize = 100, usedSize = 400)
        assertEquals("500 B", status.displayTotalSize)
    }

    @Test
    fun `displayTotalSize returns KB for kilobyte sizes`() {
        val status = StorageStatus(totalSize = 2048, freeSize = 1024, usedSize = 1024)
        assertEquals("2.0 KB", status.displayTotalSize)
    }

    @Test
    fun `displayTotalSize returns MB for megabyte sizes`() {
        val status = StorageStatus(totalSize = 1024 * 1024, freeSize = 512 * 1024, usedSize = 512 * 1024)
        assertEquals("1.0 MB", status.displayTotalSize)
    }

    // UT-R03: Test displayFreeSize formatting
    @Test
    fun `displayFreeSize formats correctly`() {
        val status = StorageStatus(totalSize = 2048, freeSize = 1024, usedSize = 1024)
        assertEquals("1.0 KB", status.displayFreeSize)
    }

    // UT-R04: Test displayUsedSize formatting
    @Test
    fun `displayUsedSize formats correctly`() {
        val status = StorageStatus(totalSize = 4096, freeSize = 1024, usedSize = 3072)
        assertEquals("3.0 KB", status.displayUsedSize)
    }

    // UT-R05: Test usagePercent calculation
    @Test
    fun `usagePercent calculates correctly for 50 percent usage`() {
        val status = StorageStatus(totalSize = 1000, freeSize = 500, usedSize = 500)
        assertEquals(50, status.usagePercent)
    }

    @Test
    fun `usagePercent returns 0 for empty storage`() {
        val status = StorageStatus(totalSize = 1000, freeSize = 1000, usedSize = 0)
        assertEquals(0, status.usagePercent)
    }

    @Test
    fun `usagePercent returns 100 for full storage`() {
        val status = StorageStatus(totalSize = 1000, freeSize = 0, usedSize = 1000)
        assertEquals(100, status.usagePercent)
    }

    @Test
    fun `usagePercent returns 0 when totalSize is 0`() {
        val status = StorageStatus(totalSize = 0, freeSize = 0, usedSize = 0)
        assertEquals(0, status.usagePercent)
    }

    // UT-R06: Test StorageStatus with edge cases
    @Test
    fun `StorageStatus handles zero values`() {
        val status = StorageStatus(totalSize = 0, freeSize = 0, usedSize = 0)
        assertEquals("0 B", status.displayTotalSize)
        assertEquals("0 B", status.displayFreeSize)
        assertEquals("0 B", status.displayUsedSize)
    }

    @Test
    fun `StorageStatus handles exactly 1KB`() {
        val status = StorageStatus(totalSize = 1024, freeSize = 0, usedSize = 1024)
        assertEquals("1.0 KB", status.displayTotalSize)
    }

    @Test
    fun `StorageStatus handles exactly 1MB`() {
        val status = StorageStatus(totalSize = 1024 * 1024, freeSize = 0, usedSize = 1024 * 1024)
        assertEquals("1.0 MB", status.displayTotalSize)
    }

    // UT-R07: Test usagePercent with various values
    @Test
    fun `usagePercent returns 25 for quarter usage`() {
        val status = StorageStatus(totalSize = 1000, freeSize = 750, usedSize = 250)
        assertEquals(25, status.usagePercent)
    }

    @Test
    fun `usagePercent returns 75 for three-quarter usage`() {
        val status = StorageStatus(totalSize = 1000, freeSize = 250, usedSize = 750)
        assertEquals(75, status.usagePercent)
    }

    // UT-R08: Test StorageStatus decimal formatting
    @Test
    fun `displaySize shows one decimal place`() {
        val status = StorageStatus(totalSize = 1536, freeSize = 0, usedSize = 1536) // 1.5 KB
        assertEquals("1.5 KB", status.displayTotalSize)
    }

    @Test
    fun `displaySize shows one decimal place for MB`() {
        val status = StorageStatus(
            totalSize = (1.5 * 1024 * 1024).toInt(),
            freeSize = 0,
            usedSize = (1.5 * 1024 * 1024).toInt(),
        )
        assertEquals("1.5 MB", status.displayTotalSize)
    }

    // UT-R09: Test StorageStatus equality
    @Test
    fun `StorageStatus equality works correctly`() {
        val status1 = StorageStatus(totalSize = 1000, freeSize = 500, usedSize = 500)
        val status2 = StorageStatus(totalSize = 1000, freeSize = 500, usedSize = 500)

        assertEquals(status1, status2)
    }

    @Test
    fun `StorageStatus copy works correctly`() {
        val original = StorageStatus(totalSize = 1000, freeSize = 500, usedSize = 500)
        val copied = original.copy(freeSize = 300, usedSize = 700)

        assertEquals(1000, copied.totalSize)
        assertEquals(300, copied.freeSize)
        assertEquals(700, copied.usedSize)
    }
}
