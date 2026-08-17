package org.uoyabause.android.auth

import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.uoyabause.android.TestYabauseApplication

/**
 * Simple test to verify test environment setup
 */
@RunWith(RobolectricTestRunner::class)
@Config(
    sdk = [28],
    application = TestYabauseApplication::class,
)
class SimpleTest {
    @Test
    fun `basic assertion test`() {
        assertEquals(2 + 2, 4)
        assertTrue("Basic true assertion", true)
        assertFalse("Basic false assertion", false)
    }

    @Test
    fun `string comparison test`() {
        val expected = "Hello, World!"
        val actual = "Hello, " + "World!"
        assertEquals(expected, actual)
    }

    @Test
    fun `null checking test`() {
        val nullValue: String? = null
        val nonNullValue: String? = "not null"

        assertNull("Value should be null", nullValue)
        assertNotNull("Value should not be null", nonNullValue)
    }
}
