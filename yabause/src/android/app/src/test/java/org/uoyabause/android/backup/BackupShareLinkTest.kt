package org.uoyabause.android.backup

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class BackupShareLinkTest {
    @Test
    fun buildUrl_producesHttpsLink() {
        assertEquals(
            "https://www.yabasanshiro.com/backup/abc123",
            BackupShareLink.buildUrl("abc123"),
        )
    }

    @Test
    fun parseId_fromHttpsLink() {
        assertEquals(
            "abc123",
            BackupShareLink.parseId("https://www.yabasanshiro.com/backup/abc123"),
        )
    }

    @Test
    fun parseId_fromCustomScheme() {
        assertEquals(
            "abc123",
            BackupShareLink.parseId("yabasanshiro://backup/abc123"),
        )
    }

    @Test
    fun parseId_ignoresTrailingSlashAndQuery() {
        assertEquals(
            "abc123",
            BackupShareLink.parseId("https://www.yabasanshiro.com/backup/abc123/?utm=x"),
        )
    }

    @Test
    fun parseId_returnsNullForUnrelatedUrl() {
        assertNull(BackupShareLink.parseId("https://www.yabasanshiro.com/games/123"))
        assertNull(BackupShareLink.parseId("saturngame://yabasanshiro/play/foo"))
    }

    @Test
    fun parseId_ignoresFragment() {
        assertEquals(
            "abc123",
            BackupShareLink.parseId("https://www.yabasanshiro.com/backup/abc123#section"),
        )
    }

    @Test
    fun hashtags_includesFixedTagsAndSanitizedGameName() {
        // Spaces and punctuation are stripped so the game name is a valid hashtag.
        assertEquals(
            "#yabasave #SegaSaturn #ThunderForceV",
            BackupShareLink.hashtags("Thunder Force V"),
        )
        assertEquals(
            "#yabasave #SegaSaturn #NIGHTSintoDreams",
            BackupShareLink.hashtags("NIGHTS into Dreams..."),
        )
    }

    @Test
    fun hashtags_keepsJapaneseGameName() {
        assertEquals(
            "#yabasave #SegaSaturn #バーチャファイター",
            BackupShareLink.hashtags("バーチャファイター"),
        )
    }

    @Test
    fun hashtags_blankGameNameYieldsOnlyFixedTags() {
        assertEquals("#yabasave #SegaSaturn", BackupShareLink.hashtags(""))
        assertEquals("#yabasave #SegaSaturn", BackupShareLink.hashtags("   "))
    }

    @Test
    fun parseId_returnsNullForMalformedId() {
        // Illegal characters in the id segment are rejected.
        assertNull(BackupShareLink.parseId("https://www.yabasanshiro.com/backup/bad%20id"))
        assertNull(BackupShareLink.parseId("yabasanshiro://backup/a b c"))
        // Over-long id is rejected.
        assertNull(BackupShareLink.parseId("yabasanshiro://backup/" + "a".repeat(129)))
        // Empty id is rejected.
        assertNull(BackupShareLink.parseId("https://www.yabasanshiro.com/backup/"))
    }
}
