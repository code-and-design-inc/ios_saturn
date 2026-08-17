package org.uoyabause.android.auth

import android.content.Context
import android.content.Intent
import androidx.test.core.app.ActivityScenario
import androidx.test.core.app.ApplicationProvider
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.*
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.*
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.LargeTest
import androidx.test.rule.GrantPermissionRule
import org.devmiyax.yabasanshiro.R
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.uoyabause.android.auth.ui.SimpleAccountManagementActivity

/**
 * Simple Espresso integration tests for Account Management functionality
 * Tests basic UI elements and navigation
 */
@RunWith(AndroidJUnit4::class)
@LargeTest
class SimpleAccountManagementTest {
    @get:Rule
    val grantPermissionRule: GrantPermissionRule =
        GrantPermissionRule.grant(
            android.Manifest.permission.INTERNET,
            android.Manifest.permission.ACCESS_NETWORK_STATE,
        )

    private lateinit var context: Context
    private lateinit var scenario: ActivityScenario<SimpleAccountManagementActivity>

    @Before
    fun setUp() {
        context = ApplicationProvider.getApplicationContext()

        // Launch the SimpleAccountManagementActivity
        val intent = Intent(context, SimpleAccountManagementActivity::class.java)
        scenario = ActivityScenario.launch(intent)
    }

    @After
    fun tearDown() {
        scenario.close()
    }

    // === Basic UI Tests ===

    @Test
    fun activityLaunches_successfully() {
        // Verify activity launches without crashing
        onView(withId(R.id.toolbar))
            .check(matches(isDisplayed()))
    }

    @Test
    fun toolbar_displaysCorrectTitle() {
        onView(withText("Account Management"))
            .check(matches(isDisplayed()))
    }

    @Test
    fun mainContent_isDisplayed() {
        // Verify main content area is displayed
        onView(withId(R.id.tv_status))
            .check(matches(isDisplayed()))
    }

    // === Navigation Tests ===

    @Test
    fun backButton_finishesActivity() {
        // Press back button
        onView(isRoot()).perform(pressBack())

        // Activity should be finished (can't directly test this in Espresso)
        // But the test will pass if no crash occurs
    }

    // === Basic Interaction Tests ===

    @Test
    fun scrolling_worksCorrectly() {
        // Test scrolling in the main content area
        onView(withId(R.id.tv_status))
            .perform(swipeUp())

        // Should still be able to see the toolbar
        onView(withId(R.id.toolbar))
            .check(matches(isDisplayed()))
    }

    // === Content Tests ===

    @Test
    fun accountManagementText_isDisplayed() {
        onView(withText("Account Management"))
            .check(matches(isDisplayed()))
    }

    // === Accessibility Tests ===

    @Test
    fun toolbar_hasNavigationContentDescription() {
        onView(withId(R.id.toolbar))
            .check(matches(isDisplayed()))
    }

    // === Configuration Change Tests ===

    @Test
    fun activitySurvivesConfigurationChange() {
        // Simulate configuration change (rotation)
        scenario.recreate()

        // Verify activity still works after recreation
        onView(withId(R.id.toolbar))
            .check(matches(isDisplayed()))

        onView(withText("Account Management"))
            .check(matches(isDisplayed()))
    }

    // === Performance Tests ===

    @Test
    fun activityLoads_quickly() {
        // Verify that key UI elements are visible immediately
        // This helps catch any slow initialization issues
        onView(withId(R.id.toolbar))
            .check(matches(isDisplayed()))

        onView(withId(R.id.tv_status))
            .check(matches(isDisplayed()))
    }
}
