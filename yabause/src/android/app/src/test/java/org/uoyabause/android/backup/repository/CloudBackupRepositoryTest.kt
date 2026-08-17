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
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Unit tests for CloudBackupRepository and related data classes
 * Test cases: UT-R10 ~ UT-R20
 *
 * Note: Full repository tests require mocking FirebaseBackupDataSource,
 * which requires dependency injection refactoring.
 * These tests focus on the data class logic.
 */
class CloudBackupRepositoryTest {
    // ==============================
    // UT-R10 ~ UT-R14: BackupLimits tests
    // ==============================

    // UT-R10: Test BackupLimits creation
    @Test
    fun `BackupLimits initializes with default currentCount of 0`() {
        val limits = BackupLimits(maxCount = 10)

        assertEquals(10, limits.maxCount)
        assertEquals(0, limits.currentCount)
    }

    @Test
    fun `BackupLimits initializes with provided currentCount`() {
        val limits = BackupLimits(maxCount = 10, currentCount = 5)

        assertEquals(10, limits.maxCount)
        assertEquals(5, limits.currentCount)
    }

    // UT-R11: Test isLimitReached
    @Test
    fun `isLimitReached returns false when under limit`() {
        val limits = BackupLimits(maxCount = 10, currentCount = 5)

        assertFalse(limits.isLimitReached)
    }

    @Test
    fun `isLimitReached returns true when at limit`() {
        val limits = BackupLimits(maxCount = 10, currentCount = 10)

        assertTrue(limits.isLimitReached)
    }

    @Test
    fun `isLimitReached returns true when over limit`() {
        val limits = BackupLimits(maxCount = 10, currentCount = 15)

        assertTrue(limits.isLimitReached)
    }

    @Test
    fun `isLimitReached returns true when maxCount is 0`() {
        val limits = BackupLimits(maxCount = 0, currentCount = 0)

        assertTrue(limits.isLimitReached)
    }

    // UT-R12: Test remainingCount
    @Test
    fun `remainingCount calculates correctly`() {
        val limits = BackupLimits(maxCount = 10, currentCount = 3)

        assertEquals(7, limits.remainingCount)
    }

    @Test
    fun `remainingCount returns 0 when at limit`() {
        val limits = BackupLimits(maxCount = 10, currentCount = 10)

        assertEquals(0, limits.remainingCount)
    }

    @Test
    fun `remainingCount returns 0 when over limit`() {
        val limits = BackupLimits(maxCount = 10, currentCount = 15)

        assertEquals(0, limits.remainingCount)
    }

    @Test
    fun `remainingCount returns maxCount when currentCount is 0`() {
        val limits = BackupLimits(maxCount = 10, currentCount = 0)

        assertEquals(10, limits.remainingCount)
    }

    // UT-R13: Test displayString
    @Test
    fun `displayString formats correctly`() {
        val limits = BackupLimits(maxCount = 10, currentCount = 5)

        assertEquals("5 / 10", limits.displayString)
    }

    @Test
    fun `displayString shows zero correctly`() {
        val limits = BackupLimits(maxCount = 3, currentCount = 0)

        assertEquals("0 / 3", limits.displayString)
    }

    @Test
    fun `displayString shows full correctly`() {
        val limits = BackupLimits(maxCount = 256, currentCount = 256)

        assertEquals("256 / 256", limits.displayString)
    }

    // UT-R14: Test BackupLimits equality
    @Test
    fun `BackupLimits equality works correctly`() {
        val limits1 = BackupLimits(maxCount = 10, currentCount = 5)
        val limits2 = BackupLimits(maxCount = 10, currentCount = 5)

        assertEquals(limits1, limits2)
    }

    @Test
    fun `BackupLimits copy works correctly`() {
        val original = BackupLimits(maxCount = 10, currentCount = 5)
        val copied = original.copy(currentCount = 8)

        assertEquals(10, copied.maxCount)
        assertEquals(8, copied.currentCount)
    }

