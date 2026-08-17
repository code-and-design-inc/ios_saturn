package org.uoyabause.android.auth.viewmodel

import android.app.Application
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.viewModelScope
import com.firebase.ui.auth.IdpResponse
import kotlinx.coroutines.launch
import org.uoyabause.android.auth.models.AccountUiState
import org.uoyabause.android.auth.models.ConnectedAccountsState
import org.uoyabause.android.auth.models.UserProfile
import org.uoyabause.android.auth.repository.AccountRepository
import org.uoyabause.android.auth.utils.AccountConstants

/**
 * ViewModel for Account Management Screen
 *
 * Manages UI state and coordinates authentication operations across multiple account providers.
 * Implements MVVM pattern with reactive LiveData for UI updates.
 *
 * ## Responsibilities
 * - Managing authentication state for Firebase, Discord, and RetroAchievements
 * - Coordinating account linking/unlinking operations
 * - Handling PIN code generation for cross-device sync
 * - Managing user data export and account deletion
 * - Providing reactive UI state updates via LiveData
 *
 * ## Architecture
 * - Uses Repository pattern for data operations
 * - Coroutines for asynchronous operations
 * - LiveData for reactive UI updates
 * - Sealed classes for type-safe UI states
 *
 * @property application Application context for Android resources
 * @see AccountRepository
 * @see AccountUiState
 */
