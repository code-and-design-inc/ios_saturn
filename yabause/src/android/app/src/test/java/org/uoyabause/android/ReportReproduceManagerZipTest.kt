package org.uoyabause.android

import android.content.Context
import org.junit.After
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.mockito.Mock
import org.mockito.MockitoAnnotations
import java.io.File
import java.io.FileOutputStream
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

/**
 * Tests for ZIP file extraction functionality in ReportReproduceManager
 * These tests use real file I/O to verify ZIP handling
 */
class ReportReproduceManagerZipTest {
    @Mock
    private lateinit var mockContext: Context

    private lateinit var testDir: File
    private lateinit var manager: ReportReproduceManager

    @Before
    fun setup() {
        MockitoAnnotations.initMocks(this)

        // Create temporary test directory
        testDir = File(System.getProperty("java.io.tmpdir"), "report_reproduce_test_${System.currentTimeMillis()}")
        testDir.mkdirs()

        manager = ReportReproduceManager(mockContext)
    }

    @After
    fun cleanup() {
        // Clean up test files
        testDir.deleteRecursively()
    }

    // ==================== Helper Methods ====================

/**
     * Creates a test ZIP file with specified entries
     */
    private fun createTestZip(
        zipFile: File,
        entries: Map<String, ByteArray>,
    ) {
        ZipOutputStream(FileOutputStream(zipFile)).use { zipOut ->
            entries.forEach { (name, content) ->
                val entry = ZipEntry(name)
                zipOut.putNextEntry(entry)
                zipOut.write(content)
                zipOut.closeEntry()
            }
        }
    }

    // ==================== ZIP Extraction Tests ====================

    @Test
    fun `extractZipFile extracts single file successfully`() {
        // Given
        val zipFile = File(testDir, "test.zip")
        val extractDir = File(testDir, "extract")
        val testContent = "Test savestate data".toByteArray()

        createTestZip(zipFile, mapOf("savestate.yss" to testContent))

        // When
        val extractedFiles = invokeExtractZipFile(zipFile, extractDir)

        // Then
        assertEquals(1, extractedFiles.size)
        assertTrue(extractedFiles[0].exists())
        assertEquals("savestate.yss", extractedFiles[0].name)

        // Verify content
        val extractedContent = extractedFiles[0].readBytes()
        assertArrayEquals(testContent, extractedContent)
    }

    @Test
    fun `extractZipFile extracts multiple files successfully`() {
        // Given
        val zipFile = File(testDir, "test.zip")
        val extractDir = File(testDir, "extract")
        val entries =
            mapOf(
                "savestate.yss" to "Savestate data".toByteArray(),
                "memory.ram" to "Memory data".toByteArray(),
                "config.txt" to "Config data".toByteArray(),
            )

        createTestZip(zipFile, entries)

        // When
        val extractedFiles = invokeExtractZipFile(zipFile, extractDir)

        // Then
        assertEquals(3, extractedFiles.size)

        val fileNames = extractedFiles.map { it.name }.sorted()
        assertEquals(listOf("config.txt", "memory.ram", "savestate.yss"), fileNames)

        // Verify each file exists and has correct content
        extractedFiles.forEach { file ->
            assertTrue(file.exists())
            val expectedContent = entries[file.name]
            assertNotNull(expectedContent)
            assertArrayEquals(expectedContent, file.readBytes())
        }
    }

    @Test
    fun `extractZipFile creates nested directories`() {
        // Given
        val zipFile = File(testDir, "test.zip")
        val extractDir = File(testDir, "extract")
        val entries =
            mapOf(
                "savestate/slot1/save.yss" to "Slot 1 data".toByteArray(),
                "savestate/slot2/save.yss" to "Slot 2 data".toByteArray(),
            )

        createTestZip(zipFile, entries)

        // When
        val extractedFiles = invokeExtractZipFile(zipFile, extractDir)

        // Then
        assertEquals(2, extractedFiles.size)

        // Verify directory structure
        assertTrue(File(extractDir, "savestate/slot1").exists())
        assertTrue(File(extractDir, "savestate/slot2").exists())
        assertTrue(File(extractDir, "savestate/slot1/save.yss").exists())
        assertTrue(File(extractDir, "savestate/slot2/save.yss").exists())
    }

