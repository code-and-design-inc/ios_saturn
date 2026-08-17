package org.uoyabause.android.auth

import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.auth.FirebaseUser

/**
 * Single source of truth for "is the user really signed in".
 *
 * Anonymous Firebase sessions are used ONLY for guest report submission. Every
 * other feature must treat an anonymous session as "not signed in". realUser()
 * returns null for anonymous users, so existing gate checks of the form
 * `currentUser ?: return`, `currentUser == null`, `currentUser?.uid` keep working
 * unchanged once their user accessor is swapped to AuthState.realUser().
 */
object AuthState {
    /** The signed-in, non-anonymous user; null when signed out OR only an anonymous session exists. */
    fun realUser(): FirebaseUser? =
        FirebaseAuth.getInstance().currentUser?.takeIf { !it.isAnonymous }

    /** True only when a real (non-anonymous) user is signed in. */
    fun isSignedIn(): Boolean = realUser() != null

    /** The user permitted to submit a report (real OR anonymous); null when no session exists at all. */
    fun reportUser(): FirebaseUser? = FirebaseAuth.getInstance().currentUser
}
