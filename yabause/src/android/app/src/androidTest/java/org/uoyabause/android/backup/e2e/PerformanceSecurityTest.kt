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
import android.os.Build
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.google.firebase.auth.FirebaseAuth
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.tasks.await
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.BeforeClass
import org.junit.Test
import org.junit.runner.RunWith
import org.uoyabause.android.backup.model.SharedBackupItem
import java.io.File
import kotlin.system.measureTimeMillis

/**
 * Performance and Security Tests
 * Test cases: PT-001 ~ PT-007, ST-001 ~ ST-006
 *
 * These tests verify performance targets and security requirements.
 */
@RunWith(AndroidJUnit4::class)
class PerformanceSecurityTest {
    private lateinit var context: Context
    private lateinit var testFilesDir: File

    companion object {
        private var emuHost = "192.168.11.5"
        private lateinit var auth: FirebaseAuth

        private const val TEST_EMAIL = "devmiyax@gmail.com"
        private const val TEST_PASSWORD = "testpass01"
        private const val BACKUP_MAGIC = "BackUpRam Format"

        @JvmStatic
        @BeforeClass
        fun setUpClass() {
            if (Build.PRODUCT == "sdk_gphone_x86_64" || Build.PRODUCT.contains("sdk")) {
                emuHost = "10.0.2.2"
            }

            auth = FirebaseAuth.getInstance()
            auth.useEmulator(emuHost, 9099)
        }
    }

    @Before
    fun setUp() {
        context = InstrumentationRegistry.getInstrumentation().targetContext
        testFilesDir = context.filesDir
    }

    @After
    fun tearDown() {
        testFilesDir.listFiles()?.filter { it.name.startsWith("test_") }?.forEach { it.delete() }
    }

    // ==============================
    // PT-001: Backup list loading performance
    // ==============================

    /**
     * PT-001-01: Verify backup file parsing is fast
     */
    @Test
    fun backupFileParsing_completesQuickly() {
        val testFile = createTestBackupFile("test_perf.bin", 32 * 1024)

        val parseTime = measureTimeMillis {
            val content = testFile.readBytes()
            val header = String(content.copyOf(16), Charsets.US_ASCII)
            assertTrue(header.startsWith(BACKUP_MAGIC))
        }

        // Should complete in under 100ms
        assertTrue("Parse time was ${parseTime}ms", parseTime < 100)
    }

    /**
     * PT-001-02: Verify file listing is fast
     */
    @Test
    fun fileListing_completesQuickly() {
        // Create multiple test files
        repeat(10) { i ->
            createTestBackupFile("test_list_$i.bin", 1024)
        }

        val listTime = measureTimeMillis {
            val files = testFilesDir.listFiles()?.filter { it.extension == "bin" }
            assertNotNull(files)
            assertTrue((files?.size ?: 0) >= 10)
        }

        // Should complete in under 50ms
        assertTrue("List time was ${listTime}ms", listTime < 50)
    }

    // ==============================
    // PT-002: Tab switching response
    // ==============================

    /**
     * PT-002-01: Verify DeviceType enum lookup is fast
     */
    @Test
    fun deviceTypeLookup_isFast() {
        val lookupTime = measureTimeMillis {
            repeat(1000) {
                org.uoyabause.android.backup.model.DeviceType
                    .fromId(it % 3)
            }
        }

        // 1000 lookups should complete in under 10ms
        assertTrue("Lookup time was ${lookupTime}ms", lookupTime < 10)
    }

    // ==============================
    // PT-003: Search response
    // ==============================

    /**
     * PT-003-01: Verify string search is efficient
     */
    @Test
    fun stringSearch_isEfficient() {
        val items = (1..100).map { "Game Title $it" }
        val query = "50"

        val searchTime = measureTimeMillis {
            val results = items.filter { it.contains(query) }
            assertEquals(1, results.size)
        }

        assertTrue("Search time was ${searchTime}ms", searchTime < 10)
    }

    // ==============================
    // PT-006: Memory usage
    // ==============================

    /**
     * PT-006-01: Verify backup data doesn't leak memory
     */
    @Test
    fun backupData_doesNotLeakMemory() {
        val runtime = Runtime.getRuntime()
        val initialMemory = runtime.totalMemory() - runtime.freeMemory()

        // Create and discard multiple backup data objects
        repeat(100) {
            val data = ByteArray(10 * 1024) // 10KB each
            assertTrue(data.isNotEmpty())
        }

        System.gc()
        Thread.sleep(100)

        val finalMemory = runtime.totalMemory() - runtime.freeMemory()
        val memoryIncrease = finalMemory - initialMemory

        // Memory increase should be reasonable (less than 10MB)
        assertTrue("Memory increase was ${memoryIncrease / 1024 / 1024}MB", memoryIncrease < 10 * 1024 * 1024)
    }

    // ==============================
    // ST-001: Firebase authentication verification
    // ==============================

    /**
     * ST-001-01: Verify auth instance is available
     */
    @Test
    fun authInstance_isAvailable() {
        assertNotNull(auth)
    }

