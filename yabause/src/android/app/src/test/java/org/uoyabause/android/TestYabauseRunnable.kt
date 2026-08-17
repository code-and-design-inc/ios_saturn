package org.uoyabause.android

/**
 * Test stub for YabauseRunnable to avoid native library dependencies in tests
 */
class TestYabauseRunnable {
    companion object {
        @JvmStatic
        fun initEmulation(
            argv: String?,
            openslES: Boolean,
        ): Int {
            // Return success for testing
            return 0
        }

        @JvmStatic
        fun deinitEmulation(): Int = 0

        @JvmStatic
        fun pause(): Int = 0

        @JvmStatic
        fun resume(): Int = 0

        @JvmStatic
        fun reset(): Int = 0

        @JvmStatic
        fun step(): Int = 0

        @JvmStatic
        fun getCurrentGameCode(): String = "TEST_GAME"

        @JvmStatic
        fun getGameinfo(filepath: String?): String = "{\"game_title\":\"Test Game\",\"product_number\":\"TEST-001\"}"

        @JvmStatic
        fun screenshot(filename: String): Int {
            // For testing, create a dummy file if it doesn't exist
            return 0 // Success
        }

        @JvmStatic
        fun savestate(path: String): String? {
            // For testing, return a path to a dummy save state
            return "$path/state.yss"
        }

        @JvmStatic
        fun savestate_compress(path: String): String? {
            // For testing, return a path to a dummy compressed save state
            return "$path/state_compressed.yss"
        }

        // Add other native methods as needed for testing
    }
}
