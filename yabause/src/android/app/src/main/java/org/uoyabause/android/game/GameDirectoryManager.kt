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
package org.uoyabause.android.game

import android.content.SharedPreferences
import android.util.Log
import org.json.JSONArray
import org.json.JSONObject

class GameDirectoryManager(
    private val prefs: SharedPreferences,
    private val defaultGamePath: String = "",
) {
    companion object {
        const val KEY_GAME_DIRECTORY = "pref_game_directory"
        const val KEY_GAME_DIRECTORY_META = "pref_game_directory_meta"
        private const val TAG = "GameDirectoryManager"
    }

    fun loadDirectoryList(): List<String> {
        val data = prefs.getString(KEY_GAME_DIRECTORY, "err") ?: "err"
        if (data == "err") {
            return if (defaultGamePath.isNotEmpty()) {
                listOf(defaultGamePath)
            } else {
                emptyList()
            }
        }
        if (data.isBlank()) return emptyList()
        return data.split(";").filter { it.isNotEmpty() }
    }

    fun saveDirectoryList(directories: List<String>) {
        val value = directories.joinToString(";") { it } + if (directories.isNotEmpty()) ";" else ""
        prefs.edit().putString(KEY_GAME_DIRECTORY, value).apply()
    }

    fun addDirectory(path: String) {
        val current = loadDirectoryList().toMutableList()
        current.add(path)
        saveDirectoryList(current)
    }

    fun removeDirectory(index: Int) {
        val current = loadDirectoryList().toMutableList()
        if (index in current.indices) {
            current.removeAt(index)
            saveDirectoryList(current)
        }
    }

    fun loadMetaList(): List<ScanFolderItem> {
        val json = prefs.getString(KEY_GAME_DIRECTORY_META, null) ?: return emptyList()
        return try {
            val array = JSONArray(json)
            (0 until array.length()).map { i ->
                val obj = array.getJSONObject(i)
                ScanFolderItem(
                    path = obj.getString("path"),
                    fileCount = obj.optInt("fileCount", 0),
                    lastScanTimestamp = obj.optLong("lastScan", 0L),
                )
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to parse directory meta: ${e.message}")
            emptyList()
        }
    }

    fun saveMetaList(items: List<ScanFolderItem>) {
        val array = JSONArray()
        items.forEach { item ->
            val obj = JSONObject().apply {
                put("path", item.path)
                put("fileCount", item.fileCount)
                put("lastScan", item.lastScanTimestamp)
            }
            array.put(obj)
        }
        prefs.edit().putString(KEY_GAME_DIRECTORY_META, array.toString()).apply()
    }
}
