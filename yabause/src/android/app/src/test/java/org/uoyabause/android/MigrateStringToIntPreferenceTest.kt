package org.uoyabause.android

import android.content.Context
import android.content.SharedPreferences
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28])
class MigrateStringToIntPreferenceTest {
    private lateinit var context: Context
    private lateinit var prefs: SharedPreferences

    @Before
    fun setUp() {
        context = RuntimeEnvironment.getApplication()
        prefs = context.getSharedPreferences("migration_test_prefs", Context.MODE_PRIVATE)
        prefs.edit().clear().apply()
    }

    @Test
    fun `migrates string value to int and lets getInt succeed`() {
        prefs.edit().putString("pref_scsp_sync_per_frame", "5").apply()

        prefs.migrateStringToIntPreference("pref_scsp_sync_per_frame", defaultInt = 4, min = 1, max = 255)

        assertEquals(5, prefs.getInt("pref_scsp_sync_per_frame", -1))
    }

    @Test
    fun `leaves existing int value untouched`() {
        prefs.edit().putInt("pref_scsp_sync_per_frame", 7).apply()

        prefs.migrateStringToIntPreference("pref_scsp_sync_per_frame", defaultInt = 4, min = 1, max = 255)

        assertEquals(7, prefs.getInt("pref_scsp_sync_per_frame", -1))
    }

    @Test
    fun `no-op when key is missing`() {
        prefs.migrateStringToIntPreference("pref_scsp_sync_per_frame", defaultInt = 4, min = 1, max = 255)

        assertFalse(prefs.contains("pref_scsp_sync_per_frame"))
    }

    @Test
    fun `falls back to default when string is not numeric`() {
        prefs.edit().putString("pref_scsp_sync_per_frame", "abc").apply()

        prefs.migrateStringToIntPreference("pref_scsp_sync_per_frame", defaultInt = 4, min = 1, max = 255)

        assertEquals(4, prefs.getInt("pref_scsp_sync_per_frame", -1))
    }

    @Test
    fun `coerces value above max`() {
        prefs.edit().putString("pref_scsp_sync_per_frame", "9999").apply()

        prefs.migrateStringToIntPreference("pref_scsp_sync_per_frame", defaultInt = 4, min = 1, max = 255)

        assertEquals(255, prefs.getInt("pref_scsp_sync_per_frame", -1))
    }

    @Test
    fun `coerces value below min`() {
        prefs.edit().putString("pref_scsp_sync_per_frame", "0").apply()

        prefs.migrateStringToIntPreference("pref_scsp_sync_per_frame", defaultInt = 4, min = 1, max = 255)

        assertEquals(1, prefs.getInt("pref_scsp_sync_per_frame", -1))
    }

    @Test
    fun `migrated key is stored as int so getInt does not throw`() {
        prefs.edit().putString("pref_scsp_sync_per_frame", "10").apply()

        prefs.migrateStringToIntPreference("pref_scsp_sync_per_frame", defaultInt = 4, min = 1, max = 255)

        // Reading as int must not raise ClassCastException after migration
        val value = prefs.getInt("pref_scsp_sync_per_frame", -1)
        assertEquals(10, value)
        assertTrue(prefs.all["pref_scsp_sync_per_frame"] is Int)
    }
}
