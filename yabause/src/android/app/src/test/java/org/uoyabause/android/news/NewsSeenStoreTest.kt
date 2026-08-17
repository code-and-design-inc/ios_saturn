package org.uoyabause.android.news

import androidx.test.core.app.ApplicationProvider
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class NewsSeenStoreTest {
    private val store = NewsSeenStore(ApplicationProvider.getApplicationContext())

    @Test
    fun startsEmpty() {
        assertTrue(store.seenIds().isEmpty())
    }

    @Test
    fun persistsMarkedIds() {
        store.markSeen("a")
        store.markSeen("b")
        assertEquals(setOf("a", "b"), store.seenIds())
    }

    @Test
    fun markSeenIsIdempotent() {
        store.markSeen("a")
        store.markSeen("a")
        assertEquals(setOf("a"), store.seenIds())
    }
}
