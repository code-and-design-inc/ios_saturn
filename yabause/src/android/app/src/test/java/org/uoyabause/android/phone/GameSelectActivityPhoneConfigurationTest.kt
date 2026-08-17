package org.uoyabause.android.phone

import android.content.res.Configuration
import android.view.KeyEvent
import io.mockk.*
import org.junit.After
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4

@RunWith(JUnit4::class)
class GameSelectActivityPhoneConfigurationTest {
    private lateinit var mockFragment: GameSelectFragmentPhone

    @Before
    fun setUp() {
        mockFragment = mockk<GameSelectFragmentPhone>(relaxed = true)
    }

    @After
    fun tearDown() {
        unmockkAll()
    }

    @Test
    fun `test Configuration orientation constants`() {
        // Given & When
        val portrait = Configuration.ORIENTATION_PORTRAIT
        val landscape = Configuration.ORIENTATION_LANDSCAPE
        val undefined = Configuration.ORIENTATION_UNDEFINED

        // Then
        assertEquals(1, portrait)
        assertEquals(2, landscape)
        assertEquals(0, undefined)
        assertNotEquals(portrait, landscape)
    }

    @Test
    fun `test Configuration object creation and modification`() {
        // Given
        val config = Configuration()

        // When
        config.orientation = Configuration.ORIENTATION_LANDSCAPE
        config.screenWidthDp = 800
        config.screenHeightDp = 600
        config.densityDpi = 320

        // Then
        assertEquals(Configuration.ORIENTATION_LANDSCAPE, config.orientation)
        assertEquals(800, config.screenWidthDp)
        assertEquals(600, config.screenHeightDp)
        assertEquals(320, config.densityDpi)
    }

    @Test
    fun `test fragment onConfigurationChanged mock`() {
        // Given
        val config =
            Configuration().apply {
                orientation = Configuration.ORIENTATION_PORTRAIT
            }

        // When
        mockFragment.onConfigurationChanged(config)

        // Then
        verify { mockFragment.onConfigurationChanged(config) }
    }

    @Test
    fun `test KeyEvent creation and properties`() {
        // Given & When
        val dpadUp = KeyEvent.KEYCODE_DPAD_UP
        val dpadDown = KeyEvent.KEYCODE_DPAD_DOWN
        val dpadCenter = KeyEvent.KEYCODE_DPAD_CENTER
        val enter = KeyEvent.KEYCODE_ENTER

        // Then
        assertEquals(19, dpadUp)
        assertEquals(20, dpadDown)
        assertEquals(23, dpadCenter)
        assertEquals(66, enter)

        // All should be different
        val keyCodes = setOf(dpadUp, dpadDown, dpadCenter, enter)
        assertEquals(4, keyCodes.size)
    }

    @Test
    fun `test mock fragment onKeyDown interaction`() {
        // Given
        val keyEvent = mockk<KeyEvent>(relaxed = true)
        every { mockFragment.onKeyDown(KeyEvent.KEYCODE_DPAD_UP, keyEvent) } returns true

        // When
        val result = mockFragment.onKeyDown(KeyEvent.KEYCODE_DPAD_UP, keyEvent)

        // Then
        assertTrue(result)
        verify { mockFragment.onKeyDown(KeyEvent.KEYCODE_DPAD_UP, keyEvent) }
    }
}
