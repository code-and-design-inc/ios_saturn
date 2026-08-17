/*
 * Copyright 2026 devMiyax
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 */
package org.uoyabause.android.backup

/**
 * Builds and parses share links for public shared backups.
 * URL form: https://www.yabasanshiro.com/backup/{id}
 * Custom scheme (desktop/fallback): yabasanshiro://backup/{id}
 * Keep these strings in sync with the web and Windows plans.
 */
object BackupShareLink {
    private const val HOST = "www.yabasanshiro.com"
    private const val PATH = "backup"

    // Fixed hashtags attached to every shared-save post so they can be searched together.
    private const val FIXED_TAGS = "#yabasave #SegaSaturn"

    // Generated ids are UUIDs; restrict to a safe id charset/length before any
    // network use as defense-in-depth against crafted deep-link URIs.
    private val ID_PATTERN = Regex("^[A-Za-z0-9_-]{1,128}$")

    fun buildUrl(id: String): String = "https://$HOST/$PATH/$id"

    /**
     * Extracts the backup id from an https link or the yabasanshiro:// custom scheme.
     * Returns null if the URI is not a backup share link or the id is malformed.
     */
    fun parseId(uri: String): String? {
        val noQuery = uri.substringBefore('?').substringBefore('#')
        val httpsPrefix = "https://$HOST/$PATH/"
        val schemePrefix = "yabasanshiro://$PATH/"
        val rest = when {
            noQuery.startsWith(httpsPrefix) -> noQuery.removePrefix(httpsPrefix)
            noQuery.startsWith(schemePrefix) -> noQuery.removePrefix(schemePrefix)
            else -> return null
        }
        val id = rest.trim('/').substringBefore('/')
        return if (ID_PATTERN.matches(id)) id else null
    }

    /**
     * Builds the hashtag line for a shared-save post: the fixed "#yabasave
     * #SegaSaturn" tags plus a "#GameName" tag. Spaces and punctuation are
     * stripped from the game title because hashtags cannot contain them; letters
     * and digits (including Japanese) are kept. A blank title yields only the
     * fixed tags.
     */
    fun hashtags(gameTitle: String): String {
        val game = gameTitle.filter { it.isLetterOrDigit() }
        return if (game.isEmpty()) FIXED_TAGS else "$FIXED_TAGS #$game"
    }
}
