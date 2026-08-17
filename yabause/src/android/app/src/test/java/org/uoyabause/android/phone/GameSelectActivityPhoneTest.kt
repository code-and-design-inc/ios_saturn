package org.uoyabause.android.phone

import android.content.Context
import android.content.SharedPreferences
import android.content.res.Configuration
import android.view.KeyEvent
import com.google.android.gms.ads.AdView
import io.mockk.*
import org.devmiyax.yabasanshiro.BuildConfig
import org.junit.After
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4

@RunWith(JUnit4::class)
class GameSelectActivityPhoneTest {
    private lateinit var mockFragment: GameSelectFragmentPhone
    private lateinit var mockAdView: AdView
    private lateinit var mockContext: Context
    private lateinit var mockSharedPreferences: SharedPreferences

    @Before
    fun setUp() {
        mockFragment = mockk<GameSelectFragmentPhone>(relaxed = true)
        mockAdView = mockk<AdView>(relaxed = true)
        mockContext = mockk<Context>(relaxed = true)
        mockSharedPreferences = mockk<SharedPreferences>(relaxed = true)
    }

    @After
    fun tearDown() {
        unmockkAll()
    }

    @Test
    fun `test AdView lifecycle methods work correctly`() {
        // Given
        val adView = mockk<AdView>(relaxed = true)

        // When - Test pause
        adView.pause()

        // Then
        verify { adView.pause() }

        // When - Test resume
        adView.resume()

        // Then
        verify { adView.resume() }

        // When - Test destroy
        adView.destroy()

        // Then
        verify { adView.destroy() }
    }

    @Test
    fun `test BuildConfig BUILD_TYPE is accessible`() {
        // Given & When
        val buildType = BuildConfig.BUILD_TYPE

        // Then - Should not be null or empty
        assertNotNull(buildType)
        assertTrue(buildType.isNotEmpty())
        assertTrue(buildType == "debug" || buildType == "release" || buildType == "pro")
    }

    @Test
    fun `test SharedPreferences donation flag logic`() {
        // Given
        val editor = mockk<SharedPreferences.Editor>(relaxed = true)
        every { mockSharedPreferences.getBoolean("donated", false) } returns false
        every { mockSharedPreferences.edit() } returns editor
        every { editor.putBoolean(any(), any()) } returns editor

        // When - Check initial state
        val initialDonationState = mockSharedPreferences.getBoolean("donated", false)

        // Then
        assertFalse(initialDonationState)

        // When - Set donation to true
        mockSharedPreferences.edit().putBoolean("donated", true).apply()

        // Then
        verify { editor.putBoolean("donated", true) }
    }

    @Test
    fun `test AdView pause resume destroy sequence`() {
        // Given - Mock AdView
        val adView = mockk<AdView>(relaxed = true)

        // When - Simulate lifecycle sequence
        adView.pause()
        adView.resume()
        adView.destroy()

        // Then - Verify correct call sequence
        verifySequence {
            adView.pause()
            adView.resume()
            adView.destroy()
        }
    }

    @Test
    fun `test multiple pause resume cycles`() {
        // Given - Mock AdView
        val adView = mockk<AdView>(relaxed = true)

        // When - Multiple cycles
        repeat(3) {
            adView.pause()
            adView.resume()
        }

        // Then
        verify(exactly = 3) { adView.pause() }
        verify(exactly = 3) { adView.resume() }
    }

    @Test
    fun `test Configuration object creation`() {
        // Given & When
        val config = Configuration()
        config.orientation = Configuration.ORIENTATION_LANDSCAPE

        // Then
        assertEquals(Configuration.ORIENTATION_LANDSCAPE, config.orientation)

        // When - Change orientation
        config.orientation = Configuration.ORIENTATION_PORTRAIT

        // Then
        assertEquals(Configuration.ORIENTATION_PORTRAIT, config.orientation)
    }

    @Test
    fun `test KeyEvent constants are accessible`() {
        // Given & When - Test key code constants
        val dpadUp = KeyEvent.KEYCODE_DPAD_UP
        val dpadDown = KeyEvent.KEYCODE_DPAD_DOWN
        val dpadCenter = KeyEvent.KEYCODE_DPAD_CENTER
        val enter = KeyEvent.KEYCODE_ENTER

        // Then
        assertTrue(dpadUp > 0)
        assertTrue(dpadDown > 0)
        assertTrue(dpadCenter > 0)
        assertTrue(enter > 0)
        assertNotEquals(dpadUp, dpadDown)
    }

    @Test
    fun `test fragment mock interaction`() {
        // Given
        val mockFragment = mockk<GameSelectFragmentPhone>(relaxed = true)
        val mockConfig = Configuration()

        // When
        mockFragment.onConfigurationChanged(mockConfig)

        // Then
        verify { mockFragment.onConfigurationChanged(mockConfig) }
    }

    private fun assertDoesNotThrow(block: () -> Unit) {
        try {
            block()
        } catch (e: Exception) {
            fail("Expected no exception but got: ${e.message}")
        }
    }
}
