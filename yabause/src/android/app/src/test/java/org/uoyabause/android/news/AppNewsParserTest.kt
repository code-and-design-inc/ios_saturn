package org.uoyabause.android.news

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class AppNewsParserTest {
    private val validJson = """
        {
          "version": 1,
          "latestVersion": "1.20.26",
          "latestVersionCode": 1202600,
          "items": [
            {
              "id": "release-1.20.26",
              "date": "2026-07-18",
              "imageUrl": "https://cdn/x.jpg",
              "linkUrl": "https://site/blog/x",
              "title": { "en": "Title EN", "ja": "Title JA" },
              "body": { "en": "Body EN", "ja": "Body JA" }
            }
          ]
        }
    """.trimIndent()

    @Test
    fun parsesValidFeed() {
        val feed = AppNewsParser.parse(validJson)!!
        assertEquals(1, feed.version)
        assertEquals("1.20.26", feed.latestVersion)
        assertEquals(1202600, feed.latestVersionCode)
        assertEquals(1, feed.items.size)
        val item = feed.items[0]
        assertEquals("release-1.20.26", item.id)
        assertEquals("Title JA", item.title["ja"])
        assertEquals("Body EN", item.body["en"])
        assertEquals("https://cdn/x.jpg", item.imageUrl)
    }

    @Test
    fun returnsNullOnInvalidJson() {
        assertNull(AppNewsParser.parse("{ not json"))
    }

    @Test
    fun returnsNullOnUnsupportedFutureVersion() {
        assertNull(AppNewsParser.parse("""{ "version": 99, "items": [] }"""))
    }

    @Test
    fun optionalFieldsMayBeMissing() {
        val json = """
            { "version": 1, "items": [
              { "id": "a", "date": "2026-01-01",
                "title": { "en": "t" }, "body": { "en": "b" } } ] }
        """.trimIndent()
        val feed = AppNewsParser.parse(json)!!
        assertNull(feed.items[0].imageUrl)
        assertNull(feed.items[0].linkUrl)
        assertTrue(feed.latestVersionCode == null)
    }

    @Test
    fun dropsItemsMissingRequiredFields() {
        val json = """
            { "version": 1, "items": [
              { "date": "2026-01-01", "title": { "en": "t" }, "body": { "en": "b" } },
              { "id": "ok", "date": "2026-01-02", "title": { "en": "t" }, "body": { "en": "b" } } ] }
        """.trimIndent()
        val feed = AppNewsParser.parse(json)!!
        assertEquals(1, feed.items.size)
        assertEquals("ok", feed.items[0].id)
    }
}
