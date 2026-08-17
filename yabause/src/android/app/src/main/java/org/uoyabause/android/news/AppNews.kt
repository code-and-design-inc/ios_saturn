package org.uoyabause.android.news

// Data classes mapping the app_news.json feed. See design doc section 4.
data class AppNewsFeed(
    val version: Int,
    val latestVersion: String?,
    val latestVersionCode: Int?,
    val items: List<AppNewsItem>,
)

data class AppNewsItem(
    val id: String,
    val date: String,
    val imageUrl: String?,
    val linkUrl: String?,
    val title: Map<String, String>,
    val body: Map<String, String>,
)
