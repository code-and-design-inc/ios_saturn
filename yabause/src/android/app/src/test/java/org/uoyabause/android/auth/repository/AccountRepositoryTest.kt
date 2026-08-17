package org.uoyabause.android.auth.repository

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import com.google.android.gms.tasks.Tasks
import com.google.firebase.FirebaseApp
import io.mockk.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.*
import org.junit.After
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config
import org.uoyabause.android.TestYabauseApplication
import org.uoyabause.android.auth.DiscordAuthManager
import org.uoyabause.android.auth.FirebaseAuthManager
import org.uoyabause.android.auth.RetroAchievementsAuthManager
import org.uoyabause.android.auth.models.*

/**
 * Unit tests for AccountRepository
 * Tests the coordination between different authentication managers
 */
@OptIn(ExperimentalCoroutinesApi::class)
@RunWith(RobolectricTestRunner::class)
@Config(
    sdk = [28],
    application = TestYabauseApplication::class,
)
class AccountRepositoryTest {
    private lateinit var context: Context
    private lateinit var repository: AccountRepository

    // Mock authentication managers
    private val mockFirebaseAuthManager = mockk<FirebaseAuthManager>(relaxed = true)
    private val mockDiscordAuthManager = mockk<DiscordAuthManager>(relaxed = true)
    private val mockRetroAchievementsAuthManager = mockk<RetroAchievementsAuthManager>(relaxed = true)

    @Before
    fun setup() {
        // Initialize Firebase for testing
        try {
            FirebaseApp.initializeApp(RuntimeEnvironment.getApplication())
        } catch (e: Exception) {
            // Firebase already initialized - ignore
        }

        context = ApplicationProvider.getApplicationContext()

        // Create repository with mocked dependencies
        repository =
            AccountRepository(
                context = context,
                firebaseAuthManager = mockFirebaseAuthManager,
                discordAuthManager = mockDiscordAuthManager,
                retroAchievementsAuthManager = mockRetroAchievementsAuthManager,
            )
    }

    @After
    fun tearDown() {
        unmockkAll()
    }

    // === Firebase Authentication Tests ===

    @Test
    fun `getUserProfile returns null when no user is authenticated`() =
        runTest {
            // Given
            mockkStatic("com.google.firebase.auth.FirebaseAuth")
            val mockFirebaseAuth = mockk<com.google.firebase.auth.FirebaseAuth>()
            every {
                com.google.firebase.auth.FirebaseAuth
                    .getInstance()
            } returns mockFirebaseAuth
            every { mockFirebaseAuth.currentUser } returns null

            // When
            val result = repository.getUserProfile()

            // Then
            assertTrue("Should succeed with null user", result.isSuccess)
            assertNull("User profile should be null", result.getOrNull())
        }

    @Test
    fun `getUserProfile returns user profile when user is authenticated`() =
        runTest {
            // Given
            mockkStatic("com.google.firebase.auth.FirebaseAuth")
            val mockFirebaseAuth = mockk<com.google.firebase.auth.FirebaseAuth>()
            val mockUser = mockk<com.google.firebase.auth.FirebaseUser>()
            val mockMetadata = mockk<com.google.firebase.auth.FirebaseUserMetadata>()

            every {
                com.google.firebase.auth.FirebaseAuth
                    .getInstance()
            } returns mockFirebaseAuth
            every { mockFirebaseAuth.currentUser } returns mockUser
            every { mockUser.uid } returns "test-uid"
            every { mockUser.displayName } returns "Test User"
            every { mockUser.email } returns "test@example.com"
            every { mockUser.photoUrl } returns null
            every { mockUser.isEmailVerified } returns true
            every { mockUser.phoneNumber } returns null
            every { mockUser.metadata } returns mockMetadata
            every { mockUser.providerData } returns emptyList()
            every { mockMetadata.lastSignInTimestamp } returns 1234567890L

            // When
            val result = repository.getUserProfile()

            // Then
            assertTrue("Should succeed", result.isSuccess)
            val profile = result.getOrNull()
            assertNotNull("Profile should not be null", profile)
            assertEquals("UID should match", "test-uid", profile?.uid)
            assertEquals("Display name should match", "Test User", profile?.displayName)
            assertEquals("Email should match", "test@example.com", profile?.email)
            assertTrue("Email should be verified", profile?.isEmailVerified == true)
        }

    // === Discord Connection Tests ===

    @Test
    fun `linkDiscordAccount calls Discord auth manager`() =
        runTest {
            // Given
            every { mockDiscordAuthManager.startDiscordLogin() } just Runs
            coEvery { mockDiscordAuthManager.cleanupFirebaseDiscordData(any()) } returns true

            mockkStatic("com.google.firebase.auth.FirebaseAuth")
            val mockFirebaseAuth = mockk<com.google.firebase.auth.FirebaseAuth>()
            val mockUser = mockk<com.google.firebase.auth.FirebaseUser>()
            every {
                com.google.firebase.auth.FirebaseAuth
                    .getInstance()
            } returns mockFirebaseAuth
            every { mockFirebaseAuth.currentUser } returns mockUser
            every { mockUser.uid } returns "test-uid"

            // When
            val result = repository.linkDiscordAccount()

            // Then
            assertTrue("Should succeed", result.isSuccess)
            assertTrue("Should return true", result.getOrNull() == true)
            verify { mockDiscordAuthManager.startDiscordLogin() }
            coVerify { mockDiscordAuthManager.cleanupFirebaseDiscordData("test-uid") }
        }