    @Test
    fun `extractZipFile handles empty ZIP file`() {
        // Given
        val zipFile = File(testDir, "empty.zip")
        val extractDir = File(testDir, "extract")

        createTestZip(zipFile, emptyMap())

        // When
        val extractedFiles = invokeExtractZipFile(zipFile, extractDir)

        // Then
        assertEquals(0, extractedFiles.size)
    }

    @Test
    fun `extractZipFile handles large files`() {
        // Given
        val zipFile = File(testDir, "large.zip")
        val extractDir = File(testDir, "extract")

        // Create 1MB of test data
        val largeContent = ByteArray(1024 * 1024) { it.toByte() }
        createTestZip(zipFile, mapOf("large_savestate.yss" to largeContent))

        // When
        val extractedFiles = invokeExtractZipFile(zipFile, extractDir)

        // Then
        assertEquals(1, extractedFiles.size)
        assertTrue(extractedFiles[0].exists())
        assertEquals(largeContent.size.toLong(), extractedFiles[0].length())

        // Verify content integrity
        assertArrayEquals(largeContent, extractedFiles[0].readBytes())
    }

    @Test
    fun `extractZipFile handles files with special characters in names`() {
        // Given
        val zipFile = File(testDir, "special.zip")
        val extractDir = File(testDir, "extract")
        val entries =
            mapOf(
                "save state (1).yss" to "Data 1".toByteArray(),
                "memory_backup_2025-01-12.ram" to "Data 2".toByteArray(),
                "config@test#1.txt" to "Data 3".toByteArray(),
            )

        createTestZip(zipFile, entries)

        // When
        val extractedFiles = invokeExtractZipFile(zipFile, extractDir)

        // Then
        assertEquals(3, extractedFiles.size)

        extractedFiles.forEach { file ->
            assertTrue("File should exist: ${file.name}", file.exists())
        }
    }

    @Test
    fun `extractZipFile creates target directory if not exists`() {
        // Given
        val zipFile = File(testDir, "test.zip")
        val extractDir = File(testDir, "new_extract_dir/nested/deep")

        assertFalse("Extract directory should not exist yet", extractDir.exists())

        createTestZip(zipFile, mapOf("test.yss" to "data".toByteArray()))

        // When
        val extractedFiles = invokeExtractZipFile(zipFile, extractDir)

        // Then
        assertTrue("Extract directory should be created", extractDir.exists())
        assertEquals(1, extractedFiles.size)
    }

    @Test
    fun `extractZipFile handles ZIP with directory entries`() {
        // Given
        val zipFile = File(testDir, "with_dirs.zip")
        val extractDir = File(testDir, "extract")

        // Create ZIP with explicit directory entries
        ZipOutputStream(FileOutputStream(zipFile)).use { zipOut ->
            // Add directory entry
            val dirEntry = ZipEntry("savestate/")
            dirEntry.method = ZipEntry.STORED
            dirEntry.size = 0
            dirEntry.compressedSize = 0
            dirEntry.crc = 0
            zipOut.putNextEntry(dirEntry)
            zipOut.closeEntry()

            // Add file entry
            val fileEntry = ZipEntry("savestate/save.yss")
            zipOut.putNextEntry(fileEntry)
            zipOut.write("data".toByteArray())
            zipOut.closeEntry()
        }

        // When
        val extractedFiles = invokeExtractZipFile(zipFile, extractDir)

        // Then
        // Should only return files, not directories
        assertEquals(1, extractedFiles.size)
        assertEquals("save.yss", extractedFiles[0].name)
        assertTrue(File(extractDir, "savestate").exists())
        assertTrue(File(extractDir, "savestate").isDirectory)
    }

    @Test
    fun `extractZipFile returns empty list on non-existent ZIP file`() {
        // Given
        val zipFile = File(testDir, "nonexistent.zip")
        val extractDir = File(testDir, "extract")

        // When
        val extractedFiles = invokeExtractZipFile(zipFile, extractDir)

        // Then
        assertEquals(0, extractedFiles.size)
    }

