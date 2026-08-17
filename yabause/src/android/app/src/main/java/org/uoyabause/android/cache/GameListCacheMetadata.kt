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

/**
 * Metadata for the game list cache.
 * Used for version management and bulk cache invalidation.
 *
 * @property version Cache format version (for compatibility checking)
 * @property createdAt Timestamp when cache was first created
 * @property lastValidatedAt Timestamp when cache was last validated
 * @property directories List of directory snapshots being monitored
 */
data class GameListCacheMetadata(
    val version: Int,
    val createdAt: Long,
    val lastValidatedAt: Long,
    val directories: List<DirectorySnapshot>,
) {
    companion object {
        const val CURRENT_VERSION = 1
    }
}
