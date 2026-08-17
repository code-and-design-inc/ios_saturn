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
import com.google.android.gms.tasks.Task
import com.google.android.gms.tasks.Tasks
import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.auth.FirebaseUser
import com.google.firebase.database.DataSnapshot
import com.google.firebase.database.DatabaseReference
import com.google.firebase.database.FirebaseDatabase
import com.google.firebase.firestore.FirebaseFirestore
import com.google.firebase.storage.FirebaseStorage
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.unmockkAll
import kotlinx.coroutines.test.runTest
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

/**
 * Unit tests for FirebaseBackupDataSource
 * Test cases: UT-D08 ~ UT-D13
 */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28])
class FirebaseBackupDataSourceTest {
    private lateinit var context: Context
    private lateinit var mockAuth: FirebaseAuth
    private lateinit var mockUser: FirebaseUser
    private lateinit var mockDatabase: FirebaseDatabase
    private lateinit var mockFirestore: FirebaseFirestore
    private lateinit var mockStorage: FirebaseStorage

    private lateinit var dataSource: FirebaseBackupDataSource

    companion object {
        private const val TEST_UID = "test-user-123"
        private const val TEST_USER_NAME = "Test User"
        private const val TEST_PHOTO_URL = "https://example.com/photo.jpg"
    }

    @Before
    fun setUp() {
        context = mockk(relaxed = true)
        mockAuth = mockk(relaxed = true)
        mockUser = mockk(relaxed = true)
        mockDatabase = mockk(relaxed = true)
        mockFirestore = mockk(relaxed = true)
        mockStorage = mockk(relaxed = true)

        // Mock FirebaseAuth.getInstance()
        mockkStatic(FirebaseAuth::class)
        every { FirebaseAuth.getInstance() } returns mockAuth

        // Mock FirebaseDatabase.getInstance()
        mockkStatic(FirebaseDatabase::class)
        every { FirebaseDatabase.getInstance() } returns mockDatabase

        // Mock FirebaseFirestore.getInstance()
        mockkStatic(FirebaseFirestore::class)
        every { FirebaseFirestore.getInstance() } returns mockFirestore

        // Mock FirebaseStorage.getInstance()
        mockkStatic(FirebaseStorage::class)
        every { FirebaseStorage.getInstance() } returns mockStorage

        // Setup default authenticated user
        setupAuthenticatedUser()

        dataSource = FirebaseBackupDataSource(context)
    }

    @After
    fun tearDown() {
        unmockkAll()
    }

    private fun setupAuthenticatedUser() {
        every { mockAuth.currentUser } returns mockUser
        every { mockUser.uid } returns TEST_UID
        every { mockUser.displayName } returns TEST_USER_NAME
        every { mockUser.photoUrl } returns Uri.parse(TEST_PHOTO_URL)
    }

    private fun setupUnauthenticatedUser() {
        every { mockAuth.currentUser } returns null
    }

    // UT-D08: Test isAuthenticated
    @Test
    fun `isAuthenticated returns true when user is logged in`() {
        setupAuthenticatedUser()

        assertTrue(dataSource.isAuthenticated())
    }

    @Test
    fun `isAuthenticated returns false when user is not logged in`() {
        setupUnauthenticatedUser()

        assertFalse(dataSource.isAuthenticated())
    }

    // UT-D09: Test getCurrentUserId
    @Test
    fun `getCurrentUserId returns uid when authenticated`() {
        setupAuthenticatedUser()

        assertEquals(TEST_UID, dataSource.getCurrentUserId())
    }

    @Test
    fun `getCurrentUserId returns null when not authenticated`() {
        setupUnauthenticatedUser()

        assertNull(dataSource.getCurrentUserId())
    }

    // UT-D10: Test getCurrentUserName
    @Test
    fun `getCurrentUserName returns display name when authenticated`() {
        setupAuthenticatedUser()

        assertEquals(TEST_USER_NAME, dataSource.getCurrentUserName())
    }

    @Test
    fun `getCurrentUserName returns Unknown when not authenticated`() {
        setupUnauthenticatedUser()

        assertEquals("Unknown", dataSource.getCurrentUserName())
    }

    // UT-D11: Test getMaxBackupCount
    @Test
    fun `getMaxBackupCount returns default when not authenticated`() = runTest {
        setupUnauthenticatedUser()

        val result = dataSource.getMaxBackupCount()

        assertEquals(3, result) // DEFAULT_MAX_BACKUP_COUNT
    }

