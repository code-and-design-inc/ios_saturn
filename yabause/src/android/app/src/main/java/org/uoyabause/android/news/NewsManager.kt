package org.uoyabause.android.news

import android.util.Log
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import org.devmiyax.yabasanshiro.BuildConfig
import java.util.Locale
import java.util.concurrent.TimeUnit

// Fetches app_news.json on launch and shows the newest unseen item, if any.
object NewsManager {
    const val NEWS_URL = "https://www.yabasanshiro.com/app_news.json"
    private const val TAG = "NewsManager"

    private val client =
        OkHttpClient
            .Builder()
            .connectTimeout(5, TimeUnit.SECONDS)
            .readTimeout(5, TimeUnit.SECONDS)
            .build()

    fun checkAndShow(activity: FragmentActivity) {
        activity.lifecycleScope.launch {
            try {
                val json = withContext(Dispatchers.IO) { fetch() } ?: return@launch
                val feed = AppNewsParser.parse(json) ?: return@launch
                val store = NewsSeenStore(activity)
                val item = NewsLogic.selectLatestUnseen(feed.items, store.seenIds()) ?: return@launch

                val locale = Locale.getDefault()
                val langKey = NewsLogic.toNewsLangKey(locale.language, locale.country)
                // Skip if we cannot resolve any text (no matching locale and no English).
                if (NewsLogic.resolveText(item.title, langKey) == null) return@launch

                if (!activity.lifecycle.currentState.isAtLeast(Lifecycle.State.RESUMED) ||
                    activity.isFinishing
                ) {
                    return@launch
                }

                val showUpdate =
                    NewsLogic.shouldShowUpdate(
                        BuildConfig.VERSION_CODE,
                        feed.latestVersionCode,
                    )
                val playUrl = NewsLogic.playStoreUrl(BuildConfig.APPLICATION_ID)

                val fm = activity.supportFragmentManager
                // Avoid a duplicate if one is already shown/restored (e.g. across recreation).
                if (fm.findFragmentByTag(NewsDialogFragment.TAG) != null) return@launch
                val title = NewsLogic.resolveText(item.title, langKey) ?: return@launch
                val body = NewsLogic.resolveText(item.body, langKey).orEmpty()
                NewsDialogFragment
                    .newInstance(title, body, item.imageUrl, item.linkUrl, showUpdate, playUrl)
                    .show(fm, NewsDialogFragment.TAG)
                // "Shown == seen": mark now so it does not reappear next launch.
                store.markSeen(item.id)
            } catch (e: Exception) {
                Log.i(TAG, "news check skipped: ${e.message}")
            }
        }
    }

    private fun fetch(): String? {
        val request = Request.Builder().url(NEWS_URL).build()
        client.newCall(request).execute().use { response ->
            if (!response.isSuccessful) return null
            return response.body?.string()
        }
    }
}
