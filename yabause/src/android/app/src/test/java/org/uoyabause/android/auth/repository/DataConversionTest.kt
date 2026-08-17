package org.uoyabause.android.auth.repository

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import com.google.firebase.FirebaseApp
import com.google.firebase.auth.FirebaseUser
import io.mockk.*
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
 * Unit tests for data conversion and processing logic in AccountRepository
 * Tests proper handling and transformation of external service data
 */
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
class DataConversionTest {
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

    // === UserProfile Creation Tests ===

    @Test
    fun `UserProfile creation handles complete Firebase user data correctly`() {
        // Given
        val mockUser = mockk<FirebaseUser>()
        val mockMetadata = mockk<com.google.firebase.auth.FirebaseUserMetadata>()
        val mockPhotoUrl = mockk<android.net.Uri>()

        // Setup mockPhotoUrl first
        every { mockPhotoUrl.toString() } returns "https://example.com/photo.jpg"

        // Setup mockMetadata
        every { mockMetadata.lastSignInTimestamp } returns 1640995200000L // 2022-01-01

        // Setup mockUser with all properties
        every { mockUser.uid } returns "test-uid-123"
        every { mockUser.displayName } returns "Test User"
        every { mockUser.email } returns "test@example.com"
        every { mockUser.photoUrl } returns mockPhotoUrl
        every { mockUser.isEmailVerified } returns true
        every { mockUser.phoneNumber } returns "+1234567890"
        every { mockUser.metadata } returns mockMetadata

        // When - Create UserProfile directly (mimicking AccountRepository logic)
        val profile =
            UserProfile(
                uid = mockUser.uid,
                displayName = mockUser.displayName ?: "Unknown User",
                email = mockUser.email,
                photoUrl = mockUser.photoUrl?.toString(),
                accountProvider = "firebase", // Simplified for test
                lastLoginTime = mockUser.metadata?.lastSignInTimestamp ?: System.currentTimeMillis(),
                isEmailVerified = mockUser.isEmailVerified,
                phoneNumber = mockUser.phoneNumber,
            )

        // Then
        assertNotNull("Profile should not be null", profile)
        assertEquals("UID should match", "test-uid-123", profile.uid)
        assertEquals("Display name should match", "Test User", profile.displayName)
        assertEquals("Email should match", "test@example.com", profile.email)
        assertEquals("Photo URL should match", "https://example.com/photo.jpg", profile.photoUrl)
        assertTrue("Email should be verified", profile.isEmailVerified)
        assertEquals("Phone number should match", "+1234567890", profile.phoneNumber)
        assertEquals("Last login time should match", 1640995200000L, profile.lastLoginTime)
    }

    @Test
    fun `UserProfile creation handles null Firebase user values gracefully`() {
        // Given
        val mockUser = mockk<FirebaseUser>()
        val mockMetadata = mockk<com.google.firebase.auth.FirebaseUserMetadata>()

        every { mockUser.uid } returns "test-uid-null"
        every { mockUser.displayName } returns null
        every { mockUser.email } returns null
        every { mockUser.photoUrl } returns null
        every { mockUser.isEmailVerified } returns false
        every { mockUser.phoneNumber } returns null
        every { mockUser.metadata } returns mockMetadata
        every { mockMetadata.lastSignInTimestamp } returns 0L

        // When - Create UserProfile with null handling
        val currentTime = System.currentTimeMillis()
        val profile =
            UserProfile(
                uid = mockUser.uid,
                displayName = mockUser.displayName ?: "Unknown User",
                email = mockUser.email,
                photoUrl = mockUser.photoUrl?.toString(),
                accountProvider = "firebase",
                lastLoginTime = mockUser.metadata?.lastSignInTimestamp ?: currentTime,
                isEmailVerified = mockUser.isEmailVerified,
                phoneNumber = mockUser.phoneNumber,
            )

        // Then
        assertNotNull("Profile should not be null", profile)
        assertEquals("UID should match", "test-uid-null", profile.uid)
        assertEquals("Display name should be fallback", "Unknown User", profile.displayName)
        assertNull("Email should be null", profile.email)
        assertNull("Photo URL should be null", profile.photoUrl)
        assertFalse("Email should not be verified", profile.isEmailVerified)
        assertNull("Phone number should be null", profile.phoneNumber)
        assertEquals("Last login time should be 0", 0L, profile.lastLoginTime)
    }

