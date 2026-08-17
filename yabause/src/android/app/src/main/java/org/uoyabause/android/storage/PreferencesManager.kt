/*  Copyright 2019 devMiyax(smiyaxdev@gmail.com)

    This file is part of YabaSanshiro.

    YabaSanshiro is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    YabaSanshiro is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with YabaSanshiro; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/
package org.uoyabause.android.storage

import android.content.Context
import android.content.SharedPreferences
import android.util.Log
import androidx.preference.PreferenceManager
import java.io.File

/**
 * Centralized SharedPreferences manager for the application.
 * Provides unified access to both global and user-specific preferences.
 */
class PreferencesManager(
    private val context: Context,
) {
    companion object {
        private const val TAG = "PreferencesManager"

        // Preference file names
        const val PREFS_DISCORD_AUTH = "discord_auth_prefs"
        const val PREFS_RETROACHIEVEMENTS = "retroachievements_auth"
        const val PREFS_DISCORD_LINKS = "discord_links"
        const val PREFS_MIGRATION = "migration_prefs"
        const val PREFS_APP_METADATA = "app_metadata"

        // Migration keys
        const val MIGRATION_KEY_V18_TO_V19 = "v18_to_v19_settings_reset"
        const val KEY_LAST_VERSION_CODE = "last_version_code"
        private const val KEY_SHOW_MIGRATION_DIALOG = "show_migration_dialog"

        // SharedPreferences files that must NOT be cleared during migration
        private val PROTECTED_PREFS = setOf(
            PREFS_DISCORD_AUTH,
            PREFS_RETROACHIEVEMENTS,
            PREFS_DISCORD_LINKS,
            PREFS_MIGRATION,
            PREFS_APP_METADATA,
            "private",
            "com.google.firebase.crashlytics",
            "com.google.android.gms.measurement.prefs",
        )

        /**
         * Check version and perform migration if needed.
         * Called from YabauseApplication.onCreate().
         */
        fun performVersionMigrationIfNeeded(context: Context, currentVersionCode: Int) {
            val appMetadata = context.getSharedPreferences(PREFS_APP_METADATA, Context.MODE_PRIVATE)
            val lastVersionCode = appMetadata.getInt(KEY_LAST_VERSION_CODE, 0)

            val needsMigration = when {
                // Known v18 version code upgrading to v19+
                lastVersionCode in 1 until 300 && currentVersionCode >= 300 -> true
                // v18 had no version tracking, so lastVersionCode defaults to 0.
                // Detect v18 upgrade by checking if default prefs already have data.
                lastVersionCode == 0 && currentVersionCode >= 300 -> {
                    val defaultPrefs = PreferenceManager.getDefaultSharedPreferences(context)
                    defaultPrefs.all.isNotEmpty()
                }
                else -> false
            }

            if (needsMigration) {
                val manager = getInstance(context)
                manager.performV18ToV19Migration(context)
            }

            appMetadata.edit().putInt(KEY_LAST_VERSION_CODE, currentVersionCode).apply()
        }

        @Volatile
        private var instance: PreferencesManager? = null

        fun getInstance(context: Context): PreferencesManager = instance ?: synchronized(this) {
            instance ?: PreferencesManager(context.applicationContext).also { instance = it }
        }

        @androidx.annotation.VisibleForTesting
        fun resetForTesting() {
            instance = null
        }
    }

    /**
     * Get global SharedPreferences (not user-specific)
     * @param prefsName The name of the preferences file
     * @return SharedPreferences instance
     */
    fun getGlobalPrefs(prefsName: String): SharedPreferences = context.getSharedPreferences(prefsName, Context.MODE_PRIVATE)

    /**
     * Get user-specific SharedPreferences
     * @param baseName The base name of the preferences file
     * @param firebaseUid The Firebase user ID
     * @return SharedPreferences instance for this specific user
     */
    fun getUserPrefs(
        baseName: String,
        firebaseUid: String,
    ): SharedPreferences {
        val prefsName = "${baseName}_$firebaseUid"
        return context.getSharedPreferences(prefsName, Context.MODE_PRIVATE)
    }

    /**
     * Store a string value with expiration time
     * @param prefs SharedPreferences to use
     * @param key The key for the value
     * @param value The value to store
     * @param expiresAtMillis The expiration time in milliseconds
     */
    fun putStringWithExpiration(
        prefs: SharedPreferences,
        key: String,
        value: String,
        expiresAtMillis: Long,
    ) {
        prefs
            .edit()
            .putString(key, value)
            .putLong("${key}_expires_at", expiresAtMillis)
            .apply()
        Log.d(TAG, "Stored value with expiration for key: $key")
    }

    /**
     * Get a string value if not expired
     * @param prefs SharedPreferences to use
     * @param key The key for the value
     * @return The value or null if not found or expired
     */
    fun getStringIfNotExpired(
        prefs: SharedPreferences,
        key: String,
    ): String? {
        val expiresAt = prefs.getLong("${key}_expires_at", 0)
        if (System.currentTimeMillis() > expiresAt) {
            Log.d(TAG, "Value for key $key has expired")
            clearKeyWithExpiration(prefs, key)
            return null
        }
        return prefs.getString(key, null)
    }

    /**
     * Clear a key and its expiration timestamp
     * @param prefs SharedPreferences to use
     * @param key The key to clear
     */
    fun clearKeyWithExpiration(
        prefs: SharedPreferences,
        key: String,
    ) {
        prefs
            .edit()
            .remove(key)
            .remove("${key}_expires_at")
            .apply()
        Log.d(TAG, "Cleared key with expiration: $key")
    }

    /**
     * Check if a migration has been completed
     * @param migrationKey The migration identifier
     * @return true if migration was completed
     */
    fun isMigrationCompleted(migrationKey: String): Boolean {
        val prefs = getGlobalPrefs(PREFS_MIGRATION)
        return prefs.getBoolean(migrationKey, false)
    }

    /**
     * Mark a migration as completed
     * @param migrationKey The migration identifier
     */
    fun markMigrationCompleted(migrationKey: String) {
        val prefs = getGlobalPrefs(PREFS_MIGRATION)
        prefs.edit().putBoolean(migrationKey, true).apply()
        Log.d(TAG, "Migration marked as completed: $migrationKey")
    }

    /**
     * Perform v18 to v19 settings migration.
     * Clears global and game-specific preferences while preserving auth data.
     */
    fun performV18ToV19Migration(context: Context) {
        if (isMigrationCompleted(MIGRATION_KEY_V18_TO_V19)) {
            Log.d(TAG, "v18→v19 migration already completed, skipping")
            return
        }

        try {
            Log.d(TAG, "Starting v18→v19 migration: clearing settings")

            // Clear global preferences
            PreferenceManager
                .getDefaultSharedPreferences(context)
                .edit()
                .clear()
                .apply()
            Log.d(TAG, "Global preferences cleared")

            // Clear all non-protected SharedPreferences files
            clearNonProtectedPreferences(context)

            // Mark migration as completed
            markMigrationCompleted(MIGRATION_KEY_V18_TO_V19)

            // Set dialog display flag
            val migrationPrefs = getGlobalPrefs(PREFS_MIGRATION)
            migrationPrefs.edit().putBoolean(KEY_SHOW_MIGRATION_DIALOG, true).apply()

            Log.d(TAG, "v18→v19 migration completed successfully")
        } catch (e: Exception) {
            Log.e(TAG, "v18→v19 migration failed", e)
        }
    }

    /**
     * Clear all SharedPreferences files except protected ones.
     * This handles both standard and Harmony SharedPreferences.
     */
    private fun clearNonProtectedPreferences(context: Context) {
        try {
            val prefsDir = File(context.applicationInfo.dataDir, "shared_prefs")
            if (!prefsDir.exists()) return

            val defaultPrefsName = context.packageName + "_preferences"

            prefsDir.listFiles()?.forEach { file ->
                val name = file.nameWithoutExtension
                if (name != defaultPrefsName &&
                    name !in PROTECTED_PREFS &&
                    !PROTECTED_PREFS.any { name.startsWith(it) }
                ) {
                    try {
                        context
                            .getSharedPreferences(name, Context.MODE_PRIVATE)
                            .edit()
                            .clear()
                            .apply()
                        Log.d(TAG, "Cleared preferences: $name")
                    } catch (e: Exception) {
                        Log.w(TAG, "Failed to clear preferences: $name", e)
                    }
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to enumerate SharedPreferences files", e)
        }
    }

    /**
     * Check if migration dialog should be shown
     */
    fun shouldShowMigrationDialog(): Boolean {
        val prefs = getGlobalPrefs(PREFS_MIGRATION)
        return prefs.getBoolean(KEY_SHOW_MIGRATION_DIALOG, false)
    }

    /**
     * Mark migration dialog as shown (so it won't show again)
     */
    fun markMigrationDialogShown() {
        val prefs = getGlobalPrefs(PREFS_MIGRATION)
        prefs.edit().putBoolean(KEY_SHOW_MIGRATION_DIALOG, false).apply()
        Log.d(TAG, "Migration dialog marked as shown")
    }
}
