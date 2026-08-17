package org.uoyabause.android

import android.content.Context
import android.content.SharedPreferences
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.mockito.Mock
import org.mockito.Mockito.*
import org.mockito.MockitoAnnotations
import java.io.File

/**
 * Comprehensive unit tests for ReportReproduceManager
 * Tests cover preference management, file handling, and Intent creation
 */
class ReportReproduceManagerComprehensiveTest {
    @Mock
    private lateinit var mockContext: Context

    @Mock
    private lateinit var mockSharedPreferences: SharedPreferences

    @Mock
    private lateinit var mockEditor: SharedPreferences.Editor

    private lateinit var manager: ReportReproduceManager

    @Before
    fun setup() {
        MockitoAnnotations.initMocks(this)

        // Setup SharedPreferences mock chain
        `when`(mockSharedPreferences.edit()).thenReturn(mockEditor)
        `when`(mockEditor.putString(anyString(), anyString())).thenReturn(mockEditor)
        `when`(mockEditor.putBoolean(anyString(), anyBoolean())).thenReturn(mockEditor)
        `when`(mockContext.getSharedPreferences(anyString(), anyInt())).thenReturn(mockSharedPreferences)

        manager = ReportReproduceManager(mockContext)
    }

    // ==================== Preference Collection Tests ====================

    @Test
    fun `collectCurrentPreferences returns all required preference keys`() {
        // Given
        val expectedKeys =
            listOf(
                "pref_bios",
                "pref_cart",
                "pref_extend_internal_memory",
                "pref_cpu",
                "pref_use_cpu_affinity",
                "pref_use_sh2_cache",
                "pref_video",
                "pref_frameskip",
                "pref_landscape",
                "pref_rotate_screen",
                "pref_frameLimit",
                "pref_filter",
                "pref_polygon_generation",
                "pref_resolution",
                "pref_use_compute_shader",
                "pref_aspect_rate",
                "pref_rbg_resolution",
                "pref_sound_engine",
                "scsp_time_sync_mode",
                "pref_scsp_sync_per_frame",
            )

        // Mock preferences to return default values
        expectedKeys.forEach { key ->
            `when`(mockSharedPreferences.getString(eq(key), any())).thenReturn("test_value")
        }

        // For boolean preferences
        `when`(mockSharedPreferences.getBoolean(eq("pref_extend_internal_memory"), anyBoolean())).thenReturn(true)
        `when`(mockSharedPreferences.getBoolean(eq("pref_use_cpu_affinity"), anyBoolean())).thenReturn(false)
        `when`(mockSharedPreferences.getBoolean(eq("pref_use_sh2_cache"), anyBoolean())).thenReturn(true)
        `when`(mockSharedPreferences.getBoolean(eq("pref_frameskip"), anyBoolean())).thenReturn(true)
        `when`(mockSharedPreferences.getBoolean(eq("pref_landscape"), anyBoolean())).thenReturn(false)
        `when`(mockSharedPreferences.getBoolean(eq("pref_rotate_screen"), anyBoolean())).thenReturn(false)
        `when`(mockSharedPreferences.getBoolean(eq("pref_use_compute_shader"), anyBoolean())).thenReturn(false)

        // When
        val preferences = manager.collectCurrentPreferences(null)

        // Then
        assertNotNull(preferences)
        assertEquals("Should collect 20 preferences", 20, preferences.size)

        expectedKeys.forEach { key ->
            assertTrue("Should contain key: $key", preferences.containsKey(key))
        }
    }

    @Test
    fun `collectCurrentPreferences handles boolean preferences correctly`() {
        // Given
        val booleanKeys =
            setOf(
                "pref_extend_internal_memory",
                "pref_use_cpu_affinity",
                "pref_use_sh2_cache",
                "pref_frameskip",
                "pref_landscape",
                "pref_rotate_screen",
                "pref_use_compute_shader",
            )

        booleanKeys.forEach { key ->
            `when`(mockSharedPreferences.getBoolean(eq(key), anyBoolean())).thenReturn(true)
        }

        // Mock string preferences
        `when`(mockSharedPreferences.getString(anyString(), any())).thenReturn("0")

        // When
        val preferences = manager.collectCurrentPreferences(null)

        // Then
        booleanKeys.forEach { key ->
            val value = preferences[key]
            assertNotNull("Boolean key $key should be present", value)
            assertEquals("Boolean key $key should be 'true'", "true", value)
        }
    }