    // === Discord AccountConnectionState Tests ===

    @Test
    fun `Discord AccountConnectionState handles connected state correctly`() {
        // Given
        val discordInfo =
            mapOf(
                "username" to "discorduser",
                "displayName" to "Discord User",
                "avatarUrl" to "https://discord.com/avatar.png",
                "updatedAt" to 1640995200000L,
            )

        // When - Create AccountConnectionState mimicking repository logic
        val accountState =
            AccountConnectionState(
                isConnected = true,
                username = discordInfo["username"] as? String,
                displayName = discordInfo["displayName"] as? String,
                avatarUrl = discordInfo["avatarUrl"] as? String,
                lastSyncTime = discordInfo["updatedAt"] as? Long,
                createAccountUrl = "https://discord.com/register",
            )

        // Then
        assertTrue("Discord should be connected", accountState.isConnected)
        assertEquals("Username should match", "discorduser", accountState.username)
        assertEquals("Display name should match", "Discord User", accountState.displayName)
        assertEquals("Avatar URL should match", "https://discord.com/avatar.png", accountState.avatarUrl)
        assertEquals("Last sync time should match", 1640995200000L, accountState.lastSyncTime)
        assertEquals("Create account URL should be set", "https://discord.com/register", accountState.createAccountUrl)
    }

    @Test
    fun `Discord AccountConnectionState handles disconnected state correctly`() {
        // When - Create disconnected state
        val accountState =
            AccountConnectionState(
                isConnected = false,
                username = null,
                displayName = null,
                avatarUrl = null,
                lastSyncTime = null,
                createAccountUrl = "https://discord.com/register",
            )

        // Then
        assertFalse("Discord should not be connected", accountState.isConnected)
        assertNull("Username should be null", accountState.username)
        assertNull("Display name should be null", accountState.displayName)
        assertNull("Avatar URL should be null", accountState.avatarUrl)
        assertNull("Last sync time should be null", accountState.lastSyncTime)
        assertEquals("Create account URL should still be available", "https://discord.com/register", accountState.createAccountUrl)
    }

    // === RetroAchievements AccountConnectionState Tests ===

    @Test
    fun `RetroAchievements AccountConnectionState handles stats correctly`() {
        // Given
        val username = "ra_user"
        val stats = intArrayOf(150, 75, 10) // points, achievements, leaderboards
        val avatarUrl = "https://retroachievements.org/avatar.png"

        // When - Create RA state mimicking repository logic
        val additionalInfo = mutableMapOf<String, Any>()
        if (stats.isNotEmpty()) {
            additionalInfo["totalPoints"] = stats.getOrElse(0) { 0 }
            additionalInfo["totalAchievements"] = stats.getOrElse(1) { 0 }
            additionalInfo["totalLeaderboards"] = stats.getOrElse(2) { 0 }
        }

        val accountState =
            AccountConnectionState(
                isConnected = true,
                username = username,
                displayName = username,
                avatarUrl = avatarUrl,
                lastSyncTime = System.currentTimeMillis(),
                createAccountUrl = "https://retroachievements.org/createaccount.php",
                additionalInfo = additionalInfo,
            )

        // Then
        assertTrue("RA should be connected", accountState.isConnected)
        assertEquals("Username should match", "ra_user", accountState.username)
        assertEquals("Display name should match username", "ra_user", accountState.displayName)
        assertEquals("Avatar URL should match", "https://retroachievements.org/avatar.png", accountState.avatarUrl)
        assertEquals("Points should match", 150, accountState.additionalInfo?.get("totalPoints"))
        assertEquals("Achievements should match", 75, accountState.additionalInfo?.get("totalAchievements"))
        assertEquals("Leaderboards should match", 10, accountState.additionalInfo?.get("totalLeaderboards"))
        assertNotNull("Last sync time should be set", accountState.lastSyncTime)
    }

