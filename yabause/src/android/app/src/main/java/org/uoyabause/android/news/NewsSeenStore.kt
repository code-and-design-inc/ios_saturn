package org.uoyabause.android.news

import android.content.Context

// Local, device-only record of which news ids have already been shown.
class NewsSeenStore(context: Context) {
    private val prefs = context.applicationContext
        .getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    fun seenIds(): Set<String> = prefs.getStringSet(KEY_SEEN, emptySet()) ?: emptySet()

    fun markSeen(id: String) {
        // Copy: the returned set from getStringSet must not be mutated in place.
        val updated = HashSet(seenIds())
        updated.add(id)
        prefs.edit().putStringSet(KEY_SEEN, updated).apply()
    }

    companion object {
        private const val PREFS_NAME = "news_prefs"
        private const val KEY_SEEN = "seen_ids"
    }
}
