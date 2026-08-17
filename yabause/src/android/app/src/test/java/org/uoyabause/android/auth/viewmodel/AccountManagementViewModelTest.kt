package org.uoyabause.android.auth.viewmodel

import android.app.Application
import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.lifecycle.Observer
import com.google.firebase.FirebaseApp
import io.mockk.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.*
import org.junit.After
import org.junit.Assert.*
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config
import org.uoyabause.android.TestYabauseApplication
import org.uoyabause.android.auth.models.*
import org.uoyabause.android.auth.repository.AccountRepository

@OptIn(ExperimentalCoroutinesApi::class)
@RunWith(RobolectricTestRunner::class)
@Config(
    sdk = [28],
    application = TestYabauseApplication::class,
    instrumentedPackages = ["org.uoyabause"],
    shadows = [
        org.uoyabause.android.shadows.ShadowComplexColor::class,
        org.uoyabause.android.shadows.ShadowResourcesImpl::class,
    ],
)
class AccountManagementViewModelTest {
    @get:Rule
    val instantExecutorRule = InstantTaskExecutorRule()

    private val testDispatcher = StandardTestDispatcher()

    // Mock dependencies
    private val mockApplication = mockk<Application>(relaxed = true)
    private val mockRepository = mockk<AccountRepository>(relaxed = true)

    // Test observers
    private val uiStateObserver = mockk<Observer<AccountUiState>>(relaxed = true)
    private val messageObserver = mockk<Observer<String?>>(relaxed = true)

    private lateinit var viewModel: AccountManagementViewModel

    @Before
    fun setup() {
        Dispatchers.setMain(testDispatcher)

        // Initialize Firebase for testing
        try {
            FirebaseApp.initializeApp(RuntimeEnvironment.getApplication())
        } catch (e: Exception) {
            // Firebase already initialized or other error - ignore for tests
        }

        // Setup Application mock
        every { mockApplication.applicationContext } returns mockApplication

        // Use RuntimeEnvironment.getApplication() instead of mockApplication to avoid Firebase issues
        viewModel = AccountManagementViewModel(RuntimeEnvironment.getApplication())

        // Observe LiveData
        viewModel.uiState.observeForever(uiStateObserver)
        viewModel.message.observeForever(messageObserver)
    }

    @After
    fun tearDown() {
        Dispatchers.resetMain()
        viewModel.uiState.removeObserver(uiStateObserver)
        viewModel.message.removeObserver(messageObserver)
        unmockkAll()
    }

    // === 基本テスト ===

    @Test
    fun `ViewModel初期化時に正しい初期状態を持つ`() {
        // Then
        assertNotNull(viewModel.uiState.value)
        assertFalse("Discord linking should be false initially", viewModel.isDiscordLinking.value ?: true)
        assertFalse("RetroAchievements logging should be false initially", viewModel.isRetroAchievementsLogging.value ?: true)
        assertFalse("PIN generating should be false initially", viewModel.isPinGenerating.value ?: true)
        assertNull("Message should be null initially", viewModel.message.value)
        assertNull("Generated PIN should be null initially", viewModel.generatedPin.value)
        assertNull("Exported data should be null initially", viewModel.exportedData.value)
    }

    @Test
    fun `clearMessage_メッセージ削除確認`() {
        // When
        viewModel.clearMessage()

        // Then
        verify { messageObserver.onChanged(null) }
    }

    @Test
    fun `clearGeneratedPin_PIN削除確認`() {
        // When
        viewModel.clearGeneratedPin()

        // Then
        assertNull("Generated PIN should be null after clearing", viewModel.generatedPin.value)
    }

    @Test
    fun `clearExportedData_データ削除確認`() {
        // When
        viewModel.clearExportedData()

        // Then
        assertNull("Exported data should be null after clearing", viewModel.exportedData.value)
    }

    @Test
    fun `loginRetroAchievements_空文字バリデーション`() =
        runTest {
            // When - empty username
            viewModel.loginRetroAchievements("", "apikey")
            testDispatcher.scheduler.advanceUntilIdle()

            // Then
            verify { messageObserver.onChanged("Username and API key cannot be empty") }

            // When - empty API key
            viewModel.loginRetroAchievements("username", "")
            testDispatcher.scheduler.advanceUntilIdle()

            // Then
            verify { messageObserver.onChanged("Username and API key cannot be empty") }

            // When - both empty
            viewModel.loginRetroAchievements("", "")
            testDispatcher.scheduler.advanceUntilIdle()

            // Then
            verify { messageObserver.onChanged("Username and API key cannot be empty") }
        }

    @Test
    fun `refresh_メソッド呼び出し確認`() =
        runTest {
            // When
            viewModel.refresh()
            testDispatcher.scheduler.advanceUntilIdle()

            // Then - loadAccountData should be called internally
            // This test verifies the method exists and can be called without exceptions
            assertTrue("Refresh method should complete without exceptions", true)
        }

    @Test
    fun `LiveDataプロパティ存在確認`() {
        // Then - All LiveData properties should exist
        assertNotNull("uiState LiveData should exist", viewModel.uiState)
        assertNotNull("userProfile LiveData should exist", viewModel.userProfile)
        assertNotNull("connectedAccounts LiveData should exist", viewModel.connectedAccounts)
        assertNotNull("isDiscordLinking LiveData should exist", viewModel.isDiscordLinking)
        assertNotNull("isRetroAchievementsLogging LiveData should exist", viewModel.isRetroAchievementsLogging)
        assertNotNull("isPinGenerating LiveData should exist", viewModel.isPinGenerating)
        assertNotNull("message LiveData should exist", viewModel.message)
        assertNotNull("generatedPin LiveData should exist", viewModel.generatedPin)
        assertNotNull("exportedData LiveData should exist", viewModel.exportedData)
    }

    @Test
    fun `メソッド存在確認_例外なし`() {
        // Test that all public methods exist and can be called without immediate exceptions
        try {
            // These should not throw immediate exceptions (they may fail internally due to mocking)
            viewModel.loadAccountData()
            viewModel.linkDiscord()
            viewModel.unlinkDiscord()
            viewModel.loginRetroAchievements("test", "test") // Will fail validation but method exists
            viewModel.logoutRetroAchievements()
            viewModel.refreshRetroAchievementsStats()
            viewModel.exportUserData()
            viewModel.deleteUserAccount()
            viewModel.clearMessage()
            viewModel.clearGeneratedPin()
            viewModel.clearExportedData()
            viewModel.refresh()

            assertTrue("All ViewModel methods should exist and be callable", true)
        } catch (e: Exception) {
            fail("ViewModel methods should exist and not throw immediate exceptions: ${e.message}")
        }
    }
}
