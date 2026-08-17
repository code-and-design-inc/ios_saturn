package org.uoyabause.yabasanshiro

import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.auth.FirebaseUser
import com.google.firebase.firestore.CollectionReference
import com.google.firebase.firestore.DocumentReference
import com.google.firebase.firestore.FirebaseFirestore
import io.mockk.*
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.uoyabause.android.game.SonicR

class SonicRTest {
    private lateinit var firestoreMock: FirebaseFirestore
    private lateinit var authMock: FirebaseAuth
    private lateinit var userMock: FirebaseUser
    private lateinit var docRefMock: DocumentReference
    private lateinit var colRefMock: CollectionReference
    private lateinit var sonicR: SonicR

    @Before
    fun setUp() {
        firestoreMock = mockk(relaxed = true)
        authMock = mockk(relaxed = true)
        userMock = mockk(relaxed = true)
        docRefMock = mockk(relaxed = true)
        colRefMock = mockk(relaxed = true)

        mockkStatic(FirebaseFirestore::class)
        mockkStatic(FirebaseAuth::class)

        every { FirebaseFirestore.getInstance() } returns firestoreMock
        every { FirebaseAuth.getInstance() } returns authMock
        every { authMock.currentUser } returns userMock
        every { userMock.uid } returns "test_uid"
        every { userMock.displayName } returns "テストユーザー"
        every { userMock.photoUrl } returns android.net.Uri.parse("https://example.com/test_avatar.png")
        every { firestoreMock.collection("games") } returns colRefMock
        every { colRefMock.document(any()) } returns docRefMock
        every { docRefMock.collection("leaderboards") } returns colRefMock
        every { colRefMock.document(any()) } returns docRefMock
        every { docRefMock.collection("scores") } returns colRefMock
        every { docRefMock.set(any()) } returns mockk(relaxed = true)
        every { docRefMock.get() } returns mockk(relaxed = true)

        sonicR = SonicR("SR")
    }

    @After
    fun tearDown() {
        unmockkAll()
    }

    @Test
    fun testSonicRInitialization() {
        // Test that SonicR initializes with correct leaderboards
        assertNotNull(sonicR.leaderBoards)
        assertEquals(5, sonicR.leaderBoards?.size)

        // Check leaderboard names
        val expectedNames = listOf("Resort Island", "Radical City", "Regal Ruin", "Reactive Factory", "Radiant Emerald")
        val actualNames = sonicR.leaderBoards?.map { it.title }
        assertEquals(expectedNames, actualNames)
    }

    @Test
    fun testLeaderboardIds() {
        // Test that leaderboard IDs are correct
        val expectedIds = listOf("01", "02", "03", "04", "05")
        val actualIds = sonicR.leaderBoards?.map { it.id }
        assertEquals(expectedIds, actualIds)
    }

    @Test
    fun testBackupUpdateLogic() {
        // Test the backup update functionality
        val mockUiEvent = mockk<org.uoyabause.android.game.GameUiEvent>(relaxed = true)
        sonicR.setUiEvent(mockUiEvent)

        // Create test backup data
        val beforeData = ByteArray(256) // Empty backup
        val afterData = ByteArray(256)

        // Set some test score data (this would need to match the actual SonicR backup format)
        // This is a simplified test - in reality you'd need to understand the backup format

        assertDoesNotThrow {
            sonicR.onBackUpUpdated("sonic_backup.dat", beforeData, afterData)
        }
    }

    private fun assertNotNull(value: Any?) {
        if (value == null) {
            throw AssertionError("Expected non-null value but was null")
        }
    }

    private fun assertEquals(
        expected: Any?,
        actual: Any?,
    ) {
        if (expected != actual) {
            throw AssertionError("Expected $expected but was $actual")
        }
    }

    private fun assertDoesNotThrow(block: () -> Unit) {
        try {
            block()
        } catch (e: Exception) {
            throw AssertionError("Expected no exception but got: ${e.message}")
        }
    }
}
