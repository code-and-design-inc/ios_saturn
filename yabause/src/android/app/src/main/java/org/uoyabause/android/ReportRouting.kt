package org.uoyabause.android

/**
 * Decides which Firestore subcollection a report is written to.
 *
 * Anonymous (guest) reports go to "anon_ratings", which the calculateRatings Cloud
 * Function does NOT watch, so anonymous reports never affect the public star average.
 * Real (signed-in) reports go to "ratings" exactly as before.
 */
object ReportRouting {
    const val COLLECTION_RATINGS = "ratings"
    const val COLLECTION_ANON_RATINGS = "anon_ratings"

    fun collectionFor(isAnonymous: Boolean): String =
        if (isAnonymous) COLLECTION_ANON_RATINGS else COLLECTION_RATINGS
}
