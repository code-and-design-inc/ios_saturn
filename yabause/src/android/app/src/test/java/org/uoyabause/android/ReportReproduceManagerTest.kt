package org.uoyabause.android

import org.junit.Assert.*
import org.junit.Test
import java.io.File

/**
 * Unit tests for ReportReproduceManager data classes and basic functionality
 */
class ReportReproduceManagerTest {
    @Test
    fun `ReproduceResult with success true and all fields`() {
        // Given
        val savestateFile = File("/test/savestate.yss")
        val memoryFile = File("/test/memory.ram")
        val screenshotFile = File("/test/screenshot.png")
        val preferences =
            mapOf(
                "pref_bios" to "/test/bios",
                "pref_cpu" to "2",
            )

        // When
        val result =
            ReportReproduceManager.ReproduceResult(
                success = true,
                errorMessage = null,
                savestateFile = savestateFile,
                memoryFile = memoryFile,
                screenshotFile = screenshotFile,
                preferences = preferences,
            )

        // Then
        assertTrue("Success should be true", result.success)
        assertNull("Error message should be null", result.errorMessage)
        assertEquals("Savestate file should match", savestateFile, result.savestateFile)
        assertEquals("Memory file should match", memoryFile, result.memoryFile)
        assertEquals("Screenshot file should match", screenshotFile, result.screenshotFile)
        assertNotNull("Preferences should not be null", result.preferences)
        assertEquals("Preferences should have 2 entries", 2, result.preferences?.size)
        assertEquals("/test/bios", result.preferences?.get("pref_bios"))
        assertEquals("2", result.preferences?.get("pref_cpu"))
    }

    @Test
    fun `ReproduceResult with failure and error message`() {
        // Given & When
        val result =
            ReportReproduceManager.ReproduceResult(
                success = false,
                errorMessage = "Download failed",
                savestateFile = null,
                memoryFile = null,
                screenshotFile = null,
                preferences = null,
            )

        // Then
        assertFalse("Success should be false", result.success)
        assertEquals("Error message should match", "Download failed", result.errorMessage)
        assertNull("Savestate file should be null", result.savestateFile)
        assertNull("Memory file should be null", result.memoryFile)
        assertNull("Screenshot file should be null", result.screenshotFile)
        assertNull("Preferences should be null", result.preferences)
    }

    @Test
    fun `ReproduceResult default values`() {
        // Given & When
        val result = ReportReproduceManager.ReproduceResult()

        // Then
        assertFalse("Default success should be false", result.success)
        assertNull("Default error message should be null", result.errorMessage)
        assertNull("Default savestate file should be null", result.savestateFile)
        assertNull("Default memory file should be null", result.memoryFile)
        assertNull("Default screenshot file should be null", result.screenshotFile)
        assertNull("Default preferences should be null", result.preferences)
    }

    @Test
    fun `ReproduceResult with empty preferences map`() {
        // Given & When
        val result =
            ReportReproduceManager.ReproduceResult(
                success = true,
                preferences = emptyMap(),
            )

        // Then
        assertTrue("Success should be true", result.success)
        assertNotNull("Preferences should not be null", result.preferences)
        assertTrue("Preferences should be empty", result.preferences?.isEmpty() == true)
    }

    @Test
    fun `ReproduceResult with multiple preferences`() {
        // Given
        val preferences =
            mapOf(
                "pref_bios" to "/path/to/bios",
                "pref_cart" to "1",
                "pref_extend_internal_memory" to "true",
                "pref_cpu" to "2",
                "pref_use_cpu_affinity" to "false",
                "pref_use_sh2_cache" to "true",
                "pref_video" to "1",
                "pref_frameskip" to "0",
                "pref_landscape" to "true",
                "pref_rotate_screen" to "false",
                "pref_frameLimit" to "60",
                "pref_filter" to "1",
                "pref_polygon_generation" to "0",
                "pref_resolution" to "1",
                "pref_use_compute_shader" to "false",
                "pref_aspect_rate" to "0",
                "pref_rbg_resolution" to "1",
                "pref_sound_engine" to "0",
                "scsp_time_sync_mode" to "1",
                "pref_scsp_sync_per_frame" to "4",
            )

        // When
        val result =
            ReportReproduceManager.ReproduceResult(
                success = true,
                preferences = preferences,
            )

        // Then
        assertTrue("Success should be true", result.success)
        assertNotNull("Preferences should not be null", result.preferences)
        assertEquals("Should have 20 preferences", 20, result.preferences?.size)

        // Verify some specific keys
        assertEquals("/path/to/bios", result.preferences?.get("pref_bios"))
        assertEquals("2", result.preferences?.get("pref_cpu"))
        assertEquals("60", result.preferences?.get("pref_frameLimit"))
        assertEquals("1", result.preferences?.get("scsp_time_sync_mode"))
    }

    @Test
    fun `ReportData with preferences field`() {
        // Given
        val preferences =
            mapOf(
                "pref_bios" to "/test/bios",
                "pref_cpu" to "2",
            )

        // When
        val reportData =
            ReportData(
                id = "test123",
                uid = "user456",
                emulation_rating = 4,
                comment = "Works well",
                preferences = preferences,
            )

        // Then
        assertEquals("test123", reportData.id)
        assertEquals("user456", reportData.uid)
        assertEquals(4, reportData.emulation_rating)
        assertEquals("Works well", reportData.comment)
        assertNotNull("Preferences should not be null", reportData.preferences)
        assertEquals(2, reportData.preferences?.size)
        assertEquals("/test/bios", reportData.preferences?.get("pref_bios"))
    }

    @Test
    fun `ReportData without preferences field`() {
        // Given & When
        val reportData =
            ReportData(
                id = "test123",
                uid = "user456",
                emulation_rating = 4,
                comment = "Works well",
            )

        // Then
        assertEquals("test123", reportData.id)
        assertNull("Preferences should be null when not provided", reportData.preferences)
    }
}