    @Test
    fun `unlinkDiscordAccount calls Discord auth manager`() =
        runTest {
            // Given
            every { mockDiscordAuthManager.unlinkDiscord(any()) } returns true

            mockkStatic("com.google.firebase.auth.FirebaseAuth")
            val mockFirebaseAuth = mockk<com.google.firebase.auth.FirebaseAuth>()
            val mockUser = mockk<com.google.firebase.auth.FirebaseUser>()
            every {
                com.google.firebase.auth.FirebaseAuth
                    .getInstance()
            } returns mockFirebaseAuth
            every { mockFirebaseAuth.currentUser } returns mockUser
            every { mockUser.uid } returns "test-uid"

            // When
            val result = repository.unlinkDiscordAccount()

            // Then
            assertTrue("Should succeed", result.isSuccess)
            assertTrue("Should return true", result.getOrNull() == true)
            verify { mockDiscordAuthManager.unlinkDiscord("test-uid") }
        }

    @Test
    fun `unlinkDiscordAccount fails when no user is authenticated`() =
        runTest {
            // Given
            mockkStatic("com.google.firebase.auth.FirebaseAuth")
            val mockFirebaseAuth = mockk<com.google.firebase.auth.FirebaseAuth>()
            every {
                com.google.firebase.auth.FirebaseAuth
                    .getInstance()
            } returns mockFirebaseAuth
            every { mockFirebaseAuth.currentUser } returns null

            // When
            val result = repository.unlinkDiscordAccount()

            // Then
            assertTrue("Should fail", result.isFailure)
            assertEquals("Should have correct error message", "No authenticated user", result.exceptionOrNull()?.message)
        }

    // === RetroAchievements Authentication Tests ===

    @Test
    fun `loginRetroAchievements calls RA auth manager`() =
        runTest {
            // Given
            every { mockRetroAchievementsAuthManager.loginRetroAchievements(any(), any()) } returns Unit
            every { mockRetroAchievementsAuthManager.isRetroAchievementsLoggedIn() } returnsMany listOf(false, false, true)
            every { mockRetroAchievementsAuthManager.getCurrentRAUsername() } returns "testuser"

            // When
            val result = repository.loginRetroAchievements("testuser", "apikey")

            // Then
            assertTrue("Should succeed", result.isSuccess)
            assertTrue("Should return true", result.getOrNull() == true)
            verify { mockRetroAchievementsAuthManager.loginRetroAchievements("testuser", "apikey") }
        }

    @Test
    fun `logoutRetroAchievements calls RA auth manager`() =
        runTest {
            // Given
            every { mockRetroAchievementsAuthManager.logoutRetroAchievements() } returns Unit

            // When
            val result = repository.logoutRetroAchievements()

            // Then
            assertTrue("Should succeed", result.isSuccess)
            assertTrue("Should return true", result.getOrNull() == true)
            verify { mockRetroAchievementsAuthManager.logoutRetroAchievements() }
        }

    // === Connected Accounts Status Tests ===

