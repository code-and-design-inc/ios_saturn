package org.uoyabause.android

import com.google.firebase.Timestamp
import org.junit.Assert.*
import org.junit.Test
import java.util.Date

/**
 * Comprehensive unit tests for ReportData class
 */
class ReportDataTest {
    @Test
    fun `ReportData with all fields populated`() {
        // Given
        val timestamp = Timestamp(Date())
        val preferences =
            mapOf(
                "pref_bios" to "/test/bios",
                "pref_cpu" to "2",
            )

        // When
        val reportData =
            ReportData(
                id = "report123",
                uid = "user456",
                display_name = "Test User",
                photo_url = "https://example.com/photo.jpg",
                rating = 5,
                emulation_rating = 4,
                comment = "Great emulator!",
                platform = "Android",
                version = "1.18.8",
                version_code = 118,
                timestamp = timestamp,
                isVisible = true,
                has_attachments = true,
                screenshot_url = "https://storage.example.com/screenshot.png",
                savestate_url = "https://storage.example.com/savestate.zip",
                memory_url = "https://storage.example.com/memory.zip",
                attachment_size = 1024 * 1024 * 5, // 5MB
                preferences = preferences,
            )

        // Then
        assertEquals("report123", reportData.id)
        assertEquals("user456", reportData.uid)
        assertEquals("Test User", reportData.display_name)
        assertEquals(5, reportData.rating)
        assertEquals(4, reportData.emulation_rating)
        assertEquals("Great emulator!", reportData.comment)
        assertTrue(reportData.isVisible)
        assertTrue(reportData.has_attachments)
        assertNotNull(reportData.preferences)
        assertEquals(2, reportData.preferences?.size)
    }

    @Test
    fun `getFormattedTimestamp returns formatted date`() {
        // Given
        val date = Date(1736694000000L) // 2025-01-12 12:00:00 UTC
        val timestamp = Timestamp(date)
        val reportData =
            ReportData(
                id = "test",
                timestamp = timestamp,
            )

        // When
        val formatted = reportData.getFormattedTimestamp()

        // Then
        assertNotNull(formatted)
        assertTrue(formatted.contains("2025"))
        assertTrue(formatted.contains("/"))
        assertFalse(formatted.contains("Unknown"))
    }

    @Test
    fun `getFormattedTimestamp returns Unknown when timestamp is null`() {
        // Given
        val reportData =
            ReportData(
                id = "test",
                timestamp = null,
            )

        // When
        val formatted = reportData.getFormattedTimestamp()

        // Then
        assertEquals("Unknown", formatted)
    }

    @Test
    fun `getRatingStars returns correct number of stars`() {
        // Test cases for different ratings
        val testCases =
            listOf(
                0 to "",
                1 to "★",
                2 to "★★",
                3 to "★★★",
                4 to "★★★★",
                5 to "★★★★★",
            )

        testCases.forEach { (rating, expectedStars) ->
            // Given
            val reportData =
                ReportData(
                    id = "test",
                    emulation_rating = rating,
                )

            // When
            val stars = reportData.getRatingStars()

            // Then
            assertEquals("Rating $rating should produce $expectedStars", expectedStars, stars)
        }
    }

    @Test
    fun `hasAnyAttachments returns true when screenshot_url is present`() {
        // Given
        val reportData =
            ReportData(
                id = "test",
                screenshot_url = "https://example.com/screenshot.png",
            )

        // When & Then
        assertTrue(reportData.hasAnyAttachments())
    }

    @Test
    fun `hasAnyAttachments returns true when savestate_url is present`() {
        // Given
        val reportData =
            ReportData(
                id = "test",
                savestate_url = "https://example.com/savestate.zip",
            )

        // When & Then
        assertTrue(reportData.hasAnyAttachments())
    }

    @Test
    fun `hasAnyAttachments returns true when memory_url is present`() {
        // Given
        val reportData =
            ReportData(
                id = "test",
                memory_url = "https://example.com/memory.zip",
            )

        // When & Then
        assertTrue(reportData.hasAnyAttachments())
    }

    @Test
    fun `hasAnyAttachments returns true when all attachments are present`() {
        // Given
        val reportData =
            ReportData(
                id = "test",
                screenshot_url = "https://example.com/screenshot.png",
                savestate_url = "https://example.com/savestate.zip",
                memory_url = "https://example.com/memory.zip",
            )

        // When & Then
        assertTrue(reportData.hasAnyAttachments())
    }

    @Test
    fun `hasAnyAttachments returns false when no attachments`() {
        // Given
        val reportData =
            ReportData(
                id = "test",
                screenshot_url = null,
                savestate_url = null,
                memory_url = null,
            )

        // When & Then
        assertFalse(reportData.hasAnyAttachments())
    }

    @Test
    fun `hasAnyAttachments returns false when attachments are empty strings`() {
        // Given
        val reportData =
            ReportData(
                id = "test",
                screenshot_url = "",
                savestate_url = "",
                memory_url = "",
            )

        // When & Then
        assertFalse(reportData.hasAnyAttachments())
    }

    @Test
    fun `getFormattedAttachmentSize formats bytes correctly`() {
        // Given
        val reportData =
            ReportData(
                id = "test",
                attachment_size = 512,
            )

        // When
        val formatted = reportData.getFormattedAttachmentSize()

        // Then
        assertEquals("512 B", formatted)
    }

