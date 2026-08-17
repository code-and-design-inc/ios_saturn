package org.uoyabause.android.auth.ui

import android.app.Activity
import com.firebase.ui.auth.IdpResponse
import com.google.firebase.FirebaseApp
import com.google.firebase.auth.FirebaseUser
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.unmockkAll
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config
import org.uoyabause.android.TestYabauseApplication
import org.uoyabause.android.shadows.ShadowComplexColor
import org.uoyabause.android.shadows.ShadowResourcesImpl

/**
 * Test for Issue #78: Firebase Auth sign-in result handling.
 *
 * Verifies that the cancel message is NOT shown when:
 * - resultCode is RESULT_OK (login succeeded)
 * - but IdpResponse is null
 *
 * This was a bug where `pendingRALoginCallback` path only checked
 * `response == null` without checking `resultCode`, causing a
 * false "sign in cancelled" message on successful login.
 */
@RunWith(RobolectricTestRunner::class)
@Config(
    sdk = [28],
    application = TestYabauseApplication::class,
    instrumentedPackages = ["org.uoyabause"],
    shadows = [
        ShadowComplexColor::class,
        ShadowResourcesImpl::class,
    ],
)
class SignInResultHandlerTest {
    private lateinit var handler: SignInResultHandler
    private val mockFirebaseUser = mockk<FirebaseUser>(relaxed = true)

    @Before
    fun setup() {
        try {
            FirebaseApp.initializeApp(RuntimeEnvironment.getApplication())
        } catch (e: Exception) {
            // Firebase already initialized
        }

        // Mock IdpResponse.fromResultIntent to return null (simulating the bug scenario)
        mockkStatic(IdpResponse::class)
        every { IdpResponse.fromResultIntent(any()) } returns null

        every { mockFirebaseUser.displayName } returns "Test User"

        handler = SignInResultHandler()
    }

    @After
    fun tearDown() {
        unmockkAll()
    }

    // === Issue #78: pendingRALoginCallback path tests ===

    @Test
    fun `RA callback - RESULT_OK with null response and null user should return NoAction not Cancelled`() {
        // This is the CORE bug scenario from Issue #78:
        // Login succeeded (RESULT_OK), but response is null and user not yet available
        // BEFORE FIX: Cancelled would be returned (BUG - cancel message shown)
        // AFTER FIX: NoAction is returned (no cancel message)
        val result = handler.handleResult(
            resultCode = Activity.RESULT_OK,
            data = null,
            currentUser = null,
            hasPinCallback = false,
            hasRACallback = true,
        )

        assertTrue(
            "Should return NoAction, not Cancelled, when resultCode is RESULT_OK",
            result is SignInResultHandler.Result.NoAction,
        )
    }

    @Test
    fun `RA callback - RESULT_CANCELED with null response and null user should return Cancelled`() {
        // User actually cancelled - should show cancel message
        val result = handler.handleResult(
            resultCode = Activity.RESULT_CANCELED,
            data = null,
            currentUser = null,
            hasPinCallback = false,
            hasRACallback = true,
        )

        assertTrue(
            "Should return Cancelled when user actually cancelled",
            result is SignInResultHandler.Result.Cancelled,
        )
    }

    @Test
    fun `RA callback - RESULT_OK with valid user should return RALoginSuccess`() {
        // Login succeeded, user is available
        val result = handler.handleResult(
            resultCode = Activity.RESULT_OK,
            data = null,
            currentUser = mockFirebaseUser,
            hasPinCallback = false,
            hasRACallback = true,
        )

        assertTrue(
            "Should return RALoginSuccess when user is authenticated",
            result is SignInResultHandler.Result.RALoginSuccess,
        )
    }

    @Test
    fun `RA callback - RESULT_CANCELED with valid user should return RALoginSuccess`() {
        // Edge case: resultCode is CANCELED but user exists
        // currentUser check takes priority
        val result = handler.handleResult(
            resultCode = Activity.RESULT_CANCELED,
            data = null,
            currentUser = mockFirebaseUser,
            hasPinCallback = false,
            hasRACallback = true,
        )

        assertTrue(
            "Should return RALoginSuccess when user exists regardless of resultCode",
            result is SignInResultHandler.Result.RALoginSuccess,
        )
    }

    // === Normal sign-in flow tests ===