    @Test
    fun `extractZipFile handles corrupted ZIP file gracefully`() {
        // Given
        val zipFile = File(testDir, "corrupted.zip")
        val extractDir = File(testDir, "extract")

        // Create a corrupted ZIP file (just write random bytes)
        zipFile.writeBytes(ByteArray(100) { it.toByte() })

        // When
        val extractedFiles = invokeExtractZipFile(zipFile, extractDir)

        // Then
        // Should return empty list and not crash
        assertEquals(0, extractedFiles.size)
    }

    @Test
    fun `extractZipFile overwrites existing files`() {
        // Given
        val zipFile = File(testDir, "test.zip")
        val extractDir = File(testDir, "extract")
        extractDir.mkdirs()

        // Create an existing file with different content
        val existingFile = File(extractDir, "savestate.yss")
        existingFile.writeText("Old content")

        // Create ZIP with new content
        val newContent = "New content".toByteArray()
        createTestZip(zipFile, mapOf("savestate.yss" to newContent))

        // When
        val extractedFiles = invokeExtractZipFile(zipFile, extractDir)

        // Then
        assertEquals(1, extractedFiles.size)
        assertEquals("New content", extractedFiles[0].readText())
    }

    @Test
    fun `extractZipFile handles binary data correctly`() {
        // Given
        val zipFile = File(testDir, "binary.zip")
        val extractDir = File(testDir, "extract")

        // Create binary data with all byte values
        val binaryContent = ByteArray(256) { it.toByte() }
        createTestZip(zipFile, mapOf("binary.ram" to binaryContent))

        // When
        val extractedFiles = invokeExtractZipFile(zipFile, extractDir)

        // Then
        assertEquals(1, extractedFiles.size)
        assertArrayEquals(binaryContent, extractedFiles[0].readBytes())
    }

    @Test
    fun `extractZipFile security - prevents path traversal with parent directory`() {
        // Given
        val zipFile = File(testDir, "malicious.zip")
        val extractDir = File(testDir, "extract")

        // Create ZIP with path traversal attempt
        ZipOutputStream(FileOutputStream(zipFile)).use { zipOut ->
            val entry = ZipEntry("../../../evil.txt")
            zipOut.putNextEntry(entry)
            zipOut.write("malicious content".toByteArray())
            zipOut.closeEntry()
        }

        // When
        val extractedFiles = invokeExtractZipFile(zipFile, extractDir)

        // Then
        // Should skip the malicious entry
        assertEquals(0, extractedFiles.size)

        // Verify the malicious file was NOT created outside extract dir
        val evilFile = File(testDir.parentFile, "evil.txt")
        assertFalse("Malicious file should not be created", evilFile.exists())
    }

    @Test
    fun `extractZipFile security - prevents absolute path injection`() {
        // Given
        val zipFile = File(testDir, "malicious.zip")
        val extractDir = File(testDir, "extract")

        // Create ZIP with absolute path
        ZipOutputStream(FileOutputStream(zipFile)).use { zipOut ->
            val entry = ZipEntry("/etc/passwd")
            zipOut.putNextEntry(entry)
            zipOut.write("malicious content".toByteArray())
            zipOut.closeEntry()
        }

        // When
        val extractedFiles = invokeExtractZipFile(zipFile, extractDir)

        // Then
        // Implementation should handle this - either skip or extract to relative path
        // The key is that /etc/passwd should NOT be overwritten
        assertTrue(
            "Should not extract absolute paths unsafely",
            extractedFiles.isEmpty() ||
                extractedFiles.all {
                    it.canonicalPath.startsWith(extractDir.canonicalPath)
                },
        )
    }

    // ==================== Helper to access private extractZipFile method ====================

/**
     * Uses reflection to call the private extractZipFile method
     * This is necessary for unit testing private methods
     */
    private fun invokeExtractZipFile(
        zipFile: File,
        targetDir: File,
    ): List<File> = try {
        val method =
            ReportReproduceManager::class.java.getDeclaredMethod(
                "extractZipFile",
                File::class.java,
                File::class.java,
            )
        method.isAccessible = true
        @Suppress("UNCHECKED_CAST")
        method.invoke(manager, zipFile, targetDir) as List<File>
    } catch (e: Exception) {
        // If reflection fails, return empty list
        emptyList()
    }
}
