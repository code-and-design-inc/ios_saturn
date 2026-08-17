package org.uoyabause.android.auth.models

/**
 * User profile data class representing the authenticated user's information
 */
data class UserProfile(
    val uid: String,
    val displayName: String,
    val email: String?,
    val photoUrl: String?,
    val accountProvider: String,
    val lastLoginTime: Long,
    val isEmailVerified: Boolean,
    val phoneNumber: String? = null,
)
