package org.uoyabause.android.auth.models

/**
 * UI state for account management screen
 */
sealed class AccountUiState {
    object Loading : AccountUiState()

    data class Success(
        val userProfile: UserProfile?,
        val connectedAccounts: ConnectedAccountsState,
    ) : AccountUiState()

    data class Error(
        val message: String,
        val isRetryable: Boolean = true,
    ) : AccountUiState()

    object NotAuthenticated : AccountUiState()
}
