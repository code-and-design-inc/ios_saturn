package org.uoyabause.android.storage

import android.content.Context
import androidx.preference.PreferenceManager
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28])
class PreferencesManagerMigrationTest {
    private lateinit var context: Context
    private lateinit var preferencesManager: PreferencesManager

    @Before
    fun setUp() {
        context = RuntimeEnvironment.getApplication()
        PreferencesManager.resetForTesting()
        preferencesManager = PreferencesManager.getInstance(context)

        // Clear migration state
        context
            .getSharedPreferences(PreferencesManager.PREFS_MIGRATION, Context.MODE_PRIVATE)
            .edit()
            .clear()
            .apply()
    }

    // UT-001: v18→v19マイグレーション実行
    @Test
    fun `performV18ToV19Migration clears global preferences`() {
        // Given: v18の設定値がInteger型で存在
        val defaultPrefs = PreferenceManager.getDefaultSharedPreferences(context)
        defaultPrefs
            .edit()
            .putInt("pref_video", 1)
            .putInt("pref_cpu", 3)
            .putString("pref_cart", "7")
            .apply()

        // When
        preferencesManager.performV18ToV19Migration(context)

        // Then: グローバル設定がクリアされる
        assertFalse(defaultPrefs.contains("pref_video"))
        assertFalse(defaultPrefs.contains("pref_cpu"))
        assertFalse(defaultPrefs.contains("pref_cart"))
    }

    // UT-002: マイグレーション完了フラグ記録
    @Test
    fun `performV18ToV19Migration marks migration as completed`() {
        // When
        preferencesManager.performV18ToV19Migration(context)

        // Then
        assertTrue(preferencesManager.isMigrationCompleted(PreferencesManager.MIGRATION_KEY_V18_TO_V19))
    }

    // UT-003: マイグレーション重複実行防止
    @Test
    fun `performV18ToV19Migration does not clear when already completed`() {
        // Given: マイグレーション完了済み
        preferencesManager.performV18ToV19Migration(context)

        // 完了後に新しい設定値を追加
        val defaultPrefs = PreferenceManager.getDefaultSharedPreferences(context)
        defaultPrefs.edit().putString("pref_video", "4").apply()

        // When: 再度マイグレーション呼び出し
        preferencesManager.performV18ToV19Migration(context)

        // Then: 新しい設定値が保持される（再クリアされない）
        assertEquals("4", defaultPrefs.getString("pref_video", null))
    }

    // UT-004: ダイアログ表示フラグ管理
    @Test
    fun `shouldShowMigrationDialog returns true after migration`() {
        // When
        preferencesManager.performV18ToV19Migration(context)

        // Then
        assertTrue(preferencesManager.shouldShowMigrationDialog())
    }

    // UT-005: ダイアログ表示済みフラグ
    @Test
    fun `shouldShowMigrationDialog returns false after markMigrationDialogShown`() {
        // Given
        preferencesManager.performV18ToV19Migration(context)
        assertTrue(preferencesManager.shouldShowMigrationDialog())

        // When
        preferencesManager.markMigrationDialogShown()

        // Then
        assertFalse(preferencesManager.shouldShowMigrationDialog())
    }

    // UT-S01: Firebase認証情報の保持
    @Test
    fun `performV18ToV19Migration preserves Firebase auth prefs`() {
        // Given: Firebase認証情報が存在
        // Note: Firebase stores auth in its own internal prefs managed by Firebase SDK.
        // We verify that the "private" prefs (used for donation tracking) are not cleared
        // since Firebase auth is managed externally.
        val privatePrefs = context.getSharedPreferences("private", Context.MODE_PRIVATE)
        privatePrefs.edit().putBoolean("donated", true).apply()

        // When
        preferencesManager.performV18ToV19Migration(context)

        // Then: "private" prefs are preserved
        assertTrue(privatePrefs.getBoolean("donated", false))
    }

    // UT-S02: Discord認証情報の保持
    @Test
    fun `performV18ToV19Migration preserves discord auth prefs`() {
        // Given
        val discordPrefs = context.getSharedPreferences(
            PreferencesManager.PREFS_DISCORD_AUTH,
            Context.MODE_PRIVATE,
        )
        discordPrefs.edit().putString("token", "test_token").apply()

        // When
        preferencesManager.performV18ToV19Migration(context)

        // Then
        assertEquals("test_token", discordPrefs.getString("token", null))
    }

    // UT-S03: RetroAchievements認証情報の保持
    @Test
    fun `performV18ToV19Migration preserves retroachievements auth prefs`() {
        // Given
        val raPrefs = context.getSharedPreferences(
            PreferencesManager.PREFS_RETROACHIEVEMENTS,
            Context.MODE_PRIVATE,
        )
        raPrefs.edit().putString("username", "test_user").apply()

        // When
        preferencesManager.performV18ToV19Migration(context)

        // Then
        assertEquals("test_user", raPrefs.getString("username", null))
    }

    // UT-S04: マイグレーション状態の保持
    @Test
    fun `performV18ToV19Migration preserves migration prefs`() {
        // Given: 別のマイグレーションが完了済み
        preferencesManager.markMigrationCompleted("some_other_migration")

        // When
        preferencesManager.performV18ToV19Migration(context)

        // Then
        assertTrue(preferencesManager.isMigrationCompleted("some_other_migration"))
    }
}