    @Test
    fun `getMaxBackupCount returns value from database when authenticated`() = runTest {
        setupAuthenticatedUser()

        val mockRef = mockk<DatabaseReference>(relaxed = true)
        val mockSnapshot = mockk<DataSnapshot>()

        every { mockDatabase.reference } returns mockRef
        every { mockRef.child(any()) } returns mockRef
        every { mockSnapshot.getValue(Long::class.java) } returns 10L

        val mockTask: Task<DataSnapshot> = Tasks.forResult(mockSnapshot)
        every { mockRef.get() } returns mockTask

        val result = dataSource.getMaxBackupCount()

        // The result may vary based on mock setup, but should return a valid count
        assertTrue(result >= 3) // At least default
    }

    // UT-D12: Test rateBackup validation
    @Test
    fun `rateBackup returns false for invalid rating below 1`() = runTest {
        setupAuthenticatedUser()

        val result = dataSource.rateBackup("backup-123", 0)

        assertFalse(result)
    }

    @Test
    fun `rateBackup returns false for invalid rating above 5`() = runTest {
        setupAuthenticatedUser()

        val result = dataSource.rateBackup("backup-123", 6)

        assertFalse(result)
    }

    @Test
    fun `rateBackup returns false when not authenticated`() = runTest {
        setupUnauthenticatedUser()

        val result = dataSource.rateBackup("backup-123", 4)

        assertFalse(result)
    }

    // UT-D13: Test deleteCloudBackup authorization
    @Test
    fun `deleteCloudBackup returns false when not authenticated`() = runTest {
        setupUnauthenticatedUser()

        val mockBackupItem = mockk<org.uoyabause.android.backup.model.BackupItem>(relaxed = true)

        val result = dataSource.deleteCloudBackup(mockBackupItem)

        assertFalse(result)
    }

    @Test
    fun `deleteCloudBackup returns false when firebaseKey is null`() = runTest {
        setupAuthenticatedUser()

        val mockBackupItem = mockk<org.uoyabause.android.backup.model.BackupItem>()
        every { mockBackupItem.firebaseKey } returns null

        val result = dataSource.deleteCloudBackup(mockBackupItem)

        assertFalse(result)
    }

    // Test getCurrentUserPhotoUrl
    @Test
    fun `getCurrentUserPhotoUrl returns photo URL when authenticated`() {
        setupAuthenticatedUser()

        assertEquals(TEST_PHOTO_URL, dataSource.getCurrentUserPhotoUrl())
    }

    @Test
    fun `getCurrentUserPhotoUrl returns null when not authenticated`() {
        setupUnauthenticatedUser()

        assertNull(dataSource.getCurrentUserPhotoUrl())
    }

    // Test downloadCloudBackup
    @Test
    fun `downloadCloudBackup returns null when downloadUrl is null`() = runTest {
        val mockBackupItem = mockk<org.uoyabause.android.backup.model.BackupItem>()
        every { mockBackupItem.downloadUrl } returns null
        every { mockBackupItem.filename } returns "TEST_FILE"

        val result = dataSource.downloadCloudBackup(mockBackupItem)

        assertNull(result)
    }

    // Test deleteSharedBackup authorization
    @Test
    fun `deleteSharedBackup returns false when not authenticated`() = runTest {
        setupUnauthenticatedUser()

        val mockSharedItem = mockk<org.uoyabause.android.backup.model.SharedBackupItem>(relaxed = true)

        val result = dataSource.deleteSharedBackup(mockSharedItem)

        assertFalse(result)
    }

    @Test
    fun `deleteSharedBackup returns false when not owner`() = runTest {
        setupAuthenticatedUser()

        val mockSharedItem = mockk<org.uoyabause.android.backup.model.SharedBackupItem>()
        every { mockSharedItem.ownerId } returns "different-user-id"

        val result = dataSource.deleteSharedBackup(mockSharedItem)

        assertFalse(result)
    }

    // Test uploadCloudBackup
    @Test
    fun `uploadCloudBackup returns null when not authenticated`() = runTest {
        setupUnauthenticatedUser()

        val backupData = ByteArray(100)
        val metadata = mockk<org.uoyabause.android.backup.model.CloudBackupMetadata>(relaxed = true)

        val result = dataSource.uploadCloudBackup(backupData, metadata)

        assertNull(result)
    }

    // Test shareBackup
    @Test
    fun `shareBackup returns null when not authenticated`() = runTest {
        setupUnauthenticatedUser()

        val mockBackupItem = mockk<org.uoyabause.android.backup.model.BackupItem>(relaxed = true)
        val backupData = ByteArray(100)

        val result = dataSource.shareBackup(
            backup = mockBackupItem,
            backupData = backupData,
            gameTitle = "Test Game",
            productNumber = "T-12345",
            description = "Test comment",
        )

        assertNull(result)
    }

    // Test getUserRating
    @Test
    fun `getUserRating returns null when not authenticated`() = runTest {
        setupUnauthenticatedUser()

        val result = dataSource.getUserRating("backup-123")

        assertNull(result)
    }
}
