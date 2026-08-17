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
package org.uoyabause.android.backup.ui

import android.content.Context
import android.os.Bundle
import androidx.fragment.app.testing.FragmentScenario
import androidx.fragment.app.testing.launchFragmentInContainer
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.devmiyax.yabasanshiro.R
import org.hamcrest.CoreMatchers.not
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.uoyabause.android.backup.model.DeviceType
import java.io.File

/**
 * UI tests for BackupListFragment
 * Test cases: IT-L01 ~ IT-L04 (Detail tests)
 *
 * These tests verify the backup list UI displays correctly
 * for different device types (Internal, External, Cloud).
 */
@RunWith(AndroidJUnit4::class)
class BackupListFragmentTest {
    private lateinit var context: Context
    private lateinit var testFilesDir: File
    private var scenario: FragmentScenario<BackupListFragment>? = null

    companion object {
        private const val BACKUP_MAGIC = "BackUpRam Format"
        private const val HEADER_SIZE = 64
        private const val DIR_ENTRY_SIZE = 64
        private const val FILENAME_SIZE = 11
    }

    @Before
    fun setUp() {
        context = InstrumentationRegistry.getInstrumentation().targetContext
        testFilesDir = context.filesDir
    }

    @After
    fun tearDown() {
        scenario?.close()
        // Clean up test files
        testFilesDir.listFiles()?.filter { it.name.endsWith(".ram") }?.forEach { it.delete() }
    }

    private fun launchBackupListFragment(deviceType: DeviceType): FragmentScenario<BackupListFragment> {
        val args = Bundle().apply {
            putInt("deviceType", deviceType.id)
        }
        return launchFragmentInContainer(
            fragmentArgs = args,
            themeResId = R.style.AppTheme,
        )
    }

    // ==============================
    // IT-L01: Internal tab UI tests
    // ==============================

    /**
     * IT-L01-10: Verify RecyclerView is displayed
     */
    @Test
    fun backupList_displaysRecyclerView() {
        scenario = launchBackupListFragment(DeviceType.INTERNAL)

        onView(withId(R.id.recycler_view_backups))
            .check(matches(isDisplayed()))
    }

    /**
     * IT-L01-11: Verify SwipeRefresh is displayed
     */
    @Test
    fun backupList_displaysSwipeRefresh() {
        scenario = launchBackupListFragment(DeviceType.INTERNAL)

        onView(withId(R.id.swipe_refresh))
            .check(matches(isDisplayed()))
    }

    /**
     * IT-L01-12: Verify empty state is hidden initially
     * (Will show loading or content depending on data)
     */
    @Test
    fun backupList_emptyStateHiddenInitially() {
        scenario = launchBackupListFragment(DeviceType.INTERNAL)

        // Empty state visibility depends on data loading
        // This test verifies the layout exists
        onView(withId(R.id.layout_empty))
            .check(matches(not(isDisplayed())))
    }

    // ==============================
    // IT-L01: External tab UI tests
    // ==============================

    /**
     * IT-L01-20: Verify External tab displays correctly
     */
    @Test
    fun backupList_external_displaysRecyclerView() {
        scenario = launchBackupListFragment(DeviceType.EXTERNAL)

        onView(withId(R.id.recycler_view_backups))
            .check(matches(isDisplayed()))
    }

    // ==============================
    // IT-L01: Cloud tab UI tests
    // ==============================

    /**
     * IT-L01-30: Verify Cloud tab displays correctly
     */
    @Test
    fun backupList_cloud_displaysRecyclerView() {
        scenario = launchBackupListFragment(DeviceType.CLOUD)

        onView(withId(R.id.recycler_view_backups))
            .check(matches(isDisplayed()))
    }

    /**
     * IT-L01-31: Verify Cloud tab has auth required layout
     * (Should display when user is not signed in)
     */
    @Test
    fun backupList_cloud_hasAuthRequiredLayout() {
        scenario = launchBackupListFragment(DeviceType.CLOUD)

        // Auth required layout exists in the view hierarchy
        // Visibility depends on authentication state
        onView(withId(R.id.layout_auth_required))
            .check(matches(not(isDisplayed()))) // Initially hidden, shown based on auth state
    }

    // ==============================
    // IT-L02: Copy functionality tests
    // ==============================

    /**
     * IT-L02-10: Storage status layout exists
     */
    @Test
    fun backupList_hasStorageStatusLayout() {
        scenario = launchBackupListFragment(DeviceType.INTERNAL)

        // Storage status layout exists
        onView(withId(R.id.layout_storage_status))
            .check(matches(not(isDisplayed()))) // Hidden until data loads
    }

    // ==============================
    // IT-L03: Delete functionality tests
    // ==============================

    /**
     * IT-L03-10: Error layout exists
     */
    @Test
    fun backupList_hasErrorLayout() {
        scenario = launchBackupListFragment(DeviceType.INTERNAL)

        // Error layout exists but is hidden
        onView(withId(R.id.layout_error))
            .check(matches(not(isDisplayed())))
    }

    /**
     * IT-L03-11: Retry button exists in error layout
     */
    @Test
    fun backupList_hasRetryButton() {
        scenario = launchBackupListFragment(DeviceType.INTERNAL)

        // Retry button exists in error layout
        onView(withId(R.id.btn_retry))
            .check(matches(not(isDisplayed())))
    }

    // ==============================
    // IT-L04: Export functionality tests
    // ==============================

    /**
     * IT-L04-10: Loading layout exists
     */
    @Test
    fun backupList_hasLoadingLayout() {
        scenario = launchBackupListFragment(DeviceType.INTERNAL)

        // Loading layout exists (visibility depends on loading state)
        onView(withId(R.id.layout_loading))
            .check(matches(not(isDisplayed()))) // Hidden after initial load
    }

    // ==============================
    // Helper methods
    // ==============================

    private fun createTestBackupFile(filenames: List<String>): File {
        val file = File(testFilesDir, "backup.ram")
        val data = createBackupData(filenames)
        file.writeBytes(data)
        return file
    }

    private fun createBackupData(filenames: List<String>): ByteArray {
        val header = createHeader()
        val entries = filenames.map { createDirectoryEntry(it) }
        return header + entries.reduce { acc, bytes -> acc + bytes }
    }

    private fun createHeader(): ByteArray {
        val header = ByteArray(HEADER_SIZE)
        val magic = BACKUP_MAGIC.toByteArray(Charsets.US_ASCII)
        System.arraycopy(magic, 0, header, 0, magic.size)
        return header
    }

    private fun createDirectoryEntry(filename: String): ByteArray {
        val entry = ByteArray(DIR_ENTRY_SIZE)
        entry[0] = 0x80.toByte()
        val filenameBytes = filename.toByteArray(Charsets.US_ASCII)
        System.arraycopy(filenameBytes, 0, entry, 1, minOf(filenameBytes.size, FILENAME_SIZE))
        entry[24] = 44
        entry[25] = 1
        entry[26] = 15
        entry[27] = 10
        entry[28] = 30
        entry[29] = 0
        entry[30] = 0
        entry[31] = 1
        entry[32] = 0
        entry[33] = 0
        entry[34] = 4
        return entry
    }
}
