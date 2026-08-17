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
import android.util.Log
import com.google.gson.Gson
import com.google.gson.JsonSyntaxException

/**
 * Manages the game list cache including loading, saving, and validation.
 */
class GameListCacheManager(
    private val context: Context,
) {
    companion object {
        private const val TAG = "GameListCacheManager"
        private const val PREFS_NAME = "game_list_cache"
        private const val KEY_CACHE_METADATA = "game_list_cache_metadata"
    }

    private val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
    private val gson = Gson()
    private val directoryStateCache = DirectoryStateCache(context)

    /**
     * Loads the cache metadata from SharedPreferences.
     *
     * @return GameListCacheMetadata or null if no cache exists or cache is corrupted
     */
    fun loadCache(): GameListCacheMetadata? {
        return try {
            val json = prefs.getString(KEY_CACHE_METADATA, null) ?: return null
            gson.fromJson(json, GameListCacheMetadata::class.java)
        } catch (e: JsonSyntaxException) {
            Log.e(TAG, "Failed to parse cache metadata: ${e.message}")
            null
        } catch (e: Exception) {
            Log.e(TAG, "Error loading cache: ${e.message}")
            null
        }
    }

    /**
     * Saves the cache metadata to SharedPreferences.
     *
     * @param metadata Cache metadata to save
     * @return true if save was successful, false otherwise
     */
    fun saveCache(metadata: GameListCacheMetadata): Boolean = try {
        val json = gson.toJson(metadata)
        prefs.edit().putString(KEY_CACHE_METADATA, json).apply()
        Log.d(TAG, "Cache saved successfully")
        true
    } catch (e: Exception) {
        Log.e(TAG, "Failed to save cache: ${e.message}")
        false
    }

    /**
     * Checks if the cache is valid for the given directories.
     *
     * @param directoryPaths List of directory paths to check
     * @return true if cache is valid and all directories are unchanged
     */
    fun isCacheValid(directoryPaths: List<String>): Boolean {
        val cache = loadCache() ?: return false

        // Check version compatibility
        if (cache.version != GameListCacheMetadata.CURRENT_VERSION) {
            Log.d(TAG, "Cache version mismatch: ${cache.version} vs ${GameListCacheMetadata.CURRENT_VERSION}")
            return false
        }

        // If no directories to check, cache is valid
        if (directoryPaths.isEmpty() && cache.directories.isEmpty()) {
            return true
        }

        // Check if all directories are unchanged
        return validateAllDirectories(directoryPaths, cache.directories)
    }

    /**
     * Gets the current cache state for the given directories.
     *
     * @param directoryPaths List of directory paths to check
     * @return CacheState indicating the current state
     */
    fun getCacheState(directoryPaths: List<String>): CacheState {
        val cache = loadCache() ?: return CacheState.NO_CACHE

        // Check version compatibility
        if (cache.version != GameListCacheMetadata.CURRENT_VERSION) {
            Log.d(TAG, "Cache version mismatch, treating as NO_CACHE")
            return CacheState.NO_CACHE
        }

        // Check if directories have changed
        if (!validateAllDirectories(directoryPaths, cache.directories)) {
            return CacheState.STALE
        }

        return CacheState.VALID
    }

    /**
     * Validates all directories against their stored snapshots.
     *
     * @param directoryPaths Current list of directory paths
     * @param storedSnapshots Stored directory snapshots
     * @return true if all directories are unchanged
     */
    fun validateAllDirectories(
        directoryPaths: List<String>,
        storedSnapshots: List<DirectorySnapshot>,
    ): Boolean {
        // Create a map of stored snapshots by path
        val snapshotMap = storedSnapshots.associateBy { it.directoryPath }

        // Check each current directory
        for (path in directoryPaths) {
            val storedSnapshot = snapshotMap[path]
            if (storedSnapshot == null) {
                // New directory added - cache is stale
                Log.d(TAG, "New directory detected: $path")
                return false
            }

            if (directoryStateCache.hasChanged(path, storedSnapshot)) {
                Log.d(TAG, "Directory changed: $path")
                return false
            }
        }

        // Check if any directories were removed
        for (snapshot in storedSnapshots) {
            if (snapshot.directoryPath !in directoryPaths) {
                Log.d(TAG, "Directory removed: ${snapshot.directoryPath}")
                return false
            }
        }

        return true
    }

    /**
     * Creates a new cache with snapshots for the given directories.
     *
     * @param directoryPaths List of directory paths to cache
     * @return GameListCacheMetadata or null if creation failed
     */
    fun createCache(directoryPaths: List<String>): GameListCacheMetadata? {
        val snapshots = mutableListOf<DirectorySnapshot>()

        for (path in directoryPaths) {
            val snapshot = directoryStateCache.captureSnapshot(path)
            if (snapshot != null) {
                snapshots.add(snapshot)
            } else {
                Log.w(TAG, "Failed to capture snapshot for: $path")
            }
        }

        val now = System.currentTimeMillis()
        val metadata =
            GameListCacheMetadata(
                version = GameListCacheMetadata.CURRENT_VERSION,
                createdAt = now,
                lastValidatedAt = now,
                directories = snapshots,
            )

        return if (saveCache(metadata)) metadata else null
    }

    /**
     * Updates the cache with new snapshots for the given directories.
     *
     * @param directoryPaths List of directory paths to update
     * @return true if update was successful
     */
    fun updateCache(directoryPaths: List<String>): Boolean {
        val existingCache = loadCache()
        val snapshots = mutableListOf<DirectorySnapshot>()

        for (path in directoryPaths) {
            val snapshot = directoryStateCache.captureSnapshot(path)
            if (snapshot != null) {
                snapshots.add(snapshot)
            }
        }

        val now = System.currentTimeMillis()
        val metadata =
            GameListCacheMetadata(
                version = GameListCacheMetadata.CURRENT_VERSION,
                createdAt = existingCache?.createdAt ?: now,
                lastValidatedAt = now,
                directories = snapshots,
            )

        return saveCache(metadata)
    }

    /**
     * Invalidates the cache by removing all stored data.
     */
    fun invalidateCache() {
        prefs.edit().remove(KEY_CACHE_METADATA).apply()
        Log.d(TAG, "Cache invalidated")
    }
}
