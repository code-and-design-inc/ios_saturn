package org.uoyabause.android.leaderboard

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat

class LeaderBoardActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Placeholder activity - layout implementation pending
        // Currently just displays title
        title = "リーダーボード"

        // Setup edge-to-edge window insets handling
        setupEdgeToEdgeInsets()
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
}
