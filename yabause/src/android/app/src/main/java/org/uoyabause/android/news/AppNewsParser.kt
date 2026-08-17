package org.uoyabause.android.news

import com.google.gson.JsonObject
import com.google.gson.JsonParser

// Parses app_news.json. Fails closed: any problem yields null (no news shown).
object AppNewsParser {
    const val SUPPORTED_SCHEMA_VERSION = 1

    fun parse(json: String): AppNewsFeed? {
        return try {
            val root = JsonParser.parseString(json).asJsonObject
            val version = root.get("version")?.asInt ?: return null
            if (version > SUPPORTED_SCHEMA_VERSION) return null

            val items = root.getAsJsonArray("items")?.mapNotNull { el ->
                parseItem(el.asJsonObject)
            } ?: emptyList()

            AppNewsFeed(
                version = version,
                latestVersion = root.get("latestVersion")?.takeIf { !it.isJsonNull }?.asString,
                latestVersionCode = root.get("latestVersionCode")?.takeIf { !it.isJsonNull }?.asInt,
                items = items,
            )
        } catch (e: Exception) {
            null
        }
    }

    private fun parseItem(obj: JsonObject): AppNewsItem? {
        val id = obj.get("id")?.takeIf { !it.isJsonNull }?.asString ?: return null
        val date = obj.get("date")?.takeIf { !it.isJsonNull }?.asString ?: return null
        val title = parseStringMap(obj, "title") ?: return null
        val body = parseStringMap(obj, "body") ?: return null
        if (title.isEmpty() || body.isEmpty()) return null
        return AppNewsItem(
            id = id,
            date = date,
            imageUrl = obj.get("imageUrl")?.takeIf { !it.isJsonNull }?.asString,
            linkUrl = obj.get("linkUrl")?.takeIf { !it.isJsonNull }?.asString,
            title = title,
            body = body,
        )
    }

    private fun parseStringMap(obj: JsonObject, key: String): Map<String, String>? {
        val map = obj.getAsJsonObject(key) ?: return null
        return map.entrySet().associate { (k, v) -> k to v.asString }
    }
}
