package org.uoyabause.android

import org.junit.Assert.assertEquals
import org.junit.Test

class ReportRoutingTest {
    @Test
    fun `real user reports go to ratings`() {
        assertEquals("ratings", ReportRouting.collectionFor(isAnonymous = false))
    }

    @Test
    fun `anonymous reports go to anon_ratings`() {
        assertEquals("anon_ratings", ReportRouting.collectionFor(isAnonymous = true))
    }
}
