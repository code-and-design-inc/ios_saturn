package org.uoyabause.android

import android.content.Context
import android.content.SharedPreferences
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28])
class GetStringSafeTest {
    private lateinit var context: Context
    private lateinit var prefs: SharedPreferences

    @Before
    fun setUp() {
        context = RuntimeEnvironment.getApplication()
        prefs = context.getSharedPreferences("test_prefs", Context.MODE_PRIVATE)
        prefs.edit().clear().apply()
    }

    // UT-E02: getStringSafe - Integer型保存値
    @Test
    fun `getStringSafe returns string for integer stored value`() {
        // Given
        prefs.edit().putInt("pref_video", 1).apply()

        // When
        val result = prefs.getStringSafe("pref_video", "-1")

        // Then
        assertEquals("1", result)
    }

    // UT-E03: getStringSafe - null値
    @Test
    fun `getStringSafe returns default for missing key`() {
        // Given: no value stored

        // When
        val result = prefs.getStringSafe("pref_video", "-1")

        // Then
        assertEquals("-1", result)
    }

    // UT-E04: getStringSafe - 正常String値
    @Test
    fun `getStringSafe returns string for string stored value`() {
        // Given
        prefs.edit().putString("pref_video", "1").apply()

        // When
        val result = prefs.getStringSafe("pref_video", "-1")

        // Then
        assertEquals("1", result)
    }

    @Test
    fun `getStringSafe returns default when both string and int fail`() {
        // Given: boolean stored (neither String nor Int)
        prefs.edit().putBoolean("pref_test", true).apply()

        // When
        val result = prefs.getStringSafe("pref_test", "default")

        // Then
        assertEquals("default", result)
    }
}
