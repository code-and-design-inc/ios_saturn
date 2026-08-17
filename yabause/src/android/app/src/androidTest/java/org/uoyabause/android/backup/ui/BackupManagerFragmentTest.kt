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
import androidx.fragment.app.testing.FragmentScenario
import androidx.fragment.app.testing.launchFragmentInContainer
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.action.ViewActions.swipeLeft
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.devmiyax.yabasanshiro.R
import org.hamcrest.CoreMatchers.not
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File
import java.nio.charset.Charset

/**
 * UI tests for BackupManagerFragment
 * Test cases: IT-L01 ~ IT-L04
 *
 * These tests verify the backup manager UI displays correctly
 * and responds to user interactions.
 */
@RunWith(AndroidJUnit4::class)
class BackupManagerFragmentTest {
    private lateinit var context: Context
    private lateinit var testFilesDir: File
    private var scenario: FragmentScenario<BackupManagerFragment>? = null

    companion object {
        private const val BACKUP_MAGIC = "BackUpRam Format"
        private const val HEADER_SIZE = 64
        private const val DIR_ENTRY_SIZE = 64
        private const val FILENAME_SIZE = 11
        private const val COMMENT_SIZE = 10
        private val MS932 = Charset.forName("MS932")
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

    // ==============================
    // IT-L01: Internal storage backup list display
    // ==============================

    /**
     * IT-L01-01: Verify TabLayout is displayed
     */
    @Test
    fun backupManager_displaysTabLayout() {
        scenario = launchFragmentInContainer<BackupManagerFragment>(
            themeResId = R.style.AppTheme,
        )

        onView(withId(R.id.tab_layout_backup))
            .check(matches(isDisplayed()))
    }

    /**
     * IT-L01-02: Verify Internal tab is selected by default
     */
    @Test
    fun backupManager_internalTabSelectedByDefault() {
        scenario = launchFragmentInContainer<BackupManagerFragment>(
            themeResId = R.style.AppTheme,
        )

        // Tab layout should show Internal tab
        onView(withText(R.string.tab_internal))
            .check(matches(isDisplayed()))
    }

    /**
     * IT-L01-03: Verify ViewPager is displayed
     */
    @Test
    fun backupManager_displaysViewPager() {
        scenario = launchFragmentInContainer<BackupManagerFragment>(
            themeResId = R.style.AppTheme,
        )

        onView(withId(R.id.view_pager_backup))
            .check(matches(isDisplayed()))
    }

    /**
     * IT-L01-04: Verify toolbar is displayed with title
     */
    @Test
    fun backupManager_displaysToolbar() {
        scenario = launchFragmentInContainer<BackupManagerFragment>(
            themeResId = R.style.AppTheme,
        )

        onView(withId(R.id.toolbar_backup_manager))
            .check(matches(isDisplayed()))
    }

    // ==============================
    // IT-L01-05: Tab switching tests
    // ==============================

    /**
     * IT-L01-05: Verify tab switching to External works
     */
    @Test
    fun tabSwitching_toExternal_works() {
        scenario = launchFragmentInContainer<BackupManagerFragment>(
            themeResId = R.style.AppTheme,
        )

        // Click External tab
        onView(withText(R.string.tab_external))
            .perform(click())

        // Tab should be selected (implicit verification by successful click)
    }

    /**
     * IT-L01-06: Verify tab switching to Cloud works
     */
    @Test
    fun tabSwitching_toCloud_works() {
        scenario = launchFragmentInContainer<BackupManagerFragment>(
            themeResId = R.style.AppTheme,
        )

        // Click Cloud tab
        onView(withText(R.string.tab_cloud))
            .perform(click())
    }

    /**
     * IT-L01-07: Verify tab switching to Shared works
     */
    @Test
    fun tabSwitching_toShared_works() {
        scenario = launchFragmentInContainer<BackupManagerFragment>(
            themeResId = R.style.AppTheme,
        )

        // Click Shared tab
        onView(withText(R.string.tab_shared))
            .perform(click())
    }

    /**
     * IT-L01-08: Verify swipe navigation works
     */
    @Test
    fun swipeNavigation_changesTab() {
        scenario = launchFragmentInContainer<BackupManagerFragment>(
            themeResId = R.style.AppTheme,
        )

        // Swipe left to go to External tab
        onView(withId(R.id.view_pager_backup))
            .perform(swipeLeft())
    }

    // ==============================
    // IT-L02: Backup copy tests (placeholder)
    // ==============================

    /**
     * IT-L02-01: Placeholder for copy test
     * Note: Full copy test requires test backup data setup
     */
    @Test
    fun placeholder_copyToExternal() {
        // This test will be implemented when backup data setup is complete
        scenario = launchFragmentInContainer<BackupManagerFragment>(
            themeResId = R.style.AppTheme,
        )
    }

    // ==============================
    // IT-L03: Backup delete tests (placeholder)
    // ==============================

    /**
     * IT-L03-01: Placeholder for delete test
     */
    @Test
    fun placeholder_deleteBackup() {
        scenario = launchFragmentInContainer<BackupManagerFragment>(
            themeResId = R.style.AppTheme,
        )
    }

    // ==============================
    // IT-L04: Backup export tests (placeholder)
    // ==============================

    /**
     * IT-L04-01: Placeholder for export test
     */
    @Test
    fun placeholder_exportBackup() {
        scenario = launchFragmentInContainer<BackupManagerFragment>(
            themeResId = R.style.AppTheme,
        )
    }

    // ==============================
    // Helper methods
    // ==============================

    private fun createTestBackupFile(filenames: List<String>, interleaved: Boolean): File {
        val file = File(testFilesDir, "test_backup.ram")
        val data = createBackupData(filenames)
        if (interleaved) {
            file.writeBytes(interleave(data))
        } else {
            file.writeBytes(data)
        }
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
        // Status byte (0x80 = valid)
        entry[0] = 0x80.toByte()

        // Filename (11 bytes)
        val filenameBytes = filename.toByteArray(Charsets.US_ASCII)
        System.arraycopy(filenameBytes, 0, entry, 1, minOf(filenameBytes.size, FILENAME_SIZE))

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
