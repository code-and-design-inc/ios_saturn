package org.uoyabause.android.auth

import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.auth.FirebaseUser
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.unmockkStatic
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

class AuthStateTest {
    private val authMock = mockk<FirebaseAuth>()

    @Before
    fun setUp() {
        mockkStatic(FirebaseAuth::class)
        every { FirebaseAuth.getInstance() } returns authMock
    }

    @After
    fun tearDown() {
        unmockkStatic(FirebaseAuth::class)
    }

    @Test
    fun `realUser is null when no user signed in`() {
        every { authMock.currentUser } returns null
        assertNull(AuthState.realUser())
        assertFalse(AuthState.isSignedIn())
    }

    @Test
    fun `realUser is null when user is anonymous`() {
        val user = mockk<FirebaseUser>()
        every { user.isAnonymous } returns true
        every { authMock.currentUser } returns user
        assertNull(AuthState.realUser())
        assertFalse(AuthState.isSignedIn())
    }

    @Test
    fun `realUser returns user when signed in for real`() {
        val user = mockk<FirebaseUser>()
        every { user.isAnonymous } returns false
        every { authMock.currentUser } returns user
        assertEquals(user, AuthState.realUser())
        assertTrue(AuthState.isSignedIn())
    }

    @Test
    fun `reportUser returns anonymous user too`() {
        val user = mockk<FirebaseUser>()
        every { user.isAnonymous } returns true
        every { authMock.currentUser } returns user
        assertEquals(user, AuthState.reportUser())
    }
}