    @Test
    fun `normal flow - RESULT_OK with user should return SignInSuccess`() {
        val result = handler.handleResult(
            resultCode = Activity.RESULT_OK,
            data = null,
            currentUser = mockFirebaseUser,
            hasPinCallback = false,
            hasRACallback = false,
        )

        assertTrue(
            "Should return SignInSuccess",
            result is SignInResultHandler.Result.SignInSuccess,
        )
        assertEquals(
            "Test User",
            (result as SignInResultHandler.Result.SignInSuccess).displayName,
        )
    }

    @Test
    fun `normal flow - RESULT_CANCELED with null response should return Cancelled`() {
        val result = handler.handleResult(
            resultCode = Activity.RESULT_CANCELED,
            data = null,
            currentUser = null,
            hasPinCallback = false,
            hasRACallback = false,
        )

        assertTrue(
            "Should return Cancelled when user cancelled normal flow",
            result is SignInResultHandler.Result.Cancelled,
        )
    }

    @Test
    fun `normal flow - RESULT_CANCELED with error response should return SignInFailed`() {
        // Mock a response with an error
        val mockResponse = mockk<IdpResponse>(relaxed = true)
        val mockError = mockk<com.firebase.ui.auth.FirebaseUiException>(relaxed = true)
        every { IdpResponse.fromResultIntent(any()) } returns mockResponse
        every { mockResponse.error } returns mockError
        every { mockError.message } returns "Network error"

        val result = handler.handleResult(
            resultCode = Activity.RESULT_CANCELED,
            data = null,
            currentUser = null,
            hasPinCallback = false,
            hasRACallback = false,
        )

        assertTrue(
            "Should return SignInFailed with error",
            result is SignInResultHandler.Result.SignInFailed,
        )
        assertEquals(
            "Network error",
            (result as SignInResultHandler.Result.SignInFailed).errorMessage,
        )
    }

    // === PIN generation callback tests ===

    @Test
    fun `PIN callback - RESULT_OK with null response should return PinCallback with null`() {
        val result = handler.handleResult(
            resultCode = Activity.RESULT_OK,
            data = null,
            currentUser = mockFirebaseUser,
            hasPinCallback = true,
            hasRACallback = false,
        )

        assertTrue(
            "Should return PinCallback",
            result is SignInResultHandler.Result.PinCallback,
        )
        assertNull(
            "PinCallback should have null response (no IdpResponse)",
            (result as SignInResultHandler.Result.PinCallback).response,
        )
    }

    @Test
    fun `PIN callback - RESULT_CANCELED should return PinCallback with null`() {
        val result = handler.handleResult(
            resultCode = Activity.RESULT_CANCELED,
            data = null,
            currentUser = null,
            hasPinCallback = true,
            hasRACallback = false,
        )

        assertTrue(
            "Should return PinCallback",
            result is SignInResultHandler.Result.PinCallback,
        )
        assertNull(
            "PinCallback should have null response",
            (result as SignInResultHandler.Result.PinCallback).response,
        )
    }

    @Test
    fun `PIN callback - with valid response should return PinCallback with response`() {
        val mockResponse = mockk<IdpResponse>(relaxed = true)
        every { IdpResponse.fromResultIntent(any()) } returns mockResponse

        val result = handler.handleResult(
            resultCode = Activity.RESULT_OK,
            data = null,
            currentUser = mockFirebaseUser,
            hasPinCallback = true,
            hasRACallback = false,
        )

        assertTrue(
            "Should return PinCallback with response",
            result is SignInResultHandler.Result.PinCallback,
        )
        assertEquals(
            mockResponse,
            (result as SignInResultHandler.Result.PinCallback).response,
        )
    }

    // === Regression guard ===

    @Test
    fun `REGRESSION Issue78 - exact bug scenario must return NoAction not Cancelled`() {
        // Exact bug scenario:
        // 1. Firebase Auth UI login succeeds -> RESULT_OK
        // 2. IdpResponse.fromResultIntent returns null
        // 3. pendingRALoginCallback is set
        // 4. currentUser is null (timing issue)
        //
        // BEFORE FIX: condition was `response == null` -> Cancelled (BUG)
        // AFTER FIX: condition is `response == null && resultCode != RESULT_OK` -> NoAction

        val result = handler.handleResult(
            resultCode = Activity.RESULT_OK,
            data = null,
            currentUser = null,
            hasPinCallback = false,
            hasRACallback = true,
        )

        assertTrue(
            "REGRESSION: Must NOT return Cancelled when resultCode is RESULT_OK. " +
                "This was the bug in Issue #78 where cancel message was shown on successful login.",
            result is SignInResultHandler.Result.NoAction,
        )
    }
}