    @Test
    fun `getConnectedAccountsStatus returns correct state`() =
        runTest {
            // Given - Mock Firebase user
            mockkStatic("com.google.firebase.auth.FirebaseAuth")
            val mockFirebaseAuth = mockk<com.google.firebase.auth.FirebaseAuth>()
            val mockUser = mockk<com.google.firebase.auth.FirebaseUser>()
            val mockMetadata = mockk<com.google.firebase.auth.FirebaseUserMetadata>()

            every {
                com.google.firebase.auth.FirebaseAuth
                    .getInstance()
            } returns mockFirebaseAuth
            every { mockFirebaseAuth.currentUser } returns mockUser
            every { mockUser.uid } returns "test-uid"
            every { mockUser.email } returns "test@example.com"
            every { mockUser.displayName } returns "Test User"
            every { mockUser.photoUrl } returns null
            every { mockUser.metadata } returns mockMetadata
            every { mockMetadata.lastSignInTimestamp } returns 1234567890L

            // Given - Mock Discord connection
            every { mockDiscordAuthManager.isDiscordLinked("test-uid") } returns true
            every { mockDiscordAuthManager.getDiscordUserInfo("test-uid") } returns
                mapOf(
                    "username" to "discord_user",
                    "displayName" to "Discord User",
                    "avatarUrl" to "https://example.com/avatar.png",
                    "updatedAt" to 1234567890L,
                )

            // Given - Mock RetroAchievements connection
            every { mockRetroAchievementsAuthManager.isRetroAchievementsLoggedIn() } returns true
            every { mockRetroAchievementsAuthManager.getCurrentRAUsername() } returns "ra_user"

            // Mock static methods for RA stats
            mockkStatic("org.uoyabause.android.achievements.RetroAchievementsManager")
            every {
                org.uoyabause.android.achievements.RetroAchievementsManager
                    .getUserStatsNative()
            } returns intArrayOf(100, 50, 5)
            every {
                org.uoyabause.android.achievements.RetroAchievementsManager
                    .getUserAvatarUrlNative()
            } returns "https://example.com/ra_avatar.png"

            // When
            val result = repository.getConnectedAccountsStatus()

            // Then
            assertTrue("Should succeed", result.isSuccess)
            val connectedAccounts = result.getOrNull()
            assertNotNull("Connected accounts should not be null", connectedAccounts)

            // Verify Firebase state
            assertTrue("Firebase should be connected", connectedAccounts?.firebase?.isConnected == true)
            assertEquals("Firebase email should match", "test@example.com", connectedAccounts?.firebase?.username)

            // Verify Discord state
            assertTrue("Discord should be connected", connectedAccounts?.discord?.isConnected == true)
            assertEquals("Discord username should match", "discord_user", connectedAccounts?.discord?.username)

            // Verify RetroAchievements state
            assertTrue("RA should be connected", connectedAccounts?.retroAchievements?.isConnected == true)
            assertEquals("RA username should match", "ra_user", connectedAccounts?.retroAchievements?.username)
        }

    // === Data Export Tests ===

    @Test
    fun `exportUserData returns GDPR compliant JSON`() =
        runTest {
            // Given
            mockkStatic("com.google.firebase.auth.FirebaseAuth")
            val mockFirebaseAuth = mockk<com.google.firebase.auth.FirebaseAuth>()
            val mockUser = mockk<com.google.firebase.auth.FirebaseUser>()
            val mockMetadata = mockk<com.google.firebase.auth.FirebaseUserMetadata>()

            every {
                com.google.firebase.auth.FirebaseAuth
                    .getInstance()
            } returns mockFirebaseAuth
            every { mockFirebaseAuth.currentUser } returns mockUser
            every { mockUser.uid } returns "test-uid"
            every { mockUser.email } returns "test@example.com"
            every { mockUser.displayName } returns "Test User"
            every { mockUser.photoUrl } returns null
            every { mockUser.metadata } returns mockMetadata
            every { mockMetadata.lastSignInTimestamp } returns 1234567890L
            every { mockMetadata.creationTimestamp } returns 1234567890L

            // When
            val result = repository.exportUserData()

            // Then
            assertTrue("Should succeed", result.isSuccess)
            val exportData = result.getOrNull()
            assertNotNull("Export data should not be null", exportData)
            assertTrue("Export should contain GDPR metadata", exportData?.contains("exportMetadata") == true)
            assertTrue("Export should contain user profile", exportData?.contains("userProfile") == true)
            assertTrue("Export should contain connected accounts", exportData?.contains("connectedAccounts") == true)
            assertTrue("Export should contain privacy information", exportData?.contains("privacyInformation") == true)
        }

    @Test
    fun `exportUserData fails when no user is authenticated`() =
        runTest {
            // Given
            mockkStatic("com.google.firebase.auth.FirebaseAuth")
            val mockFirebaseAuth = mockk<com.google.firebase.auth.FirebaseAuth>()
            every {
                com.google.firebase.auth.FirebaseAuth
                    .getInstance()
            } returns mockFirebaseAuth
            every { mockFirebaseAuth.currentUser } returns null

            // When
            val result = repository.exportUserData()

            // Then
            assertTrue("Should fail", result.isFailure)
            assertEquals("Should have correct error message", "User not authenticated", result.exceptionOrNull()?.message)
        }

    // === Account Deletion Tests ===

    @Test
    fun `deleteUserAccount calls all cleanup methods`() =
        runTest {
            // Given
            every { mockDiscordAuthManager.unlinkDiscord(any()) } returns true
            every { mockRetroAchievementsAuthManager.logoutRetroAchievements() } returns Unit

            mockkStatic("com.google.firebase.auth.FirebaseAuth")
            val mockFirebaseAuth = mockk<com.google.firebase.auth.FirebaseAuth>()
            val mockUser = mockk<com.google.firebase.auth.FirebaseUser>()
            every {
                com.google.firebase.auth.FirebaseAuth
                    .getInstance()
            } returns mockFirebaseAuth
            every { mockFirebaseAuth.currentUser } returns mockUser
            every { mockUser.delete() } returns Tasks.forResult<Void>(null)

            // When
            val result = repository.deleteUserAccount()

            // Then
            assertTrue("Should succeed", result.isSuccess)
            verify { mockRetroAchievementsAuthManager.logoutRetroAchievements() }
            verify { mockUser.delete() }
        }
}