    // ==============================
    // UT-R15 ~ UT-R17: UserInfo tests
    // ==============================

    // UT-R15: Test UserInfo creation
    @Test
    fun `UserInfo initializes correctly with all fields`() {
        val userInfo = UserInfo(
            uid = "user123",
            displayName = "Test User",
            photoUrl = "https://example.com/photo.jpg",
        )

        assertEquals("user123", userInfo.uid)
        assertEquals("Test User", userInfo.displayName)
        assertEquals("https://example.com/photo.jpg", userInfo.photoUrl)
    }

    @Test
    fun `UserInfo handles null photoUrl`() {
        val userInfo = UserInfo(
            uid = "user123",
            displayName = "Test User",
            photoUrl = null,
        )

        assertEquals("user123", userInfo.uid)
        assertEquals("Test User", userInfo.displayName)
        assertEquals(null, userInfo.photoUrl)
    }

    // UT-R16: Test UserInfo equality
    @Test
    fun `UserInfo equality works correctly`() {
        val user1 = UserInfo(uid = "user123", displayName = "Test", photoUrl = null)
        val user2 = UserInfo(uid = "user123", displayName = "Test", photoUrl = null)

        assertEquals(user1, user2)
    }

    // UT-R17: Test UserInfo with empty values
    @Test
    fun `UserInfo handles empty strings`() {
        val userInfo = UserInfo(
            uid = "",
            displayName = "",
            photoUrl = null,
        )

        assertEquals("", userInfo.uid)
        assertEquals("", userInfo.displayName)
    }

    // ==============================
    // UT-R18 ~ UT-R20: ShareInfo tests
    // ==============================

    // UT-R18: Test ShareInfo creation
    @Test
    fun `ShareInfo initializes correctly`() {
        val shareInfo = ShareInfo(
            gameTitle = "Sonic the Hedgehog",
            productNumber = "T-12345",
            description = "100% completion save",
        )

        assertEquals("Sonic the Hedgehog", shareInfo.gameTitle)
        assertEquals("T-12345", shareInfo.productNumber)
        assertEquals("100% completion save", shareInfo.description)
    }

    // UT-R19: Test ShareInfo equality
    @Test
    fun `ShareInfo equality works correctly`() {
        val share1 = ShareInfo(gameTitle = "Sonic", productNumber = "T-123", description = "Test")
        val share2 = ShareInfo(gameTitle = "Sonic", productNumber = "T-123", description = "Test")

        assertEquals(share1, share2)
    }

    // UT-R20: Test ShareInfo with various content
    @Test
    fun `ShareInfo handles empty strings`() {
        val shareInfo = ShareInfo(
            gameTitle = "",
            productNumber = "",
            description = "",
        )

        assertEquals("", shareInfo.gameTitle)
        assertEquals("", shareInfo.productNumber)
        assertEquals("", shareInfo.description)
    }

    @Test
    fun `ShareInfo handles long description`() {
        val longComment = "A".repeat(500)
        val shareInfo = ShareInfo(
            gameTitle = "Test Game",
            productNumber = "T-00001",
            description = longComment,
        )

        assertEquals(500, shareInfo.description.length)
    }

    @Test
    fun `ShareInfo handles Japanese text`() {
        val shareInfo = ShareInfo(
            gameTitle = "ソニック・ザ・ヘッジホッグ",
            productNumber = "T-12345",
            description = "クリアデータ",
        )

        assertEquals("ソニック・ザ・ヘッジホッグ", shareInfo.gameTitle)
        assertEquals("クリアデータ", shareInfo.description)
    }

    @Test
    fun `ShareInfo copy works correctly`() {
        val original = ShareInfo(
            gameTitle = "Original",
            productNumber = "T-123",
            description = "Original description",
        )
        val copied = original.copy(description = "Updated description")

        assertEquals("Original", copied.gameTitle)
        assertEquals("T-123", copied.productNumber)
        assertEquals("Updated description", copied.description)
    }
}
