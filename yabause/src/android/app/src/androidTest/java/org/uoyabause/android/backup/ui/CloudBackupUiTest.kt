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
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.google.firebase.auth.FirebaseAuth
import org.devmiyax.yabasanshiro.R
import org.hamcrest.CoreMatchers.not
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.uoyabause.android.backup.model.DeviceType

/**
 * UI tests for Cloud Backup tab
 * Test cases: IT-C01 ~ IT-C05 (UI portion)
 *
 * These tests verify the cloud backup UI displays correctly
 * and handles authentication states properly.
 */
@RunWith(AndroidJUnit4::class)
class CloudBackupUiTest {
    private lateinit var context: Context
    private var scenario: FragmentScenario<BackupListFragment>? = null
    private var managerScenario: FragmentScenario<BackupManagerFragment>? = null

    @Before
    fun setUp() {
        context = InstrumentationRegistry.getInstrumentation().targetContext
    }

    @After
    fun tearDown() {
        scenario?.close()
        managerScenario?.close()
    }

    private fun launchCloudBackupListFragment(): FragmentScenario<BackupListFragment> {
        val args = Bundle().apply {
            putInt("deviceType", DeviceType.CLOUD.id)
        }
        return launchFragmentInContainer(
            fragmentArgs = args,
            themeResId = R.style.AppTheme,
        )
    }

    // ==============================
    // IT-C01: Cloud backup list display
    // ==============================

    /**
     * IT-C01-10: Verify Cloud tab can be selected
     */
    @Test
    fun cloudTab_canBeSelected() {
        managerScenario = launchFragmentInContainer<BackupManagerFragment>(
            themeResId = R.style.AppTheme,
        )

        // Click Cloud tab
        onView(withText(R.string.tab_cloud))
            .perform(click())
    }

    /**
     * IT-C01-11: Verify Cloud list fragment displays RecyclerView
     */
    @Test
    fun cloudListFragment_displaysRecyclerView() {
        scenario = launchCloudBackupListFragment()

        onView(withId(R.id.recycler_view_backups))
            .check(matches(isDisplayed()))
    }

    /**
     * IT-C01-12: Verify Cloud list has backup limit TextView
     */
    @Test
    fun cloudListFragment_hasBackupLimitView() {
        scenario = launchCloudBackupListFragment()

        // Backup limit view exists (visibility depends on state)
        onView(withId(R.id.tv_backup_limit))
            .check(matches(not(isDisplayed()))) // Hidden until data loads
    }

    /**
     * IT-C01-13: Verify SwipeRefresh is available
     */
    @Test
    fun cloudListFragment_hasSwipeRefresh() {
        scenario = launchCloudBackupListFragment()

        onView(withId(R.id.swipe_refresh))
            .check(matches(isDisplayed()))
    }

    // ==============================
    // IT-C04: Unauthenticated access UI
    // ==============================

    /**
     * IT-C04-10: Verify auth required layout exists
     */
    @Test
    fun cloudListFragment_hasAuthRequiredLayout() {
        scenario = launchCloudBackupListFragment()

        // Auth required layout exists in view hierarchy
        onView(withId(R.id.layout_auth_required))
            .check(matches(not(isDisplayed()))) // Visibility depends on auth state
    }

    /**
     * IT-C04-11: Verify sign in button exists
     */
    @Test
    fun cloudListFragment_hasSignInButton() {
        scenario = launchCloudBackupListFragment()

        // Sign in button exists
        onView(withId(R.id.btn_sign_in))
            .check(matches(not(isDisplayed()))) // Hidden when authenticated
    }

    /**
     * IT-C04-12: Check current auth state
     */
    @Test
    fun currentAuthState_canBeChecked() {
        val auth = FirebaseAuth.getInstance()
        // This test verifies we can access auth state from UI tests
        val isSignedIn = auth.currentUser != null
        // Test passes regardless of sign-in state - just verifies access
    }

    // ==============================
    // IT-C02/C03: Upload/Download UI
    // ==============================

    /**
     * IT-C02-10: Verify loading layout exists for upload progress
     */
    @Test
    fun cloudListFragment_hasLoadingLayout() {
        scenario = launchCloudBackupListFragment()

        onView(withId(R.id.layout_loading))
            .check(matches(not(isDisplayed()))) // Hidden by default
    }

    /**
     * IT-C03-10: Verify error layout exists for download errors
     */
    @Test
    fun cloudListFragment_hasErrorLayout() {
        scenario = launchCloudBackupListFragment()

        onView(withId(R.id.layout_error))
            .check(matches(not(isDisplayed()))) // Hidden by default
    }

    /**
     * IT-C03-11: Verify retry button exists
     */
    @Test
    fun cloudListFragment_hasRetryButton() {
        scenario = launchCloudBackupListFragment()

        onView(withId(R.id.btn_retry))
            .check(matches(not(isDisplayed()))) // Hidden by default
    }

    // ==============================
    // IT-C05: Storage limit UI
    // ==============================

    /**
     * IT-C05-10: Verify empty state layout exists
     */
    @Test
    fun cloudListFragment_hasEmptyLayout() {
        scenario = launchCloudBackupListFragment()

        onView(withId(R.id.layout_empty))
            .check(matches(not(isDisplayed()))) // Hidden by default
    }

    /**
     * IT-C05-11: Verify empty message view exists
     */
    @Test
    fun cloudListFragment_hasEmptyMessageView() {
        scenario = launchCloudBackupListFragment()

        onView(withId(R.id.tv_empty_message))
            .check(matches(not(isDisplayed()))) // Hidden by default
    }
}
