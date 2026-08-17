package org.uoyabause.android

import android.app.Application
import android.util.Log

/**
 * Test application class that avoids native library loading
 */
class TestYabauseApplication : Application() {
    companion object {
        private const val TAG = "TestYabauseApplication"
    }

    override fun onCreate() {
        super.onCreate()
        Log.d(TAG, "Test application created - skipping native library initialization")

        // Skip all native library initialization for tests
        // This prevents UnsatisfiedLinkError during test execution
    }
}
