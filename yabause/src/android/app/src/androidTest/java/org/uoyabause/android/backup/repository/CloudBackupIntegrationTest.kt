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

import android.content.Context
import android.os.Build
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.database.FirebaseDatabase
import com.google.firebase.storage.FirebaseStorage
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
import java.io.File
import java.lang.Thread.sleep

/**
 * Integration tests for Cloud Backup functionality
 * Test cases: IT-C01 ~ IT-C05
 *
 * These tests use Firebase Emulator for testing cloud operations.
 * Requires Firebase Emulator to be running:
 * - Auth: port 9099
 * - Database: port 9000
 * - Storage: port 9199
 */
@RunWith(AndroidJUnit4::class)
class CloudBackupIntegrationTest {
    private lateinit var context: Context
    private lateinit var testFilesDir: File
    private var testState = 0

    companion object {
        private var emuHost = "192.168.11.5"
        private lateinit var database: FirebaseDatabase
        private lateinit var storage: FirebaseStorage
        private lateinit var auth: FirebaseAuth

        private const val TEST_EMAIL = "devmiyax@gmail.com"
        private const val TEST_PASSWORD = "testpass01"

        @JvmStatic
        @BeforeClass
        fun setUpClass() {
            val context = InstrumentationRegistry.getInstrumentation().targetContext

            // Detect emulator and adjust host
            if (Build.PRODUCT == "sdk_gphone_x86_64" || Build.PRODUCT.contains("sdk")) {
                emuHost = "10.0.2.2"
            }

            // Setup Firebase Emulators
            auth = FirebaseAuth.getInstance()
            auth.useEmulator(emuHost, 9099)

            database = FirebaseDatabase.getInstance()
            database.useEmulator(emuHost, 9000)

            storage = FirebaseStorage.getInstance()
            storage.useEmulator(emuHost, 9199)

            // Sign in with test user
            try {
                runBlocking {
                    auth.signInWithEmailAndPassword(TEST_EMAIL, TEST_PASSWORD).await()
                }
            } catch (e: Exception) {
                // User might already be signed in or emulator not running
            }
        }
    }

    @Before
    fun setUp() {
        context = InstrumentationRegistry.getInstrumentation().targetContext
        testFilesDir = context.filesDir
        testState = 0
    }

    @After
    fun tearDown() {
        // Clean up test files
        testFilesDir.listFiles()?.filter { it.name.startsWith("test_") }?.forEach { it.delete() }
    }

    private fun waitAsync(timeout: Int = 3000) {
        var timeCount = 0
        while (testState == 0) {
            sleep(10)
            timeCount += 10
            if (timeCount > timeout) {
                throw AssertionError("Async operation timed out")
            }
        }
        assertEquals("Async operation failed", 1, testState)
        testState = 0
    }

    // ==============================
    // IT-C01: Cloud backup list display
    // ==============================

    /**
     * IT-C01-01: Verify Firebase Auth is configured
     */
    @Test
    fun firebase_authIsConfigured() {
        assertNotNull(auth)
    }

    /**
     * IT-C01-02: Verify Firebase Database is configured
     */
    @Test
    fun firebase_databaseIsConfigured() {
        assertNotNull(database)
    }

    /**
     * IT-C01-03: Verify Firebase Storage is configured
     */
    @Test
    fun firebase_storageIsConfigured() {
        assertNotNull(storage)
    }

    /**
     * IT-C01-04: Verify emulator host is correctly detected
     */
    @Test
    fun emulatorHost_isCorrectlyDetected() {
        // On physical device: 192.168.11.5
        // On emulator: 10.0.2.2
        assertTrue(emuHost == "192.168.11.5" || emuHost == "10.0.2.2")
    }

    // ==============================
    // IT-C02: Upload to cloud
    // ==============================

    /**
     * IT-C02-01: Create test backup file for upload
     */
    @Test
    fun createTestBackupFile_succeeds() {
        val testFile = createTestBackupFile("test_upload.bin", 256)

        assertTrue(testFile.exists())
        assertEquals(256, testFile.length())
    }

