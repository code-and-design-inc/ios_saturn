/*
 * Copyright 2024 devMiyax
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.uoyabause.android.backup.model

import androidx.annotation.StringRes
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.YabauseApplication
import org.uoyabause.android.YabauseStorage
import java.io.File

/**
 * Enum representing the 6 local backup file types supported by the Saturn emulator.
 * Each entry maps to a specific backup RAM file on disk.
 *
 * @property displayNameResId String resource ID for the human-readable name shown in the UI
 * @property fileName The filename on disk
 * @property relativeDir Either "memory" or "cartridge", indicating which storage path to use
 * @property deviceType INTERNAL for memory files, EXTERNAL for cartridge files (backward compat)
 */
enum class LocalBackupFile(
    @StringRes val displayNameResId: Int,
    val fileName: String,
    val relativeDir: String,
    val deviceType: DeviceType,
    val biosSize: Int,
    val biosBlockSize: Int,
) {
    MEMORY_FILEBIOS(
        displayNameResId = R.string.backup_file_memory_filebios,
        fileName = "memory_filebios.ram",
        relativeDir = "memory",
        deviceType = DeviceType.INTERNAL,
        biosSize = 0x8000,
        biosBlockSize = 0x40,
    ),
    MEMORY_STANDARD(
        displayNameResId = R.string.backup_file_memory_standard,
        fileName = "memory.ram",
        relativeDir = "memory",
        deviceType = DeviceType.INTERNAL,
        biosSize = 8 * 1024 * 1024,
        biosBlockSize = 0x40,
    ),
    CARTRIDGE_4MBIT(
        displayNameResId = R.string.backup_file_cartridge_4mbit,
        fileName = "backup4.ram",
        relativeDir = "cartridge",
        deviceType = DeviceType.EXTERNAL,
        biosSize = 0x80000,
        biosBlockSize = 0x200,
    ),
    CARTRIDGE_8MBIT(
        displayNameResId = R.string.backup_file_cartridge_8mbit,
        fileName = "backup8.ram",
        relativeDir = "cartridge",
        deviceType = DeviceType.EXTERNAL,
        biosSize = 0x100000,
        biosBlockSize = 0x200,
    ),
    CARTRIDGE_16MBIT(
        displayNameResId = R.string.backup_file_cartridge_16mbit,
        fileName = "backup16.ram",
        relativeDir = "cartridge",
        deviceType = DeviceType.EXTERNAL,
        biosSize = 0x200000,
        biosBlockSize = 0x200,
    ),
    CARTRIDGE_32MBIT(
        displayNameResId = R.string.backup_file_cartridge_32mbit,
        fileName = "backup32.ram",
        relativeDir = "cartridge",
        deviceType = DeviceType.EXTERNAL,
        biosSize = 0x400000,
        biosBlockSize = 0x400,
    ),
    ;

    /**
     * Human-readable display name resolved from string resources.
     */
    val displayName: String
        get() = try {
            YabauseApplication.appContext.getString(displayNameResId)
        } catch (e: Exception) {
            fileName
        }

    /**
     * File name without extension, used as screenshot subdirectory key.
     * e.g., "memory_filebios", "memory", "backup4"
     */
    val fileKey: String
        get() = fileName.substringBeforeLast('.')

    /**
     * Get the full file path for this backup file.
     */
    fun getFilePath(): String {
        val path: String? = when (relativeDir) {
            "memory" -> YabauseStorage.storage.getMemoryPath(fileName)
            "cartridge" -> YabauseStorage.storage.getCartridgePath(fileName)
            else -> YabauseStorage.storage.getMemoryPath(fileName)
        }
        return path ?: ""
    }

    /**
     * Get the File object for this backup file.
     */
    fun getFile(): File {
        val path = getFilePath()
        require(path.isNotEmpty()) { "Backup file path is empty for $fileName" }
        return File(path)
    }

    /**
     * Check if this backup file exists on disk.
     */
    fun exists(): Boolean = try {
        getFile().exists()
    } catch (e: IllegalArgumentException) {
        false
    }

    companion object {
        /**
         * Get only the backup files that exist on disk.
         */
        fun getExistingFiles(): List<LocalBackupFile> = entries.filter { it.exists() }
    }
}