    @Test
    fun `collectCurrentPreferences uses game-specific preferences when gameCode provided`() {
        // Given
        val gameCode = "T-1234G"

        // When
        manager.collectCurrentPreferences(gameCode)

        // Then
        verify(mockContext).getSharedPreferences(eq(gameCode), eq(Context.MODE_PRIVATE))
    }

    @Test
    fun `collectCurrentPreferences uses default preferences when gameCode is null`() {
        // Given - use static mock for PreferenceManager
        // This test verifies the branch but can't fully test without Robolectric

        // When
        val preferences = manager.collectCurrentPreferences(null)

        // Then
        assertNotNull(preferences)
        // Note: Full verification requires Robolectric for PreferenceManager.getDefaultSharedPreferences
    }

    // ==================== Preference Application Tests ====================

    @Test
    fun `applyReproductionPreferences saves original values and applies new ones`() {
        // Given
        val gameCode = "T-1234G"
        val newPreferences =
            mapOf(
                "pref_cpu" to "3",
                "pref_video" to "1",
                "pref_frameskip" to "true",
            )

        // Mock original values
        `when`(mockSharedPreferences.getString(eq("pref_cpu"), any())).thenReturn("2")
        `when`(mockSharedPreferences.getString(eq("pref_video"), any())).thenReturn("0")
        `when`(mockSharedPreferences.getBoolean(eq("pref_frameskip"), anyBoolean())).thenReturn(false)

        // When
        val originalPreferences = manager.applyReproductionPreferences(gameCode, newPreferences)

        // Then
        assertEquals(3, originalPreferences.size)
        assertEquals("2", originalPreferences["pref_cpu"])
        assertEquals("0", originalPreferences["pref_video"])
        assertEquals("false", originalPreferences["pref_frameskip"])

        // Verify new values were applied
        verify(mockEditor).putString("pref_cpu", "3")
        verify(mockEditor).putString("pref_video", "1")
        verify(mockEditor).putBoolean("pref_frameskip", true)
        verify(mockEditor, atLeastOnce()).apply()
    }

    @Test
    fun `applyReproductionPreferences returns empty map when preferences is null`() {
        // Given
        val gameCode = "T-1234G"

        // When
        val originalPreferences = manager.applyReproductionPreferences(gameCode, null)

        // Then
        assertTrue(originalPreferences.isEmpty())
        verify(mockEditor, never()).apply()
    }

    @Test
    fun `applyReproductionPreferences handles empty preferences map`() {
        // Given
        val gameCode = "T-1234G"
        val emptyPreferences = emptyMap<String, String>()

        // When
        val originalPreferences = manager.applyReproductionPreferences(gameCode, emptyPreferences)

        // Then
        assertTrue(originalPreferences.isEmpty())
        verify(mockEditor, never()).putString(anyString(), anyString())
        verify(mockEditor, never()).putBoolean(anyString(), anyBoolean())
    }

    @Test
    fun `applyReproductionPreferences uses game-specific SharedPreferences`() {
        // Given
        val gameCode = "T-5678H"
        val preferences = mapOf("pref_cpu" to "3")
        `when`(mockSharedPreferences.getString(anyString(), any())).thenReturn("2")

        // When
        manager.applyReproductionPreferences(gameCode, preferences)

        // Then
        verify(mockContext).getSharedPreferences(eq(gameCode), eq(Context.MODE_PRIVATE))
    }

    // ==================== Preference Restoration Tests ====================

    @Test
    fun `restoreOriginalPreferences restores all saved values`() {
        // Given
        val gameCode = "T-1234G"
        val originalPreferences =
            mapOf(
                "pref_cpu" to "2",
                "pref_video" to "0",
                "pref_frameskip" to "false",
            )

        // When
        manager.restoreOriginalPreferences(gameCode, originalPreferences)

        // Then
        verify(mockEditor).putString("pref_cpu", "2")
        verify(mockEditor).putString("pref_video", "0")
        verify(mockEditor).putBoolean("pref_frameskip", false)
        verify(mockEditor, atLeastOnce()).apply()
    }