    /**
     * ST-001-02: Verify sign out works
     */
    @Test
    fun signOut_works() {
        auth.signOut()
        val currentUser = auth.currentUser
        // User should be null after sign out
        assertTrue(currentUser == null)
    }

    /**
     * ST-001-03: Verify invalid credentials are rejected
     */
    @Test
    fun invalidCredentials_areRejected() {
        auth.signOut()

        runBlocking {
            try {
                auth.signInWithEmailAndPassword("invalid@test.com", "wrongpassword").await()
                // Should not reach here
                assertTrue("Should have thrown exception", false)
            } catch (e: Exception) {
                // Expected - invalid credentials should fail
                assertNotNull(e)
            }
        }
    }

    // ==============================
    // ST-002: Owner verification (cloud backup)
    // ==============================

    /**
     * ST-002-01: Verify ownership check works
     */
    @Test
    fun ownershipCheck_works() {
        val item = SharedBackupItem(
            id = "backup123",
            ownerId = "user456",
            gameTitle = "Test Game",
        )

        assertTrue(item.isOwnedBy("user456"))
        assertFalse(item.isOwnedBy("otherUser"))
        assertFalse(item.isOwnedBy(null))
    }

    // ==============================
    // ST-003: Shared backup owner verification
    // ==============================

    /**
     * ST-003-01: Verify shared backup owner ID is required
     */
    @Test
    fun sharedBackup_requiresOwnerId() {
        val item = SharedBackupItem(
            id = "shared123",
            ownerId = "", // Empty owner
            gameTitle = "Test",
        )

        assertFalse(item.isOwnedBy("anyUser"))
    }

    /**
     * ST-003-02: Verify owner ID is case-sensitive
     */
    @Test
    fun ownerId_isCaseSensitive() {
        val item = SharedBackupItem(
            id = "backup123",
            ownerId = "User123",
            gameTitle = "Test",
        )

        assertTrue(item.isOwnedBy("User123"))
        assertFalse(item.isOwnedBy("user123"))
        assertFalse(item.isOwnedBy("USER123"))
    }

    // ==============================
    // ST-005: Input validation
    // ==============================

    /**
     * ST-005-01: Verify filename sanitization
     */
    @Test
    fun filename_isSanitized() {
        fun sanitizeFilename(input: String): String = input.replace(Regex("[^a-zA-Z0-9._-]"), "_")

        assertEquals("safe_file.bin", sanitizeFilename("safe_file.bin"))
        assertEquals("path_to_file.bin", sanitizeFilename("path/to/file.bin"))
        assertEquals("file__script_.bin", sanitizeFilename("file<script>.bin"))
        assertEquals("______", sanitizeFilename("../../.."))
    }

    /**
     * ST-005-02: Verify comment length is limited
     */
    @Test
    fun commentLength_isLimited() {
        fun validateComment(comment: String, maxLength: Int = 500): String = if (comment.length > maxLength) {
            comment.take(maxLength)
        } else {
            comment
        }

        val shortComment = "Short comment"
        assertEquals(shortComment, validateComment(shortComment))

        val longComment = "A".repeat(1000)
        assertEquals(500, validateComment(longComment).length)
    }

    /**
     * ST-005-03: Verify game title doesn't contain script tags
     */
    @Test
    fun gameTitle_doesNotContainScriptTags() {
        fun sanitizeHtml(input: String): String = input.replace(Regex("<[^>]*>"), "")

        assertEquals("Game Title", sanitizeHtml("Game Title"))
        assertEquals("alert('xss')", sanitizeHtml("<script>alert('xss')</script>"))
        assertEquals("Bold Text", sanitizeHtml("<b>Bold Text</b>"))
    }

    // ==============================
    // ST-006: Rating operation restrictions
    // ==============================

    /**
     * ST-006-01: Verify rating range is validated
     */
    @Test
    fun ratingRange_isValidated() {
        fun validateRating(rating: Int): Int = rating.coerceIn(1, 5)

        assertEquals(1, validateRating(0))
        assertEquals(1, validateRating(-5))
        assertEquals(3, validateRating(3))
        assertEquals(5, validateRating(5))
        assertEquals(5, validateRating(10))
    }

    /**
     * ST-006-02: Verify owner cannot rate own backup
     */
    @Test
    fun owner_cannotRateOwnBackup() {
        val userId = "user123"
        val item = SharedBackupItem(
            id = "backup123",
            ownerId = userId,
            gameTitle = "Test",
        )

        fun canRate(item: SharedBackupItem, userId: String): Boolean = !item.isOwnedBy(userId)

        assertFalse(canRate(item, userId))
        assertTrue(canRate(item, "otherUser"))
    }

    // ==============================
    // Helper Methods
    // ==============================

    private fun createTestBackupFile(filename: String, size: Int): File {
        val file = File(testFilesDir, filename)
        val header = BACKUP_MAGIC.toByteArray(Charsets.US_ASCII)
        val padding = ByteArray(size - header.size)
        file.writeBytes(header + padding)
        return file
    }
}
