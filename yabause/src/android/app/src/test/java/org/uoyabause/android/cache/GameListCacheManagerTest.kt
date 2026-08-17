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
import android.content.SharedPreferences
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.`when`
import org.mockito.junit.MockitoJUnitRunner

@RunWith(MockitoJUnitRunner.Silent::class)
class GameListCacheManagerTest {
    @Mock
    private lateinit var mockContext: Context

    @Mock
    private lateinit var mockSharedPreferences: SharedPreferences

    @Mock
    private lateinit var mockEditor: SharedPreferences.Editor

    private lateinit var cacheManager: GameListCacheManager

    @Before
    fun setup() {
        `when`(mockContext.getSharedPreferences("game_list_cache", Context.MODE_PRIVATE))
            .thenReturn(mockSharedPreferences)
        `when`(mockSharedPreferences.edit()).thenReturn(mockEditor)
        `when`(
            mockEditor.putString(org.mockito.ArgumentMatchers.anyString(), org.mockito.ArgumentMatchers.anyString()),
        ).thenReturn(mockEditor)
        `when`(mockEditor.remove(org.mockito.ArgumentMatchers.anyString())).thenReturn(mockEditor)

        cacheManager = GameListCacheManager(mockContext)
    }

    // T009: Unit test for isCacheValid()
    @Test
    fun `isCacheValid returns false when no cache exists`() {
        `when`(mockSharedPreferences.getString("game_list_cache_metadata", null))
            .thenReturn(null)

        val result = cacheManager.isCacheValid(emptyList())

        assertFalse(result)
    }

    @Test
    fun `isCacheValid returns false when cache version mismatch`() {
        val oldVersionJson =
            """
            {
                "version": 0,
                "createdAt": 1000,
                "lastValidatedAt": 1000,
                "directories": []
            }
            """.trimIndent()

        `when`(mockSharedPreferences.getString("game_list_cache_metadata", null))
            .thenReturn(oldVersionJson)

        val result = cacheManager.isCacheValid(emptyList())

        assertFalse(result)
    }

    @Test
    fun `isCacheValid returns true when cache valid and no changes`() {
        val validJson =
            """
            {
                "version": 1,
                "createdAt": 1000,
                "lastValidatedAt": 1000,
                "directories": []
            }
            """.trimIndent()

        `when`(mockSharedPreferences.getString("game_list_cache_metadata", null))
            .thenReturn(validJson)

        val result = cacheManager.isCacheValid(emptyList())

        assertTrue(result)
    }

    // T020: Unit test for cache creation on first run
    @Test
    fun `loadCache returns null when no cache exists`() {
        `when`(mockSharedPreferences.getString("game_list_cache_metadata", null))
            .thenReturn(null)

        val result = cacheManager.loadCache()

        assertNull(result)
    }

    @Test
    fun `getCacheState returns NO_CACHE when no cache exists`() {
        `when`(mockSharedPreferences.getString("game_list_cache_metadata", null))
            .thenReturn(null)

        val state = cacheManager.getCacheState(emptyList())

        assertEquals(CacheState.NO_CACHE, state)
    }

    // T021: Unit test for cache persistence (save/load cycle)
    @Test
    fun `loadCache returns valid metadata when cache exists`() {
        val validJson =
            """
            {
                "version": 1,
                "createdAt": 1000,
                "lastValidatedAt": 2000,
                "directories": [
                    {
                        "directoryPath": "/games",
                        "fileCount": 5,
                        "fileListHash": "abc123",
                        "capturedAt": 1500
                    }
                ]
            }
            """.trimIndent()

        `when`(mockSharedPreferences.getString("game_list_cache_metadata", null))
            .thenReturn(validJson)

        val result = cacheManager.loadCache()

        assertNotNull(result)
        assertEquals(1, result?.version)
        assertEquals(1000L, result?.createdAt)
        assertEquals(2000L, result?.lastValidatedAt)
        assertEquals(1, result?.directories?.size)
        assertEquals("/games", result?.directories?.get(0)?.directoryPath)
    }

    @Test
    fun `loadCache returns null on invalid JSON`() {
        `when`(mockSharedPreferences.getString("game_list_cache_metadata", null))
            .thenReturn("invalid json {{{")

        val result = cacheManager.loadCache()

        assertNull(result)
    }

    // T025: Handle cache version mismatch
    @Test
    fun `getCacheState returns NO_CACHE when version mismatch`() {
        val oldVersionJson =
            """
            {
                "version": 0,
                "createdAt": 1000,
                "lastValidatedAt": 1000,
                "directories": []
            }
            """.trimIndent()

        `when`(mockSharedPreferences.getString("game_list_cache_metadata", null))
            .thenReturn(oldVersionJson)

        val state = cacheManager.getCacheState(emptyList())

        assertEquals(CacheState.NO_CACHE, state)
    }
}
