package org.uoyabause.android

import android.content.Context
import io.mockk.*
import org.junit.After
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config
import java.io.File
import java.util.zip.ZipFile

/**
 * Unit tests for ReportAttachmentManager
 *
 * Note: Tests focusing on pure logic methods (formatFileSize, estimateAttachmentSize, etc.)
 * that don't depend on native methods or complex singletons.
 */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], application = TestYabauseApplication::class)
class ReportAttachmentManagerTest {
    private lateinit var context: Context
    private lateinit var attachmentManager: ReportAttachmentManager
    private lateinit var tempDir: File

    @Before
    fun setUp() {
        context = RuntimeEnvironment.getApplication()
        attachmentManager = ReportAttachmentManager(context)

        // Create temporary directory for test files
        tempDir = File(context.cacheDir, "test_attachments")
        if (!tempDir.exists()) {
            tempDir.mkdirs()
        }
    }

    @After
    fun tearDown() {
        // Clean up temporary directory
        tempDir.deleteRecursively()
    }

    // ---------------------------------------
    // Pure logic tests (no dependencies)
    // ---------------------------------------

    @Test
    fun testEstimateAttachmentSize_NoAttachments() {
        val size =
            attachmentManager.estimateAttachmentSize(
                includeScreenshot = false,
                includeStateSave = false,
            )

        assertEquals("Size should be 0 when no attachments", 0L, size)
    }

    @Test
    fun testEstimateAttachmentSize_ScreenshotOnly() {
        val size =
            attachmentManager.estimateAttachmentSize(
                includeScreenshot = true,
                includeStateSave = false,
            )

        // Expected: 2MB for screenshot
        assertEquals("Screenshot should be estimated at 2MB", 2 * 1024 * 1024L, size)
    }

    @Test
    fun testEstimateAttachmentSize_StateSaveOnly() {
        val size =
            attachmentManager.estimateAttachmentSize(
                includeScreenshot = false,
                includeStateSave = true,
            )

        // Expected: 1MB for save state
        assertEquals("Save state should be estimated at 1MB", 1 * 1024 * 1024L, size)
    }

    @Test
    fun testEstimateAttachmentSize_BothAttachments() {
        val size =
            attachmentManager.estimateAttachmentSize(
                includeScreenshot = true,
                includeStateSave = true,
            )

        // Expected: 3MB total
        assertEquals("Both attachments should be estimated at 3MB", 3 * 1024 * 1024L, size)
    }

    @Test
    fun testFormatFileSize_Bytes() {
        val formatted = attachmentManager.formatFileSize(512)
        assertEquals("512 B", formatted)
    }

    @Test
    fun testFormatFileSize_Kilobytes() {
        val formatted = attachmentManager.formatFileSize(1536) // 1.5 KB
        assertEquals("1.5 KB", formatted)
    }

    @Test
    fun testFormatFileSize_Megabytes() {
        val formatted = attachmentManager.formatFileSize(2 * 1024 * 1024) // 2 MB
        assertEquals("2.0 MB", formatted)
    }

    @Test
    fun testFormatFileSize_LargeMegabytes() {
        val formatted = attachmentManager.formatFileSize(15 * 1024 * 1024 + 512 * 1024) // 15.5 MB
        assertEquals("15.5 MB", formatted)
    }

    // ---------------------------------------
    // File operation tests
    // ---------------------------------------

    @Test
    fun testCleanupFiles_SingleFile() {
        val testFile = File(tempDir, "test_cleanup.txt")
        testFile.writeText("test content")
        assertTrue("Test file should exist before cleanup", testFile.exists())

        attachmentManager.cleanupFiles(testFile)

        assertFalse("File should be deleted after cleanup", testFile.exists())
    }

    @Test
    fun testCleanupFiles_MultipleFiles() {
        val file1 = File(tempDir, "test1.txt")
        val file2 = File(tempDir, "test2.txt")
        val file3 = File(tempDir, "test3.txt")

        file1.writeText("test1")
        file2.writeText("test2")
        file3.writeText("test3")

        attachmentManager.cleanupFiles(file1, file2, file3)

        assertFalse("File 1 should be deleted", file1.exists())
        assertFalse("File 2 should be deleted", file2.exists())
        assertFalse("File 3 should be deleted", file3.exists())
    }

    @Test
    fun testCleanupFiles_NullFile() {
        // Should not throw exception
        attachmentManager.cleanupFiles(null)
    }

    @Test
    fun testCleanupFiles_NonExistentFile() {
        val nonExistent = File(tempDir, "does_not_exist.txt")

        // Should not throw exception
        attachmentManager.cleanupFiles(nonExistent)
    }

