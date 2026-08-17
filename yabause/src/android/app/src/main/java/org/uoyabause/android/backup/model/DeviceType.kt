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

/**
 * Enum representing backup storage device types.
 * Used for categorizing backup locations in the standalone backup manager.
 */
enum class DeviceType(
    val id: Int,
    val displayName: String,
) {
    /**
     * Internal device memory (device ID: 0)
     * Corresponds to the emulated internal Saturn backup RAM
     */
    INTERNAL(0, "Internal"),

    /**
     * External device memory (device ID: 1)
     * Corresponds to the emulated external backup cartridge
     */
    EXTERNAL(1, "External"),

    /**
     * Cloud storage via Firebase (device ID: 48)
     * User's personal cloud backup storage
     */
    CLOUD(48, "Cloud"),

    /**
     * Shared backups from other users
     * Public backups shared by the community
     */
    SHARED(-1, "Shared"),
    ;

    companion object {
        /**
         * Get DeviceType from device ID
         * @param id The device ID
         * @return The corresponding DeviceType, or INTERNAL if not found
         */
        fun fromId(id: Int): DeviceType = entries.find { it.id == id } ?: INTERNAL

        /**
         * Get DeviceType from tab position
         * @param position The tab position (0-2)
         * @return The corresponding DeviceType
         */
        fun fromTabPosition(position: Int): DeviceType = when (position) {
            0 -> INTERNAL
            1 -> CLOUD
            2 -> SHARED
            else -> INTERNAL
        }
    }
}
