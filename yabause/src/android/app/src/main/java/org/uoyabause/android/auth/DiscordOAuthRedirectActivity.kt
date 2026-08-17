package org.uoyabause.android.auth

import android.content.Intent
import android.os.Bundle
import android.util.Log
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.launch
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.phone.GameSelectActivityPhone

/**
 * Activity to handle Discord OAuth2 redirects
 * This activity receives the redirect from Discord after authorization
 */
class DiscordOAuthRedirectActivity : AppCompatActivity() {
    private lateinit var firebaseAuthManager: FirebaseAuthManager

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_discord_oauth_redirect)

        // Setup edge-to-edge window insets handling
        setupEdgeToEdgeInsets()

        firebaseAuthManager = FirebaseAuthManager(this)

        // Process the intent that launched this activity
        handleIntent(intent)
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        handleIntent(intent)
    }

    /**
     * Handle the intent containing the OAuth redirect data
     */
    private fun handleIntent(intent: Intent) {
        val data = intent.data
        if (data == null) {
            Log.e(TAG, "No data in intent")
            showError(getString(R.string.discord_link_error))
            navigateBackToOriginalActivity()
            return
        }

        Log.d(TAG, "Received OAuth redirect: $data")

        lifecycleScope.launch {
            try {
                val success = firebaseAuthManager.handleDiscordRedirect(data)

                if (success) {
                    Toast
                        .makeText(
                            this@DiscordOAuthRedirectActivity,
                            R.string.discord_account_linked,
                            Toast.LENGTH_SHORT,
                        ).show()
                } else {
                    Toast
                        .makeText(
                            this@DiscordOAuthRedirectActivity,
                            R.string.discord_account_link_failed,
                            Toast.LENGTH_SHORT,
                        ).show()
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error handling Discord OAuth redirect", e)
                showError("${getString(R.string.discord_link_error)}: ${e.message}")
            } finally {
                navigateBackToOriginalActivity()
            }
        }
    }

    /**
     * Navigate back to the original activity that started the Discord authentication flow
     */
    private fun navigateBackToOriginalActivity() {
        // Create an intent to return to GameSelectActivityPhone
        val intent = Intent(this, GameSelectActivityPhone::class.java)
        // Clear the task and start fresh to avoid stacking activities
        intent.flags = Intent.FLAG_ACTIVITY_CLEAR_TOP
        startActivity(intent)
        // Finish this activity
        finish()
    }

    /**
     * Show an error message to the user
     */
    private fun showError(message: String) {
        Toast.makeText(this, message, Toast.LENGTH_LONG).show()
    }

    /**
     * Setup edge-to-edge window insets handling for Android 15+ (API 35+)
     */
    private fun setupEdgeToEdgeInsets() {
        val rootView = findViewById<android.view.View>(android.R.id.content)
        ViewCompat.setOnApplyWindowInsetsListener(rootView) { view, windowInsets ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())

            view.setPadding(
                view.paddingLeft,
                insets.top,
                view.paddingRight,
                view.paddingBottom,
            )

            windowInsets
        }
    }

    companion object {
        private const val TAG = "DiscordOAuthRedirect"
    }
}