    @Test
    fun testShouldCompress_BelowThreshold() {
        val file1 = File(tempDir, "small1.txt")
        val file2 = File(tempDir, "small2.txt")

        file1.writeBytes(ByteArray(1024 * 1024)) // 1MB
        file2.writeBytes(ByteArray(1024 * 1024)) // 1MB

        val files = listOf(file1, file2)
        val threshold = 5 * 1024 * 1024L // 5MB

        val shouldCompress = attachmentManager.shouldCompress(files, threshold)

        assertFalse("Should not compress when below threshold", shouldCompress)

        // Cleanup
        file1.delete()
        file2.delete()
    }

    @Test
    fun testShouldCompress_AboveThreshold() {
        val file1 = File(tempDir, "large1.txt")
        val file2 = File(tempDir, "large2.txt")

        file1.writeBytes(ByteArray(3 * 1024 * 1024)) // 3MB
        file2.writeBytes(ByteArray(3 * 1024 * 1024)) // 3MB

        val files = listOf(file1, file2)
        val threshold = 5 * 1024 * 1024L // 5MB

        val shouldCompress = attachmentManager.shouldCompress(files, threshold)

        assertTrue("Should compress when above threshold", shouldCompress)

        // Cleanup
        file1.delete()
        file2.delete()
    }

    @Test
    fun testShouldCompress_EmptyList() {
        val shouldCompress = attachmentManager.shouldCompress(emptyList())
        assertFalse("Should not compress empty list", shouldCompress)
    }

    @Test
    fun testCompressFiles_Success() {
        val file1 = File(tempDir, "test1.txt")
        val file2 = File(tempDir, "test2.txt")

        file1.writeText("Content of file 1")
        file2.writeText("Content of file 2")

        val files = listOf(file1, file2)
        val zipFile = attachmentManager.compressFiles(files, "test_archive.zip", tempDir.absolutePath)

        assertNotNull("Compressed file should not be null", zipFile)
        assertTrue("Compressed file should exist", zipFile!!.exists())
        assertTrue("Compressed file should be a ZIP", zipFile.name.endsWith(".zip"))
        assertTrue("ZIP file should have size > 0", zipFile.length() > 0)

        // Verify ZIP contents
        ZipFile(zipFile).use { zip ->
            val entries = zip.entries().toList()
            assertEquals("ZIP should contain 2 files", 2, entries.size)

            val entry1 = zip.getEntry("test1.txt")
            assertNotNull("test1.txt should be in ZIP", entry1)

            val entry2 = zip.getEntry("test2.txt")
            assertNotNull("test2.txt should be in ZIP", entry2)

            // Verify content
            zip.getInputStream(entry1).use { input ->
                val content = input.bufferedReader().use { it.readText() }
                assertEquals("Content of file 1", content)
            }
        }

        // Cleanup
        file1.delete()
        file2.delete()
        zipFile.delete()
    }

    @Test
    fun testCompressFiles_EmptyList() {
        val zipFile = attachmentManager.compressFiles(emptyList())

        assertNull("Should return null for empty file list", zipFile)
    }

    @Test
    fun testCompressFiles_NonExistentFile() {
        val nonExistent = File(tempDir, "does_not_exist.txt")
        val existingFile = File(tempDir, "exists.txt")
        existingFile.writeText("This file exists")

        val files = listOf(nonExistent, existingFile)
        val zipFile = attachmentManager.compressFiles(files, "partial_archive.zip", tempDir.absolutePath)

        assertNotNull("Should still create ZIP with existing files", zipFile)
        assertTrue("Compressed file should exist", zipFile!!.exists())

        // Verify ZIP contains only the existing file
        ZipFile(zipFile).use { zip ->
            val entries = zip.entries().toList()
            assertEquals("ZIP should contain 1 file", 1, entries.size)
            assertNotNull("exists.txt should be in ZIP", zip.getEntry("exists.txt"))
            assertNull("does_not_exist.txt should not be in ZIP", zip.getEntry("does_not_exist.txt"))
        }

        // Cleanup
        existingFile.delete()
        zipFile.delete()
    }

    @Test
    fun testCompressFiles_LargeFiles() {
        val file1 = File(tempDir, "large_file.bin")

        // Create a 1MB file
        file1.writeBytes(ByteArray(1024 * 1024))

        val files = listOf(file1)
        val zipFile = attachmentManager.compressFiles(files, "large_archive.zip", tempDir.absolutePath)

        assertNotNull("Compressed file should not be null", zipFile)
        assertTrue("Compressed file should exist", zipFile!!.exists())

        // Verify the compressed size is smaller than the original
        assertTrue(
            "Compressed file should be smaller than original",
            zipFile.length() < file1.length(),
        )

        // Cleanup
        file1.delete()
        zipFile.delete()
    }
}