class AccountManagementViewModel(
    application: Application,
) : AndroidViewModel(application) {
    private val accountRepository = AccountRepository(application.applicationContext)

    // UI State
    private val _uiState = MutableLiveData<AccountUiState>(AccountUiState.Loading)
    val uiState: LiveData<AccountUiState> = _uiState

    // User profile data
    private val _userProfile = MutableLiveData<UserProfile?>()
    val userProfile: LiveData<UserProfile?> = _userProfile

    // Connected accounts status
    private val _connectedAccounts = MutableLiveData<ConnectedAccountsState>()
    val connectedAccounts: LiveData<ConnectedAccountsState> = _connectedAccounts

    // Loading states for individual operations
    private val _isDiscordLinking = MutableLiveData<Boolean>(false)
    val isDiscordLinking: LiveData<Boolean> = _isDiscordLinking

    private val _isRetroAchievementsLogging = MutableLiveData<Boolean>(false)
    val isRetroAchievementsLogging: LiveData<Boolean> = _isRetroAchievementsLogging

    private val _isPinGenerating = MutableLiveData<Boolean>(false)
    val isPinGenerating: LiveData<Boolean> = _isPinGenerating

    // Messages for user feedback
    private val _message = MutableLiveData<String?>()
    val message: LiveData<String?> = _message

    // Generated PIN code
    private val _generatedPin = MutableLiveData<String?>()
    val generatedPin: LiveData<String?> = _generatedPin

    // Signal that re-authentication is needed for PIN generation
    private val _pinNeedsAuth = MutableLiveData<Boolean>(false)
    val pinNeedsAuth: LiveData<Boolean> = _pinNeedsAuth

    // Exported data for GDPR compliance
    private val _exportedData = MutableLiveData<String?>()
    val exportedData: LiveData<String?> = _exportedData

    init {
        // Set up callback for account data changes (e.g., auto-login)
        accountRepository.onAccountDataChanged = {
            // Refresh UI when account data changes
            android.util.Log.d("AccountManagementViewModel", "Account data changed, refreshing UI")
            loadAccountData()
        }

        loadAccountData()
    }

    /**
     * Load all account data
     */
    fun loadAccountData() {
        viewModelScope.launch {
            try {
                _uiState.value = AccountUiState.Loading

                val userProfileResult = accountRepository.getUserProfile()
                val connectedAccountsResult = accountRepository.getConnectedAccountsStatus()

                if (userProfileResult.isSuccess && connectedAccountsResult.isSuccess) {
                    val profile = userProfileResult.getOrNull()
                    val accounts = connectedAccountsResult.getOrThrow()

                    _userProfile.value = profile
                    _connectedAccounts.value = accounts

                    // Always show Success - userProfile may be null if not signed in to Google
                    _uiState.value = AccountUiState.Success(profile, accounts)
                } else {
                    val error =
                        userProfileResult.exceptionOrNull()
                            ?: connectedAccountsResult.exceptionOrNull()
                            ?: Exception("Unknown error")
                    _uiState.value =
                        AccountUiState.Error(
                            error.message ?: AccountConstants.ERROR_AUTHENTICATION_FAILED,
                        )
                }
            } catch (e: Exception) {
                _uiState.value =
                    AccountUiState.Error(
                        e.message ?: AccountConstants.ERROR_AUTHENTICATION_FAILED,
                    )
            }
        }
    }

    /**
     * Link Discord account
     */
    fun linkDiscord() {
        linkDiscordAccount()
    }

    /**
     * Unlink Discord account
     */
    fun unlinkDiscord() {
        unlinkDiscordAccount()
    }

    /**
     * Link Discord account (internal)
     */
    private fun linkDiscordAccount() {
        viewModelScope.launch {
            try {
                _isDiscordLinking.value = true
                val result = accountRepository.linkDiscordAccount()

                if (result.isSuccess) {
                    _message.value = AccountConstants.SUCCESS_ACCOUNT_LINKED
                    loadAccountData() // Refresh data
                } else {
                    _message.value = result.exceptionOrNull()?.message
                        ?: AccountConstants.ERROR_AUTHENTICATION_FAILED
                }
            } catch (e: Exception) {
                _message.value = e.message ?: AccountConstants.ERROR_AUTHENTICATION_FAILED
            } finally {
                _isDiscordLinking.value = false
            }
        }
    }

    /**
     * Unlink Discord account
     */
    fun unlinkDiscordAccount() {
        viewModelScope.launch {
            try {
                _isDiscordLinking.value = true
                val result = accountRepository.unlinkDiscordAccount()

                if (result.isSuccess) {
                    _message.value = AccountConstants.SUCCESS_ACCOUNT_UNLINKED
                    loadAccountData() // Refresh data
                } else {
                    _message.value = result.exceptionOrNull()?.message
                        ?: AccountConstants.ERROR_AUTHENTICATION_FAILED
                }
            } catch (e: Exception) {
                _message.value = e.message ?: AccountConstants.ERROR_AUTHENTICATION_FAILED
            } finally {
                _isDiscordLinking.value = false
            }
        }
    }

    /**
     * Login to RetroAchievements
     */
    fun loginRetroAchievements(
        username: String,
        apiKey: String,
    ) {
        if (username.isBlank() || apiKey.isBlank()) {
            _message.value = "Username and API key cannot be empty"
            return
        }

        viewModelScope.launch {
            try {
                _isRetroAchievementsLogging.value = true
                val result = accountRepository.loginRetroAchievements(username, apiKey)

                if (result.isSuccess) {
                    _message.value = "Successfully logged in to RetroAchievements"
                    loadAccountData() // Refresh data
                } else {
                    _message.value = result.exceptionOrNull()?.message
                        ?: "RetroAchievements login failed"
                }
            } catch (e: Exception) {
                _message.value = e.message ?: "RetroAchievements login failed"
            } finally {
                _isRetroAchievementsLogging.value = false
            }
        }
    }

    /**
     * Logout from RetroAchievements
     */
    fun logoutRetroAchievements() {
        viewModelScope.launch {
            try {
                _isRetroAchievementsLogging.value = true
                val result = accountRepository.logoutRetroAchievements()

                if (result.isSuccess) {
                    _message.value = "Successfully logged out from RetroAchievements"
                    loadAccountData() // Refresh data
                } else {
                    _message.value = result.exceptionOrNull()?.message
                        ?: "RetroAchievements logout failed"
                }
            } catch (e: Exception) {
                _message.value = e.message ?: "RetroAchievements logout failed"
            } finally {
                _isRetroAchievementsLogging.value = false
            }
        }
    }

    /**
     * Refresh RetroAchievements stats and update UI
     */
    fun refreshRetroAchievementsStats() {
        viewModelScope.launch {
            try {
                val result = accountRepository.refreshRetroAchievementsStats()

                if (result.isSuccess) {
                    // Refresh the UI data after stats are updated
                    loadAccountData()
                }
            } catch (e: Exception) {
                // Silently handle errors for stats refresh
                android.util.Log.w("AccountManagementViewModel", "Failed to refresh RA stats", e)
            }
        }
    }

    /**
     * Try to generate PIN using existing idpToken first, then Firebase ID token.
     * If no valid token exists at all, signals pinNeedsAuth so the UI can launch sign-in.
     */
    fun tryGenerateDevicePIN() {
        viewModelScope.launch {
            try {
                _isPinGenerating.value = true

                // 1. Try with stored idpToken
                var pin = accountRepository.generateDevicePINDirect()

                // 2. Fallback: use Firebase ID token from current user
                if (pin == null) {
                    pin = accountRepository.generateDevicePINWithFirebaseToken()
                }

                if (pin != null) {
                    _generatedPin.value = pin
                    _message.value = "PIN generated successfully. Valid for ${AccountConstants.PIN_CODE_VALIDITY_MINUTES} minutes."
                } else {
                    // No valid token at all — need sign-in
                    _pinNeedsAuth.value = true
                }
            } catch (e: Exception) {
                _pinNeedsAuth.value = true
            } finally {
                _isPinGenerating.value = false
            }
        }
    }

    /**
     * Clear the pinNeedsAuth signal after the UI has handled it
     */
    fun clearPinNeedsAuth() {
        _pinNeedsAuth.value = false
    }

    /**
     * Generate PIN for cross-device sync with IdpResponse from sign-in
     *
     * @param response The IdpResponse from Firebase Auth UI sign-in
     */
    fun generateDevicePINWithResponse(response: IdpResponse) {
        viewModelScope.launch {
            try {
                _isPinGenerating.value = true

                // Set the sign-in result in the repository
                accountRepository.setSignInResult(response)

                // Now generate the PIN using the fresh idpToken
                val pin = accountRepository.generateDevicePINDirect()

                if (pin != null) {
                    _generatedPin.value = pin
                    _message.value = "PIN generated successfully. Valid for ${AccountConstants.PIN_CODE_VALIDITY_MINUTES} minutes."
                } else {
                    _message.value = "Failed to generate PIN"
                }
            } catch (e: Exception) {
                _message.value = e.message ?: "Failed to generate PIN"
            } finally {
                _isPinGenerating.value = false
            }
        }
    }

    /**
     * Generate PIN for cross-device sync
     * This will automatically trigger sign-in to get a fresh idpToken
     * @deprecated Use launchSignInForPinGeneration() in the Activity instead
     * @param activity The FragmentActivity to use for sign-in UI
     */
    fun generateDevicePIN(activity: FragmentActivity) {
        viewModelScope.launch {
            try {
                _isPinGenerating.value = true
                // Call the new method that automatically triggers sign-in
                val result = accountRepository.generateDevicePIN(activity)

                if (result.isSuccess) {
                    _generatedPin.value = result.getOrNull()
                    _message.value = "PIN generated successfully. Valid for ${AccountConstants.PIN_CODE_VALIDITY_MINUTES} minutes."
                } else {
                    _message.value = result.exceptionOrNull()?.message
                        ?: "Failed to generate PIN"
                }
            } catch (e: Exception) {
                _message.value = e.message ?: "Failed to generate PIN"
            } finally {
                _isPinGenerating.value = false
            }
        }
    }

    /**
     * Export user data in GDPR-compliant format
     */
    fun exportUserData() {
        viewModelScope.launch {
            try {
                val result = accountRepository.exportUserData()

                if (result.isSuccess) {
                    _exportedData.value = result.getOrNull()
                    _message.value = "Your data has been prepared for export. Please save the file."
                } else {
                    _message.value = result.exceptionOrNull()?.message
                        ?: "Data export failed. Please try again."
                }
            } catch (e: Exception) {
                _message.value = e.message ?: "Data export failed. Please try again."
            }
        }
    }

    /**
     * Delete user account with confirmation
     */
    fun deleteUserAccount() {
        viewModelScope.launch {
            try {
                val result = accountRepository.deleteUserAccount()

                if (result.isSuccess) {
                    _message.value = AccountConstants.SUCCESS_ACCOUNT_DELETED
                    _uiState.value = AccountUiState.NotAuthenticated
                } else {
                    _message.value = result.exceptionOrNull()?.message
                        ?: AccountConstants.ERROR_ACCOUNT_DELETION_FAILED
                }
            } catch (e: Exception) {
                _message.value = e.message ?: AccountConstants.ERROR_ACCOUNT_DELETION_FAILED
            }
        }
    }

    /**
     * Clear the current message
     */
    fun clearMessage() {
        _message.value = null
    }

    /**
     * Clear the generated PIN
     */
    fun clearGeneratedPin() {
        _generatedPin.value = null
    }

    /**
     * Clear the exported data
     */
    fun clearExportedData() {
        _exportedData.value = null
    }

    /**
     * Refresh account data
     */
    fun refresh() {
        loadAccountData()
    }
}
