package org.uoyabause.android.auth.ui

import android.app.Activity
import android.content.Intent
import com.firebase.ui.auth.IdpResponse
import com.google.firebase.auth.FirebaseUser

/**
 * Handles Firebase Auth sign-in result logic.
 *
 * Extracted from setupSignInLauncher callbacks for testability.
 * Used by both AccountManagementFragment and SimpleAccountManagementActivity.
 */
class SignInResultHandler {
    /**
     * Result of processing sign-in result.
     */
    sealed class Result {
        /** PIN generation callback should be invoked with the response */
        data class PinCallback(
            val response: IdpResponse?,
        ) : Result()

        /** RA login callback should be invoked */
        object RALoginSuccess : Result()

        /** Sign-in was cancelled (show cancel message) */
        object Cancelled : Result()

        /** Sign-in succeeded (show welcome message) */
        data class SignInSuccess(
            val displayName: String,
        ) : Result()

        /** Sign-in failed with error */
        data class SignInFailed(
            val errorMessage: String?,
        ) : Result()

        /** No action needed (e.g., RESULT_OK but no user yet - timing issue) */
        object NoAction : Result()
    }

    /**
     * Process the sign-in result and return the appropriate action.
     *
     * @param resultCode Activity result code (RESULT_OK or RESULT_CANCELED)
     * @param data Intent data from the result
     * @param currentUser Currently authenticated Firebase user (may be null)
     * @param hasPinCallback Whether a PIN generation callback is pending
     * @param hasRACallback Whether a RetroAchievements login callback is pending
     * @return The action to take based on the result
     */
    fun handleResult(
        resultCode: Int,
        data: Intent?,
        currentUser: FirebaseUser?,
        hasPinCallback: Boolean,
        hasRACallback: Boolean,
    ): Result {
        val response = IdpResponse.fromResultIntent(data)

        return when {
            hasPinCallback -> {
                if (currentUser != null && response != null) {
                    Result.PinCallback(response)
                } else {
                    Result.PinCallback(null)
                }
            }
            hasRACallback -> {
                if (currentUser != null) {
                    Result.RALoginSuccess
                } else {
                    if (response == null && resultCode != Activity.RESULT_OK) {
                        Result.Cancelled
                    } else {
                        Result.NoAction
                    }
                }
            }
            else -> {
                // Normal sign-in flow
                if (resultCode == Activity.RESULT_OK || currentUser != null) {
                    Result.SignInSuccess(currentUser?.displayName ?: "User")
                } else {
                    if (response == null) {
                        Result.Cancelled
                    } else {
                        val errorMsg = response.error?.message
                        Result.SignInFailed(errorMsg)
                    }
                }
            }
        }
    }
}
