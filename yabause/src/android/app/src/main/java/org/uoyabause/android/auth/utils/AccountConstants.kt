package org.uoyabause.android.auth.utils

/**
 * Constants for account management
 */
object AccountConstants {
    // Account creation URLs
    const val DISCORD_CREATE_ACCOUNT_URL = "https://discord.com/register"
    const val RETROACHIEVEMENTS_CREATE_ACCOUNT_URL = "https://retroachievements.org/createaccount.php"

    // PIN code settings
    const val PIN_CODE_LENGTH = 6
    const val PIN_CODE_VALIDITY_MINUTES = 5
    const val PIN_CODE_VALIDITY_MILLIS = PIN_CODE_VALIDITY_MINUTES * 60 * 1000L

    // Request codes
    const val REQUEST_CODE_DISCORD_OAUTH = 10001
    const val REQUEST_CODE_GOOGLE_SIGN_IN = 10002

    // Preference keys
    const val PREF_KEY_ACCOUNT_MANAGEMENT = "pref_account_management"
    const val PREF_KEY_LAST_SYNC_TIME = "last_sync_time"

    // Error messages
    const val ERROR_NETWORK_UNAVAILABLE = "Network connection is unavailable"
    const val ERROR_AUTHENTICATION_FAILED = "Authentication failed"
    const val ERROR_ACCOUNT_DELETION_FAILED = "Failed to delete account"
    const val ERROR_DATA_EXPORT_FAILED = "Failed to export user data"

    // Success messages
    const val SUCCESS_ACCOUNT_LINKED = "Account successfully linked"
    const val SUCCESS_ACCOUNT_UNLINKED = "Account successfully unlinked"
    const val SUCCESS_DATA_EXPORTED = "User data exported successfully"
    const val SUCCESS_ACCOUNT_DELETED = "Account deleted successfully"
}
