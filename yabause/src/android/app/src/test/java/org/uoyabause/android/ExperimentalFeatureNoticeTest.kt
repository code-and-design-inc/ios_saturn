package org.uoyabause.android

import android.content.SharedPreferences
import io.mockk.every
import io.mockk.mockk
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ExperimentalFeatureNoticeTest {
    @Test
    fun `prefKey is namespaced per toggle`() {
        assertEquals(
            "shown_experimental_notice_pref_use_compute_shader",
            ExperimentalFeatureNotice.prefKey("pref_use_compute_shader"),
        )
    }

    @Test
    fun `shouldShow is true when never shown`() {
        val prefs = mockk<SharedPreferences>()
        every {
            prefs.getBoolean("shown_experimental_notice_pref_polygon_generation_compute_rasterizer", false)
        } returns false
        assertTrue(
            ExperimentalFeatureNotice.shouldShow(prefs, "pref_polygon_generation_compute_rasterizer"),
        )
    }

    @Test
    fun `shouldShow is false when already shown`() {
        val prefs = mockk<SharedPreferences>()
        every {
            prefs.getBoolean("shown_experimental_notice_pref_polygon_generation_compute_rasterizer", false)
        } returns true
        assertFalse(
            ExperimentalFeatureNotice.shouldShow(prefs, "pref_polygon_generation_compute_rasterizer"),
        )
    }
}