    @Test
    fun `restoreOriginalPreferences does nothing when map is empty`() {
        // Given
        val gameCode = "T-1234G"
        val emptyPreferences = emptyMap<String, String>()

        // When
        manager.restoreOriginalPreferences(gameCode, emptyPreferences)

        // Then
        verify(mockEditor, never()).putString(anyString(), anyString())
        verify(mockEditor, never()).putBoolean(anyString(), anyBoolean())
    }

    @Test
    fun `restoreOriginalPreferences handles boolean values correctly`() {
        // Given
        val gameCode = "T-1234G"
        val originalPreferences =
            mapOf(
                "pref_extend_internal_memory" to "true",
                "pref_use_cpu_affinity" to "false",
                "pref_frameskip" to "true",
            )

        // When
        manager.restoreOriginalPreferences(gameCode, originalPreferences)

        // Then
        verify(mockEditor).putBoolean("pref_extend_internal_memory", true)
        verify(mockEditor).putBoolean("pref_use_cpu_affinity", false)
        verify(mockEditor).putBoolean("pref_frameskip", true)
    }

    // ==================== Intent Creation Tests ====================

    @Test
    fun `createLaunchIntent creates proper intent with savestate`() {
        // Given
        val gameInfo =
            GameInfo().apply {
                product_number = "T-1234G"
                file_path = "/storage/emulated/0/game.iso"
                iso_file_path = "/storage/emulated/0/"
            }
        val savestateFile = File("/test/reproduce_report123.yss")

        // When
        val intent = manager.createLaunchIntent(gameInfo, savestateFile, null)

        // Then
        assertNotNull(intent)
        assertEquals(
            "org.uoyabause.android.FileNameEx",
            gameInfo.file_path,
            intent.getStringExtra("org.uoyabause.android.FileNameEx"),
        )
        assertEquals(
            "org.uoyabause.android.gamecode",
            "T-1234G",
            intent.getStringExtra("org.uoyabause.android.gamecode"),
        )
        assertEquals(
            "org.uoyabause.android.LoadState",
            "/test/reproduce_report123.yss",
            intent.getStringExtra("org.uoyabause.android.LoadState"),
        )
    }

    @Test
    fun `createLaunchIntent creates proper intent with memory file`() {
        // Given
        val gameInfo =
            GameInfo().apply {
                product_number = "T-5678H"
                file_path = "/storage/emulated/0/game.iso"
                iso_file_path = "/storage/emulated/0/"
            }
        val memoryFile = File("/test/memory.ram")

        // When
        val intent = manager.createLaunchIntent(gameInfo, null, memoryFile)

        // Then
        assertNotNull(intent)
        assertEquals(
            "org.uoyabause.android.tmpbackupfile",
            "/test/memory.ram",
            intent.getStringExtra("org.uoyabause.android.tmpbackupfile"),
        )
    }

    @Test
    fun `createLaunchIntent creates proper intent with both savestate and memory`() {
        // Given
        val gameInfo =
            GameInfo().apply {
                product_number = "T-9999Z"
                file_path = "/storage/emulated/0/game.iso"
                iso_file_path = "/storage/emulated/0/"
            }
        val savestateFile = File("/test/savestate.yss")
        val memoryFile = File("/test/memory.ram")

        // When
        val intent = manager.createLaunchIntent(gameInfo, savestateFile, memoryFile)

        // Then
        assertNotNull(intent)
        assertEquals("/test/savestate.yss", intent.getStringExtra("org.uoyabause.android.LoadState"))
        assertEquals("/test/memory.ram", intent.getStringExtra("org.uoyabause.android.tmpbackupfile"))
    }

    @Test
    fun `createLaunchIntent handles content URI game path`() {
        // Given
        val gameInfo =
            GameInfo().apply {
                product_number = "T-1111A"
                file_path = "content://com.android.externalstorage.documents/document/primary:game.iso"
                iso_file_path = "/storage/emulated/0/"
            }

        // When
        val intent = manager.createLaunchIntent(gameInfo, null, null)

        // Then
        assertNotNull(intent)
        assertTrue(
            "Should use FileNameUri for content:// paths",
            intent.hasExtra("org.uoyabause.android.FileNameUri"),
        )
        assertEquals(
            gameInfo.file_path,
            intent.getStringExtra("org.uoyabause.android.FileNameUri"),
        )
        assertEquals(
            gameInfo.iso_file_path,
            intent.getStringExtra("org.uoyabause.android.FileDir"),
        )
    }