    @Test
    fun `getFormattedAttachmentSize formats kilobytes correctly`() {
        // Given
        val reportData =
            ReportData(
                id = "test",
                attachment_size = 1024 * 5, // 5KB
            )

        // When
        val formatted = reportData.getFormattedAttachmentSize()

        // Then
        assertTrue(formatted.contains("5.0 KB"))
    }

    @Test
    fun `getFormattedAttachmentSize formats megabytes correctly`() {
        // Given
        val reportData =
            ReportData(
                id = "test",
                attachment_size = 1024 * 1024 * 10, // 10MB
            )

        // When
        val formatted = reportData.getFormattedAttachmentSize()

        // Then
        assertTrue(formatted.contains("10.0 MB"))
    }

    @Test
    fun `getFormattedAttachmentSize handles zero size`() {
        // Given
        val reportData =
            ReportData(
                id = "test",
                attachment_size = 0,
            )

        // When
        val formatted = reportData.getFormattedAttachmentSize()

        // Then
        assertEquals("0 B", formatted)
    }

    @Test
    fun `isReproducible returns true when savestate_url is present`() {
        // Given
        val reportData =
            ReportData(
                id = "test",
                savestate_url = "https://example.com/savestate.zip",
            )

        // When & Then
        assertTrue(reportData.isReproducible())
    }

    @Test
    fun `isReproducible returns true when memory_url is present`() {
        // Given
        val reportData =
            ReportData(
                id = "test",
                memory_url = "https://example.com/memory.zip",
            )

        // When & Then
        assertTrue(reportData.isReproducible())
    }

    @Test
    fun `isReproducible returns true when both savestate and memory are present`() {
        // Given
        val reportData =
            ReportData(
                id = "test",
                savestate_url = "https://example.com/savestate.zip",
                memory_url = "https://example.com/memory.zip",
            )

        // When & Then
        assertTrue(reportData.isReproducible())
    }

    @Test
    fun `isReproducible returns false when no reproduction data`() {
        // Given
        val reportData =
            ReportData(
                id = "test",
                screenshot_url = "https://example.com/screenshot.png", // Only screenshot
            )

        // When & Then
        assertFalse(reportData.isReproducible())
    }

    @Test
    fun `isReproducible returns false when URLs are empty strings`() {
        // Given
        val reportData =
            ReportData(
                id = "test",
                savestate_url = "",
                memory_url = "",
            )

        // When & Then
        assertFalse(reportData.isReproducible())
    }

    @Test
    fun `ReportData with default values`() {
        // Given & When
        val reportData = ReportData()

        // Then
        assertEquals("", reportData.id)
        assertEquals("", reportData.uid)
        assertNull(reportData.display_name)
        assertEquals(0, reportData.rating)
        assertEquals(0, reportData.emulation_rating)
        assertEquals("", reportData.comment)
        assertTrue(reportData.isVisible) // default is true
        assertFalse(reportData.has_attachments)
        assertNull(reportData.screenshot_url)
        assertNull(reportData.savestate_url)
        assertNull(reportData.memory_url)
        assertEquals(0L, reportData.attachment_size)
        assertNull(reportData.preferences)
    }

    @Test
    fun `ReportData preferences field with multiple keys`() {
        // Given
        val preferences =
            mapOf(
                "pref_bios" to "/path/to/bios",
                "pref_cpu" to "3",
                "pref_video" to "1",
                "pref_frameskip" to "true",
                "pref_landscape" to "false",
            )

        // When
        val reportData =
            ReportData(
                id = "test",
                preferences = preferences,
            )

        // Then
        assertNotNull(reportData.preferences)
        assertEquals(5, reportData.preferences?.size)
        assertEquals("/path/to/bios", reportData.preferences?.get("pref_bios"))
        assertEquals("3", reportData.preferences?.get("pref_cpu"))
        assertEquals("1", reportData.preferences?.get("pref_video"))
        assertEquals("true", reportData.preferences?.get("pref_frameskip"))
        assertEquals("false", reportData.preferences?.get("pref_landscape"))
    }

    @Test
    fun `ReportData preferences field with empty map`() {
        // Given
        val reportData =
            ReportData(
                id = "test",
                preferences = emptyMap(),
            )

        // When & Then
        assertNotNull(reportData.preferences)
        assertTrue(reportData.preferences?.isEmpty() == true)
    }

    @Test
    fun `ReportData visibility flag affects data`() {
        // Test visible report
        val visibleReport =
            ReportData(
                id = "test1",
                isVisible = true,
            )
        assertTrue(visibleReport.isVisible)

        // Test hidden report
        val hiddenReport =
            ReportData(
                id = "test2",
                isVisible = false,
            )
        assertFalse(hiddenReport.isVisible)
    }

    @Test
    fun `ReportData with long comment`() {
        // Given
        val longComment = "A".repeat(1000)
        val reportData =
            ReportData(
                id = "test",
                comment = longComment,
            )

        // When & Then
        assertEquals(1000, reportData.comment.length)
        assertEquals(longComment, reportData.comment)
    }

    @Test
    fun `ReportData with special characters in comment`() {
        // Given
        val specialComment = "Test with\nemojis 😀🎮 and special chars: <>&\""
        val reportData =
            ReportData(
                id = "test",
                comment = specialComment,
            )

        // When & Then
        assertEquals(specialComment, reportData.comment)
    }
}
