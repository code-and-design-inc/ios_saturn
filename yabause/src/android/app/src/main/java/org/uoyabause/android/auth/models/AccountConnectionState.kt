package org.uoyabause.android.auth.models

/**
 * Represents the connection state of an individual authentication service
 */
data class AccountConnectionState(
    val isConnected: Boolean,
    val username: String? = null,
    val displayName: String? = null,
    val avatarUrl: String? = null,
    val lastSyncTime: Long? = null,
    val additionalInfo: Map<String, Any>? = null,
    val error: String? = null,
    val createAccountUrl: String? = null, // URL for account creation when not connected
)
