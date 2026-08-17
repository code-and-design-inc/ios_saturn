package org.uoyabause.android.auth.models

/**
 * Represents the connection state of all authentication services
 */
data class ConnectedAccountsState(
    val firebase: AccountConnectionState,
    val discord: AccountConnectionState,
    val retroAchievements: AccountConnectionState,
)
