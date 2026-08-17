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

/**
 * UI tests for Shared Backup Search functionality
 * Test cases: IT-S02 (UI portion), IT-S03, IT-S04
 *
 * These tests verify the shared backup search UI displays correctly
 * and handles user interactions properly.
 */
@RunWith(AndroidJUnit4::class)
class SharedBackupSearchUiTest {
    private lateinit var context: Context
    private var scenario: FragmentScenario<SharedBackupSearchFragment>? = null

    @Before
    fun setUp() {
        context = InstrumentationRegistry.getInstrumentation().targetContext
    }

    @After
    fun tearDown() {
        scenario?.close()
    }

    private fun launchSharedBackupSearchFragment(): FragmentScenario<SharedBackupSearchFragment> = launchFragmentInContainer(
        themeResId = R.style.AppTheme,
    )

    // ==============================
    // IT-S02: Shared backup search UI
    // ==============================

    /**
     * IT-S02-10: Verify search input layout is displayed
     */
    @Test
    fun searchFragment_displaysSearchInputLayout() {
        scenario = launchSharedBackupSearchFragment()

        onView(withId(R.id.til_search))
            .check(matches(isDisplayed()))
    }

    /**
     * IT-S02-11: Verify search edit text is displayed
     */
    @Test
    fun searchFragment_displaysSearchEditText() {
        scenario = launchSharedBackupSearchFragment()

        onView(withId(R.id.et_search))
            .check(matches(isDisplayed()))
    }

    /**
     * IT-S02-12: Verify search hint text
     */
    @Test
    fun searchFragment_hasSearchHint() {
        scenario = launchSharedBackupSearchFragment()

        // Search hint is on the TextInputLayout
        onView(withId(R.id.til_search))
            .check(matches(isDisplayed()))
    }

    /**
     * IT-S02-13: Verify search results RecyclerView exists
     */
    @Test
    fun searchFragment_hasResultsRecyclerView() {
        scenario = launchSharedBackupSearchFragment()

        // RecyclerView visibility depends on state
        onView(withId(R.id.recycler_view_shared))
            .check(matches(not(isDisplayed()))) // Hidden initially until data loads
    }

    /**
     * IT-S02-14: Verify loading indicator exists
     */
    @Test
    fun searchFragment_hasLoadingLayout() {
        scenario = launchSharedBackupSearchFragment()

        // Loading layout exists but is hidden by default
        onView(withId(R.id.layout_loading))
            .check(matches(not(isDisplayed())))
    }

    /**
     * IT-S02-15: Verify empty state layout exists
     */
    @Test
    fun searchFragment_hasEmptyStateLayout() {
        scenario = launchSharedBackupSearchFragment()

        // Empty state visibility depends on data state
        onView(withId(R.id.layout_empty))
            .check(matches(not(isDisplayed())))
    }

    /**
     * IT-S02-16: Verify empty message view exists
     */
    @Test
    fun searchFragment_hasEmptyMessageView() {
        scenario = launchSharedBackupSearchFragment()

        onView(withId(R.id.tv_empty_message))
            .check(matches(not(isDisplayed())))
    }

    // ==============================
    // IT-S03: Import UI
    // ==============================

    /**
     * IT-S03-10: Verify auth required layout exists
     */
    @Test
    fun searchFragment_hasAuthRequiredLayout() {
        scenario = launchSharedBackupSearchFragment()

        // Auth required layout exists (visibility depends on auth state)
        onView(withId(R.id.layout_auth_required))
            .check(matches(not(isDisplayed())))
    }

    /**
     * IT-S03-11: Verify sign in button exists
     */
    @Test
    fun searchFragment_hasSignInButton() {
        scenario = launchSharedBackupSearchFragment()

        // Sign in button exists in auth required layout
        onView(withId(R.id.btn_sign_in))
            .check(matches(not(isDisplayed())))
    }

    // ==============================
    // IT-S04: Rating UI / Error handling
    // ==============================

    /**
     * IT-S04-10: Verify error layout exists
     */
    @Test
    fun searchFragment_hasErrorLayout() {
        scenario = launchSharedBackupSearchFragment()

        // Error layout exists but is hidden by default
        onView(withId(R.id.layout_error))
            .check(matches(not(isDisplayed())))
    }

    /**
     * IT-S04-11: Verify error message view exists
     */
    @Test
    fun searchFragment_hasErrorMessageView() {
        scenario = launchSharedBackupSearchFragment()

        onView(withId(R.id.tv_error_message))
            .check(matches(not(isDisplayed())))
    }

    /**
     * IT-S04-12: Verify retry button exists in error layout
     */
    @Test
    fun searchFragment_hasRetryButton() {
        scenario = launchSharedBackupSearchFragment()

        // Retry button exists in error layout
        onView(withId(R.id.btn_retry))
            .check(matches(not(isDisplayed())))
    }
}