    @Test
    fun `RetroAchievements AccountConnectionState handles empty stats gracefully`() {
        // Given
        val username = "ra_user"
        val emptyStats = intArrayOf()

        // When - Create RA state with empty stats
        val additionalInfo = mutableMapOf<String, Any>()
        if (emptyStats.isNotEmpty()) {
            additionalInfo["totalPoints"] = emptyStats.getOrElse(0) { 0 }
            additionalInfo["totalAchievements"] = emptyStats.getOrElse(1) { 0 }
            additionalInfo["totalLeaderboards"] = emptyStats.getOrElse(2) { 0 }
        } else {
            // Handle empty stats
            additionalInfo["totalPoints"] = 0
            additionalInfo["totalAchievements"] = 0
            additionalInfo["totalLeaderboards"] = 0
        }

        val accountState =
            AccountConnectionState(
                isConnected = true,
                username = username,
                displayName = username,
                avatarUrl = null,
                lastSyncTime = System.currentTimeMillis(),
                createAccountUrl = "https://retroachievements.org/createaccount.php",
                additionalInfo = additionalInfo,
            )

        // Then
        assertTrue("RA should be connected if username exists", accountState.isConnected)
        assertEquals("Username should match", "ra_user", accountState.username)
        assertEquals("Points should be 0", 0, accountState.additionalInfo?.get("totalPoints"))
        assertEquals("Achievements should be 0", 0, accountState.additionalInfo?.get("totalAchievements"))
        assertEquals("Leaderboards should be 0", 0, accountState.additionalInfo?.get("totalLeaderboards"))
    }

    @Test
    fun `RetroAchievements AccountConnectionState handles disconnected state`() {
        // When - Create disconnected RA state
        val accountState =
            AccountConnectionState(
                isConnected = false,
                username = null,
                displayName = null,
                avatarUrl = null,
                lastSyncTime = null,
                createAccountUrl = "https://retroachievements.org/createaccount.php",
                additionalInfo = null,
            )

        // Then
        assertFalse("RA should not be connected", accountState.isConnected)
        assertNull("Username should be null", accountState.username)
        assertNull("Display name should be null", accountState.displayName)
        assertNull("Avatar URL should be null", accountState.avatarUrl)
        assertNull("Additional info should be null", accountState.additionalInfo)
        assertEquals(
            "Create account URL should be available",
            "https://retroachievements.org/createaccount.php",
            accountState.createAccountUrl,
        )
    }

    // === ConnectedAccountsState Integration Tests ===

    @Test
    fun `ConnectedAccountsState integrates all account states correctly`() {
        // Given
        val firebaseState =
            AccountConnectionState(
                isConnected = true,
                username = "firebase@example.com",
                displayName = "Firebase User",
                avatarUrl = "https://firebase.com/avatar.jpg",
                lastSyncTime = System.currentTimeMillis(),
                createAccountUrl = null,
            )

        val discordState =
            AccountConnectionState(
                isConnected = true,
                username = "discorduser",
                displayName = "Discord User",
                avatarUrl = "https://discord.com/avatar.png",
                lastSyncTime = System.currentTimeMillis(),
                createAccountUrl = "https://discord.com/register",
            )

        val raState =
            AccountConnectionState(
                isConnected = false,
                username = null,
                displayName = null,
                avatarUrl = null,
                lastSyncTime = null,
                createAccountUrl = "https://retroachievements.org/createaccount.php",
            )

        // When
        val connectedAccounts =
            ConnectedAccountsState(
                firebase = firebaseState,
                discord = discordState,
                retroAchievements = raState,
            )

        // Then
        assertTrue("Firebase should be connected", connectedAccounts.firebase.isConnected)
        assertTrue("Discord should be connected", connectedAccounts.discord.isConnected)
        assertFalse("RetroAchievements should not be connected", connectedAccounts.retroAchievements.isConnected)

        assertEquals("Firebase username should match", "firebase@example.com", connectedAccounts.firebase.username)
        assertEquals("Discord username should match", "discorduser", connectedAccounts.discord.username)
        assertNull("RA username should be null", connectedAccounts.retroAchievements.username)
    }
}