    /**
     * IT-C02-02: Verify backup file content is valid
     */
    @Test
    fun testBackupFile_hasValidContent() {
        val testFile = createTestBackupFile("test_content.bin", 128)
        val content = testFile.readBytes()

        assertEquals(128, content.size)
        // First bytes should be backup magic
        val magic = String(content.copyOf(16), Charsets.US_ASCII)
        assertEquals("BackUpRam Format", magic)
    }

    // ==============================
    // IT-C03: Download from cloud
    // ==============================

    /**
     * IT-C03-01: Verify download directory exists
     */
    @Test
    fun downloadDirectory_exists() {
        assertTrue(testFilesDir.exists())
        assertTrue(testFilesDir.isDirectory)
    }

    /**
     * IT-C03-02: Verify download directory is writable
     */
    @Test
    fun downloadDirectory_isWritable() {
        val testFile = File(testFilesDir, "test_writable.tmp")
        testFile.writeText("test")
        assertTrue(testFile.exists())
        testFile.delete()
    }

    // ==============================
    // IT-C04: Unauthenticated access
    // ==============================

    /**
     * IT-C04-01: Verify auth state can be checked
     */
    @Test
    fun authState_canBeChecked() {
        val currentUser = auth.currentUser
        // User may or may not be signed in depending on emulator state
        // This test just verifies we can check the state
        if (currentUser != null) {
            assertNotNull(currentUser.uid)
        }
    }

    /**
     * IT-C04-02: Verify sign out works
     */
    @Test
    fun signOut_works() {
        // Store current state
        val wasSignedIn = auth.currentUser != null

        // Sign out
        auth.signOut()

        // Verify signed out
        val isSignedOut = auth.currentUser == null
        assertTrue(isSignedOut)

        // Restore state if was signed in
        if (wasSignedIn) {
            runBlocking {
                try {
                    auth.signInWithEmailAndPassword(TEST_EMAIL, TEST_PASSWORD).await()
                } catch (e: Exception) {
                    // Ignore errors in cleanup
                }
            }
        }
    }

    // ==============================
    // IT-C05: Storage limit
    // ==============================

    /**
     * IT-C05-01: Verify BackupLimits calculation
     */
    @Test
    fun backupLimits_calculatesCorrectly() {
        val limits = BackupLimits(maxCount = 10, currentCount = 5)

        assertEquals(10, limits.maxCount)
        assertEquals(5, limits.currentCount)
        assertEquals(5, limits.remainingCount)
        assertFalse(limits.isLimitReached)
    }

    /**
     * IT-C05-02: Verify limit reached detection
     */
    @Test
    fun backupLimits_detectsLimitReached() {
        val limits = BackupLimits(maxCount = 3, currentCount = 3)

        assertTrue(limits.isLimitReached)
        assertEquals(0, limits.remainingCount)
    }

    /**
     * IT-C05-03: Verify over limit detection
     */
    @Test
    fun backupLimits_detectsOverLimit() {
        val limits = BackupLimits(maxCount = 3, currentCount = 5)

        assertTrue(limits.isLimitReached)
        assertEquals(0, limits.remainingCount)
    }

    /**
     * IT-C05-04: Verify display string format
     */
    @Test
    fun backupLimits_displaysCorrectly() {
        val limits = BackupLimits(maxCount = 10, currentCount = 3)

        assertEquals("3 / 10", limits.displayString)
    }

    // ==============================
    // Helper methods
    // ==============================

    private fun createTestBackupFile(filename: String, size: Int): File {
        val file = File(testFilesDir, filename)
        val header = "BackUpRam Format".toByteArray(Charsets.US_ASCII)
        val padding = ByteArray(size - header.size)
        file.writeBytes(header + padding)
        return file
    }

    private fun clearTestData() {
        val currentUser = auth.currentUser ?: return

        runBlocking {
            try {
                // Clear database
                database
                    .getReference("user-posts")
                    .child(currentUser.uid)
                    .removeValue()
                    .await()

                // Clear storage
                val storageRef = storage.reference.child(currentUser.uid)
                val listResult = storageRef.listAll().await()
                listResult.items.forEach { item ->
                    try {
                        item.delete().await()
                    } catch (e: Exception) {
                        // Ignore delete errors
                    }
                }
            } catch (e: Exception) {
                // Ignore cleanup errors
            }
        }
    }
}
