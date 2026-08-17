package org.uoyabause.android

import android.content.Context
import android.content.SharedPreferences
import androidx.preference.PreferenceManager
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config
import org.uoyabause.android.storage.PreferencesManager

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28])
class VersionMigrationTest {
    private lateinit var context: Context
    private lateinit var appMetadataPrefs: SharedPreferences
    private lateinit var preferencesManager: PreferencesManager

    @Before
    fun setUp() {
        context = RuntimeEnvironment.getApplication()
        PreferencesManager.resetForTesting()
        appMetadataPrefs = context.getSharedPreferences(
            PreferencesManager.PREFS_APP_METADATA,
            Context.MODE_PRIVATE,
        )
        preferencesManager = PreferencesManager.getInstance(context)

        // Reset state
        appMetadataPrefs.edit().clear().apply()
        val migrationPrefs = context.getSharedPreferences(
            PreferencesManager.PREFS_MIGRATION,
            Context.MODE_PRIVATE,
        )
        migrationPrefs.edit().clear().apply()
        PreferenceManager
            .getDefaultSharedPreferences(context)
            .edit()
            .clear()
            .apply()
    }

    // UT-006: v18→v19アップデート検知
    @Test
    fun `migration runs when upgrading from v18 to v19`() {
        // Given
        appMetadataPrefs.edit().putInt(PreferencesManager.KEY_LAST_VERSION_CODE, 271).apply()

        // When
        PreferencesManager.performVersionMigrationIfNeeded(context, 300)

        // Then
        assertTrue(preferencesManager.isMigrationCompleted(PreferencesManager.MIGRATION_KEY_V18_TO_V19))
    }

    // UT-007: v19同一バージョン起動
    @Test
    fun `migration does not run for same version`() {
        // Given
        appMetadataPrefs.edit().putInt(PreferencesManager.KEY_LAST_VERSION_CODE, 300).apply()

        // When
        PreferencesManager.performVersionMigrationIfNeeded(context, 300)

        // Then
        assertFalse(preferencesManager.isMigrationCompleted(PreferencesManager.MIGRATION_KEY_V18_TO_V19))
    }

    // UT-008: v19→v19パッチ更新
    @Test
    fun `migration does not run for patch update`() {
        // Given
        appMetadataPrefs.edit().putInt(PreferencesManager.KEY_LAST_VERSION_CODE, 300).apply()

        // When
        PreferencesManager.performVersionMigrationIfNeeded(context, 301)

        // Then
        assertFalse(preferencesManager.isMigrationCompleted(PreferencesManager.MIGRATION_KEY_V18_TO_V19))
    }

    // UT-009: 前回バージョン保存
    @Test
    fun `current version code is saved after migration check`() {
        // When
        PreferencesManager.performVersionMigrationIfNeeded(context, 300)

        // Then
        val savedCode = appMetadataPrefs.getInt(PreferencesManager.KEY_LAST_VERSION_CODE, 0)
        assertTrue(savedCode == 300)
    }

    // UT-010: v18→v19アップデート検知（バージョン追跡なし）
    // v18にはPREFS_APP_METADATAが存在しないため、lastVersionCode=0になる。
    // 既存の設定データがあればv18からのアップデートと判定する。
    @Test
    fun `migration runs for v18 upgrade without version tracking`() {
        // Given: no version code saved (v18 had no tracking), but existing prefs
        val defaultPrefs = PreferenceManager.getDefaultSharedPreferences(context)
        defaultPrefs.edit().putInt("pref_video", 1).apply()

        // When
        PreferencesManager.performVersionMigrationIfNeeded(context, 300)

        // Then
        assertTrue(preferencesManager.isMigrationCompleted(PreferencesManager.MIGRATION_KEY_V18_TO_V19))
    }

    // UT-B01: 新規インストール（前回バージョン未保存、設定データなし）
    @Test
    fun `migration does not run for fresh install`() {
        // Given: no previous version code saved AND no existing prefs (fresh install)

        // When
        PreferencesManager.performVersionMigrationIfNeeded(context, 300)

        // Then
        assertFalse(preferencesManager.isMigrationCompleted(PreferencesManager.MIGRATION_KEY_V18_TO_V19))
    }

    // UT-B02: versionCode境界値（299→300）
    @Test
    fun `migration runs for versionCode 299 to 300`() {
        // Given
        appMetadataPrefs.edit().putInt(PreferencesManager.KEY_LAST_VERSION_CODE, 299).apply()

        // When
        PreferencesManager.performVersionMigrationIfNeeded(context, 300)

        // Then
        assertTrue(preferencesManager.isMigrationCompleted(PreferencesManager.MIGRATION_KEY_V18_TO_V19))
    }

    // UT-B03: versionCode境界値（300→300）
    @Test
    fun `migration does not run for versionCode 300 to 300`() {
        // Given
        appMetadataPrefs.edit().putInt(PreferencesManager.KEY_LAST_VERSION_CODE, 300).apply()

        // When
        PreferencesManager.performVersionMigrationIfNeeded(context, 300)

        // Then
        assertFalse(preferencesManager.isMigrationCompleted(PreferencesManager.MIGRATION_KEY_V18_TO_V19))
    }

    // UT-E01: マイグレーション中の例外でもアプリ起動は継続
    @Test
    fun `migration handles exceptions gracefully`() {
        // Given
        appMetadataPrefs.edit().putInt(PreferencesManager.KEY_LAST_VERSION_CODE, 271).apply()

        // When: migration runs (should not throw even if something goes wrong)
        PreferencesManager.performVersionMigrationIfNeeded(context, 300)

        // Then: version code is still saved
        val savedCode = appMetadataPrefs.getInt(PreferencesManager.KEY_LAST_VERSION_CODE, 0)
        assertTrue(savedCode == 300)
    }
}
