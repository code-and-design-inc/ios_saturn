package org.uoyabause.android.news

// Pure decision logic for the news popup. No Android dependencies so it is unit-testable.
object NewsLogic {
    const val RELEASE_APP_ID = "org.devmiyax.yabasanshioro2"
    private const val PRO_APP_ID = "org.devmiyax.yabasanshioro2.pro"
    private const val PLAY_URL_PREFIX = "https://play.google.com/store/apps/details?id="

    fun selectLatestUnseen(items: List<AppNewsItem>, seenIds: Set<String>): AppNewsItem? =
        items.sortedByDescending { it.date }.firstOrNull { it.id !in seenIds }

    fun toNewsLangKey(language: String, country: String): String =
        when (language) {
            "zh" -> "zh-CN"
            "pt" -> "pt-BR"
            else -> language
        }

    fun resolveText(map: Map<String, String>, langKey: String): String? =
        map[langKey] ?: map["en"]

    fun shouldShowUpdate(installedVersionCode: Int, latestVersionCode: Int?): Boolean =
        latestVersionCode != null && installedVersionCode < latestVersionCode

    fun playStoreUrl(applicationId: String): String =
        if (applicationId == RELEASE_APP_ID) {
            PLAY_URL_PREFIX + RELEASE_APP_ID
        } else {
            PLAY_URL_PREFIX + PRO_APP_ID
        }
}