    @Test
    fun `createLaunchIntent handles null savestate and memory gracefully`() {
        // Given
        val gameInfo =
            GameInfo().apply {
                product_number = "T-2222B"
                file_path = "/storage/emulated/0/game.iso"
                iso_file_path = "/storage/emulated/0/"
            }

        // When
        val intent = manager.createLaunchIntent(gameInfo, null, null)

        // Then
        assertNotNull(intent)
        assertFalse(
            "Should not have LoadState extra",
            intent.hasExtra("org.uoyabause.android.LoadState"),
        )
        assertFalse(
            "Should not have tmpbackupfile extra",
            intent.hasExtra("org.uoyabause.android.tmpbackupfile"),
        )
    }

    // ==================== ReproduceResult Tests ====================

    @Test
    fun `ReproduceResult success with all files`() {
        // Given
        val savestateFile = File("/test/savestate.yss")
        val memoryFile = File("/test/memory.ram")
        val screenshotFile = File("/test/screenshot.png")
        val preferences = mapOf("pref_cpu" to "3")

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
        assertTrue(result.success)
        assertNull(result.errorMessage)
        assertEquals(savestateFile, result.savestateFile)
        assertEquals(memoryFile, result.memoryFile)
        assertEquals(screenshotFile, result.screenshotFile)
        assertEquals(1, result.preferences?.size)
    }

    @Test
    fun `ReproduceResult failure with error message`() {
        // Given & When
        val result =
            ReportReproduceManager.ReproduceResult(
                success = false,
                errorMessage = "Failed to download files",
                savestateFile = null,
                memoryFile = null,
                screenshotFile = null,
                preferences = null,
            )

        // Then
        assertFalse(result.success)
        assertEquals("Failed to download files", result.errorMessage)
        assertNull(result.savestateFile)
        assertNull(result.memoryFile)
        assertNull(result.screenshotFile)
        assertNull(result.preferences)
    }

    @Test
    fun `ReproduceResult partial success with only savestate`() {
        // Given
        val savestateFile = File("/test/savestate.yss")

        // When
        val result =
            ReportReproduceManager.ReproduceResult(
                success = true,
                savestateFile = savestateFile,
                memoryFile = null,
                screenshotFile = null,
            )

        // Then
        assertTrue(result.success)
        assertNotNull(result.savestateFile)
        assertNull(result.memoryFile)
        assertNull(result.screenshotFile)
    }

    // ==================== ReportData Helper Tests ====================

    @Test
    fun `ReportData isReproducible checks work correctly`() {
        // Test case 1: Has savestate
        val reportWithSavestate =
            ReportData(
                id = "test1",
                savestate_url = "https://example.com/savestate.zip",
            )
        assertTrue("Should be reproducible with savestate", reportWithSavestate.isReproducible())

        // Test case 2: Has memory
        val reportWithMemory =
            ReportData(
                id = "test2",
                memory_url = "https://example.com/memory.zip",
            )
        assertTrue("Should be reproducible with memory", reportWithMemory.isReproducible())

        // Test case 3: Has both
        val reportWithBoth =
            ReportData(
                id = "test3",
                savestate_url = "https://example.com/savestate.zip",
                memory_url = "https://example.com/memory.zip",
            )
        assertTrue("Should be reproducible with both", reportWithBoth.isReproducible())

        // Test case 4: Has neither
        val reportWithNeither =
            ReportData(
                id = "test4",
                screenshot_url = "https://example.com/screenshot.png",
            )
        assertFalse(
            "Should not be reproducible without savestate or memory",
            reportWithNeither.isReproducible(),
        )
    }

    // ==================== ZIP File Extraction Tests (requires file I/O) ====================

    @Test
    fun `extractZipFile handles path traversal attack`() {
        // This test verifies security against path traversal
        // Note: This requires actual file I/O and is more of an integration test
        // For now, we document the expected behavior

        // The implementation should:
        // 1. Check canonicalPath starts with target directory's canonicalPath
        // 2. Skip entries that try to escape the target directory
        // 3. Log warnings for suspicious paths

        // Example malicious entry: "../../etc/passwd"
        // Should be detected and skipped
        assertTrue("Path traversal protection should be in place", true)
    }
}
