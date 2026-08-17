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
package org.uoyabause.android

import android.content.Context
import android.os.Build
import android.os.Environment
import android.util.Log
import org.apache.commons.io.FileUtils
import java.io.File

object StorageMigrationHelper {
    private const val TAG = "StorageMigrationHelper"

    // --- Internal storage (primary volume) ---

    /**
     * Returns true if there are game files in the old Android/data location
     * that have not yet been moved to Android/media.
     */
    fun needsInternalMigration(context: Context): Boolean {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return false
        val newGames = internalMediaGamesDir(context)
        // Already migrated if new dir has content
        if (newGames.exists() && newGames.listFiles()?.isNotEmpty() == true) return false
        return oldInternalGamesCandidates(context).any { it.exists() && it.listFiles()?.isNotEmpty() == true }
    }

    /**
     * Moves game files one-by-one from the old Android/data location to Android/media.
     * [onProgress] is called with the current file name on the calling thread.
     */
    fun performInternalMigration(
        context: Context,
        onProgress: (String) -> Unit,
    ) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return
        val newGamesDir = internalMediaGamesDir(context)
        newGamesDir.mkdirs()

        val source =
            oldInternalGamesCandidates(context)
                .firstOrNull { it.exists() && it.listFiles()?.isNotEmpty() == true } ?: return

        source.listFiles()?.forEach { file ->
            onProgress(file.name)
            try {
                FileUtils.moveToDirectory(file, newGamesDir, true)
            } catch (e: Exception) {
                Log.e(TAG, "Failed to move ${file.name}: ${e.message}")
            }
        }
        Log.i(TAG, "Internal migration complete -> ${newGamesDir.path}")
    }

    // --- External storage (SD card) ---

    /**
     * Returns true if there are game files in the old Android/data location on the SD card
     * that have not yet been moved to Android/media.
     */
    fun needsExternalMigration(
        context: Context,
        sdCardFilesDir: File,
    ): Boolean {
        val newGames = externalMediaGamesDir(context, sdCardFilesDir)
        if (newGames.exists() && newGames.listFiles()?.isNotEmpty() == true) return false
        // Check subdirectory candidates first
        if (oldExternalGamesCandidates(sdCardFilesDir).any { it.exists() && it.listFiles()?.isNotEmpty() == true }) {
            return true
        }
        // Also check if game files exist directly in the files/ dir (original SD card behaviour)
        return sdCardFilesDir.exists() && sdCardFilesDir.listFiles { _, name -> isGameFile(name) }?.isNotEmpty() == true
    }

    /**
     * Moves game files one-by-one from the old SD-card Android/data location to Android/media.
     */
    fun performExternalMigration(
        context: Context,
        sdCardFilesDir: File,
        onProgress: (String) -> Unit,
    ) {
        val newGamesDir = externalMediaGamesDir(context, sdCardFilesDir)
        newGamesDir.mkdirs()

        // Try subdirectory candidates first
        val subDirSource = oldExternalGamesCandidates(sdCardFilesDir)
            .firstOrNull { it.exists() && it.listFiles()?.isNotEmpty() == true }

        if (subDirSource != null) {
            subDirSource.listFiles()?.forEach { file ->
                onProgress(file.name)
                try {
                    FileUtils.moveToDirectory(file, newGamesDir, true)
                } catch (e: Exception) {
                    Log.e(TAG, "Failed to move SD ${file.name}: ${e.message}")
                }
            }
        } else {
            // Game files stored directly in files/ dir
            sdCardFilesDir.listFiles { _, name -> isGameFile(name) }?.forEach { file ->
                onProgress(file.name)
                try {
                    FileUtils.moveToDirectory(file, newGamesDir, true)
                } catch (e: Exception) {
                    Log.e(TAG, "Failed to move SD ${file.name}: ${e.message}")
                }
            }
        }
        Log.i(TAG, "External migration complete -> ${newGamesDir.path}")
    }

    // --- Helpers ---

    private fun internalMediaGamesDir(context: Context): File =
        File(
            Environment.getExternalStorageDirectory(),
            "Android/media/${context.packageName}/games",
        )

    private fun oldInternalGamesCandidates(context: Context): List<File> {
        val filesDir = context.getExternalFilesDir(null) ?: return emptyList()
        return listOf(
            File(filesDir, "yabause/games"), // original structure
            File(filesDir, "games"), // Android 14+ special location
        )
    }

    private fun externalMediaGamesDir(
        context: Context,
        sdCardFilesDir: File,
    ): File {
        val sdRoot = sdCardFilesDir.absolutePath.substringBefore("/Android/data")
        return File("$sdRoot/Android/media/${context.packageName}/games")
    }

    private fun oldExternalGamesCandidates(sdCardFilesDir: File): List<File> =
        listOf(
            File(sdCardFilesDir, "yabause/games"),
            File(sdCardFilesDir, "games"),
        )

    private fun isGameFile(name: String): Boolean {
        val lower = name.lowercase()
        return lower.endsWith(".iso") ||
            lower.endsWith(".bin") ||
            lower.endsWith(".img") ||
            lower.endsWith(".cue") ||
            lower.endsWith(".ccd") ||
            lower.endsWith(".mds") ||
            lower.endsWith(".chd")
    }
}
