/*  Copyright 2019 devMiyax(smiyaxdev@gmail.com)

    This file is part of YabaSanshiro.

    YabaSanshiro is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    YabaSanshiro is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with YabaSanshiro; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/
package org.uoyabause.android.cache

import android.content.Context
import android.net.Uri
import android.util.Log
import androidx.documentfile.provider.DocumentFile
import java.io.File
import java.security.MessageDigest

/**
 * Handles capturing and comparing directory state snapshots.
 * Supports both traditional File API and SAF (Storage Access Framework) DocumentFile.
 */
class DirectoryStateCache(
    private val context: Context? = null,
) {
    companion object {
        private const val TAG = "DirectoryStateCache"

        // Supported game file extensions
        private val GAME_EXTENSIONS =
            setOf(
                "cue",
                "mds",
                "ccd",
                "chd",
                "iso",
                "bin",
                "img",
                "mdf",
            )
    }

    /**
     * Captures a snapshot of the directory state.
     *
     * @param path Directory path (file path or content:// URI)
     * @return DirectorySnapshot or null if directory doesn't exist or can't be read
     */
    fun captureSnapshot(path: String): DirectorySnapshot? = try {
        if (path.startsWith("content://")) {
            captureSnapshotFromUri(path)
        } else {
            captureSnapshotFromFile(path)
        }
    } catch (e: Exception) {
        Log.e(TAG, "Error capturing snapshot for $path: ${e.message}")
        null
    }

    /**
     * Checks if the directory has changed compared to the stored snapshot.
     *
     * @param path Directory path
     * @param storedSnapshot Previously captured snapshot
     * @return true if directory has changed, false otherwise
     */
    fun hasChanged(
        path: String,
        storedSnapshot: DirectorySnapshot,
    ): Boolean {
        val currentSnapshot = captureSnapshot(path) ?: return true

        // Compare file count first (quick check)
        if (currentSnapshot.fileCount != storedSnapshot.fileCount) {
            Log.d(
                TAG,
                "Directory changed: file count mismatch " +
                    "(${currentSnapshot.fileCount} vs ${storedSnapshot.fileCount})",
            )
            return true
        }

        // Compare hash (detailed check)
        if (currentSnapshot.fileListHash != storedSnapshot.fileListHash) {
            Log.d(TAG, "Directory changed: hash mismatch")
            return true
        }

        return false
    }

    /**
     * Calculates MD5 hash of the file list.
     * Files are sorted by name to ensure consistent hash regardless of enumeration order.
     *
     * @param entries List of file entries
     * @return MD5 hash as hex string
     */
    fun calculateFileListHash(entries: List<FileEntry>): String {
        val sortedEntries = entries.sortedBy { it.name }
        val hashInput =
            sortedEntries.joinToString("\n") {
                "${it.name}|${it.size}|${it.lastModified}"
            }

        val md = MessageDigest.getInstance("MD5")
        val digest = md.digest(hashInput.toByteArray())
        return digest.joinToString("") { "%02x".format(it) }
    }

    private fun captureSnapshotFromFile(path: String): DirectorySnapshot? {
        val dir = File(path)
        if (!dir.exists() || !dir.isDirectory) {
            return null
        }

        val entries = mutableListOf<FileEntry>()
        collectFileEntries(dir, entries)

        val hash = calculateFileListHash(entries)

        return DirectorySnapshot(
            directoryPath = path,
            fileCount = entries.size,
            fileListHash = hash,
            capturedAt = System.currentTimeMillis(),
        )
    }

    private fun collectFileEntries(
        dir: File,
        entries: MutableList<FileEntry>,
    ) {
        dir.listFiles()?.forEach { file ->
            if (file.isDirectory) {
                collectFileEntries(file, entries)
            } else if (isGameFile(file.name)) {
                entries.add(
                    FileEntry(
                        name = file.name,
                        size = file.length(),
                        lastModified = file.lastModified(),
                    ),
                )
            }
        }
    }

    private fun captureSnapshotFromUri(uriString: String): DirectorySnapshot? {
        val ctx = context ?: return null
        val uri = Uri.parse(uriString)
        val docFile = DocumentFile.fromTreeUri(ctx, uri) ?: return null

        if (!docFile.exists() || !docFile.isDirectory) {
            return null
        }

        val entries = mutableListOf<FileEntry>()
        collectDocumentFileEntries(docFile, entries)

        val hash = calculateFileListHash(entries)

        return DirectorySnapshot(
            directoryPath = uriString,
            fileCount = entries.size,
            fileListHash = hash,
            capturedAt = System.currentTimeMillis(),
        )
    }

    private fun collectDocumentFileEntries(
        docFile: DocumentFile,
        entries: MutableList<FileEntry>,
    ) {
        docFile.listFiles().forEach { file ->
            if (file.isDirectory) {
                collectDocumentFileEntries(file, entries)
            } else {
                val name = file.name ?: return@forEach
                if (isGameFile(name)) {
                    entries.add(
                        FileEntry(
                            name = name,
                            size = file.length(),
                            lastModified = file.lastModified(),
                        ),
                    )
                }
            }
        }
    }

    private fun isGameFile(name: String): Boolean {
        val extension = name.substringAfterLast('.', "").lowercase()
        return extension in GAME_EXTENSIONS
    }
}
