package org.uoyabause.android.news

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class NewsLogicTest {
    private fun item(id: String, date: String) =
        AppNewsItem(id, date, null, null, mapOf("en" to "t"), mapOf("en" to "b"))

    @Test
    fun selectsNewestUnseen() {
        val items = listOf(item("a", "2026-01-01"), item("b", "2026-03-01"), item("c", "2026-02-01"))
        val sel = NewsLogic.selectLatestUnseen(items, setOf("b"))
        assertEquals("c", sel?.id)
    }

    @Test
    fun returnsNullWhenAllSeen() {
        val items = listOf(item("a", "2026-01-01"))
        assertNull(NewsLogic.selectLatestUnseen(items, setOf("a")))
    }

    @Test
    fun returnsNullOnEmpty() {
        assertNull(NewsLogic.selectLatestUnseen(emptyList(), emptySet()))
    }

    @Test
    fun mapsLocaleToNewsKey() {
        assertEquals("zh-CN", NewsLogic.toNewsLangKey("zh", "CN"))
        assertEquals("pt-BR", NewsLogic.toNewsLangKey("pt", "BR"))
        assertEquals("ja", NewsLogic.toNewsLangKey("ja", "JP"))
        assertEquals("de", NewsLogic.toNewsLangKey("de", "DE"))
    }

    @Test
    fun resolvesTextWithEnglishFallback() {
        val m = mapOf("en" to "hello", "ja" to "konnichiwa")
        assertEquals("konnichiwa", NewsLogic.resolveText(m, "ja"))
        assertEquals("hello", NewsLogic.resolveText(m, "fr"))
        assertNull(NewsLogic.resolveText(mapOf("de" to "x"), "fr"))
    }

    @Test
    fun updateButtonShowsWhenOlder() {
        assertTrue(NewsLogic.shouldShowUpdate(1000, 1001))
        assertFalse(NewsLogic.shouldShowUpdate(1001, 1001))
        assertFalse(NewsLogic.shouldShowUpdate(1002, 1001))
        assertFalse(NewsLogic.shouldShowUpdate(1000, null))
    }

    @Test
    fun playStoreUrlByAppId() {
        assertTrue(NewsLogic.playStoreUrl("org.devmiyax.yabasanshioro2").endsWith("id=org.devmiyax.yabasanshioro2"))
        assertTrue(NewsLogic.playStoreUrl("org.devmiyax.yabasanshioro2.debug").endsWith("id=org.devmiyax.yabasanshioro2.pro"))
        assertTrue(NewsLogic.playStoreUrl("org.devmiyax.yabasanshioro2.pro").endsWith("id=org.devmiyax.yabasanshioro2.pro"))
    }
}
