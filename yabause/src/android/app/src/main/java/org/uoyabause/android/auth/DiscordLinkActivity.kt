package org.uoyabause.android.auth

import android.app.UiModeManager
import android.content.Context
import android.content.res.Configuration
import android.os.Bundle
import android.view.View
import android.widget.Button
import android.widget.ImageView
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.lifecycle.lifecycleScope
import com.bumptech.glide.Glide
import com.google.firebase.auth.FirebaseUser
import kotlinx.coroutines.launch
import org.devmiyax.yabasanshiro.R

/**
 * Activity that demonstrates Discord account linking
 * This can be used as a reference for implementing Discord linking in other activities
 */
class DiscordLinkActivity :
    AppCompatActivity(),
    DiscordLinkPromptDialog.DiscordLinkPromptListener {
    private lateinit var firebaseAuthManager: FirebaseAuthManager

    // UI elements
    private lateinit var imgUserAvatar: ImageView
    private lateinit var tvUserName: TextView
    private lateinit var tvAccountStatus: TextView
    private lateinit var tvDiscordStatus: TextView
    private lateinit var btnLinkDiscord: Button
    private lateinit var btnUnlinkDiscord: Button
    private lateinit var tvStatusMessage: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_discord_link)

        // Setup edge-to-edge window insets handling
        setupEdgeToEdgeInsets()

        // Set up action bar
        supportActionBar?.setDisplayHomeAsUpEnabled(true)
        supportActionBar?.title = getString(R.string.link_discord_account)

        // Initialize Firebase Auth Manager
        firebaseAuthManager = FirebaseAuthManager(this)

        // Initialize UI elements
        initializeViews()

        // Set up button click listeners
        setupClickListeners()

        // Update UI with current user info
        updateUserInfo()
    }

    override fun onResume() {
        super.onResume()
        // Refresh UI when activity resumes (in case Discord linking happened)
        updateUserInfo()
    }

    override fun onSupportNavigateUp(): Boolean {
        onBackPressedDispatcher.onBackPressed()
        return true
    }

    /**
     * Initialize view references
     */
    private fun initializeViews() {
        imgUserAvatar = findViewById(R.id.img_user_avatar)
        tvUserName = findViewById(R.id.tv_user_name)
        tvAccountStatus = findViewById(R.id.tv_account_status)
        tvDiscordStatus = findViewById(R.id.tv_discord_status)
        btnLinkDiscord = findViewById(R.id.btn_link_discord)
        btnUnlinkDiscord = findViewById(R.id.btn_unlink_discord)
        tvStatusMessage = findViewById(R.id.tv_status_message)
    }

    /**
     * Set up button click listeners
     */
    private fun setupClickListeners() {
        btnLinkDiscord.setOnClickListener {
            if (isAndroidTv(this)) {
                // Show message on Android TV
                Toast.makeText(this, getString(R.string.discord_link_not_available_on_tv), Toast.LENGTH_LONG).show()
                return@setOnClickListener
            }
            firebaseAuthManager.startDiscordLinking(this)
            tvStatusMessage.text = getString(R.string.discord_link_prompt_message)
        }

        btnUnlinkDiscord.setOnClickListener {
            showDiscordUnlinkConfirmation()
        }
    }

    /**
     * Update UI with current user information
     */
    private fun updateUserInfo() {
        val currentUser = firebaseAuthManager.getCurrentUser()

        if (currentUser == null) {
            // No user is signed in, show error and finish activity
            Toast.makeText(this, "You must be signed in to link Discord", Toast.LENGTH_SHORT).show()
            finish()
            return
        }

        // Update user profile info
        updateUserProfile(currentUser)

        // Check Discord link status
        lifecycleScope.launch {
            val isDiscordLinked = firebaseAuthManager.isDiscordLinked()
            updateDiscordLinkStatus(isDiscordLinked)
        }
    }

    /**
     * Update user profile information in the UI
     */
    private fun updateUserProfile(user: FirebaseUser) {
        // Set user name
        tvUserName.text = user.displayName ?: "Anonymous User"

        // Set account status
        val provider = user.providerData.firstOrNull()?.providerId ?: "unknown"
        tvAccountStatus.text = "Firebase Account: $provider"

        // Load user avatar
        val photoUrl = user.photoUrl
        if (photoUrl != null) {
            Glide
                .with(this)
                .load(photoUrl)
                .circleCrop()
                .placeholder(R.drawable.ic_discord_logo)
                .into(imgUserAvatar)
        } else {
            // Use default avatar
            imgUserAvatar.setImageResource(R.drawable.ic_discord_logo)
        }
    }

    /**
     * Update Discord link status in the UI
     */
    private fun updateDiscordLinkStatus(isLinked: Boolean) {
        if (isLinked) {
            tvDiscordStatus.text = "Discord: Linked"
            btnLinkDiscord.visibility = View.GONE
            btnUnlinkDiscord.visibility = View.VISIBLE
            tvStatusMessage.text = getString(R.string.discord_already_linked)
        } else {
            tvDiscordStatus.text = "Discord: Not Linked"
            btnLinkDiscord.visibility = View.VISIBLE
            btnUnlinkDiscord.visibility = View.GONE
            tvStatusMessage.text = getString(R.string.discord_link_description)
        }
    }

    /**
     * Show confirmation dialog for unlinking Discord account
     */
    private fun showDiscordUnlinkConfirmation() {
        // Create and show a confirmation dialog
        androidx.appcompat.app.AlertDialog
            .Builder(this)
            .setTitle(R.string.discord_unlink_account)
            .setMessage(R.string.discord_unlink_confirm)
            .setPositiveButton(android.R.string.ok) { _, _ ->
                unlinkDiscordAccount()
            }.setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    /**
     * Unlink Discord account
     */
    private fun unlinkDiscordAccount() {
        lifecycleScope.launch {
            val success = firebaseAuthManager.unlinkDiscord()
            if (success) {
                Toast
                    .makeText(
                        this@DiscordLinkActivity,
                        R.string.discord_account_unlinked,
                        Toast.LENGTH_SHORT,
                    ).show()
                updateUserInfo() // Refresh UI
            } else {
                Toast
                    .makeText(
                        this@DiscordLinkActivity,
                        R.string.discord_link_error,
                        Toast.LENGTH_SHORT,
                    ).show()
            }
        }
    }

    /**
     * Called when the user accepts to link their Discord account
     */
    override fun onDiscordLinkAccepted() {
        if (isAndroidTv(this)) {
            // Show message on Android TV and do nothing else
            Toast.makeText(this, getString(R.string.discord_link_not_available_on_tv), Toast.LENGTH_LONG).show()
            return
        }
        firebaseAuthManager.startDiscordLinking(this)
    }

    /**
     * Called when the user declines to link their Discord account
     */
    override fun onDiscordLinkDeclined() {
        tvStatusMessage.text = getString(R.string.discord_link_cancelled)
    }

    /**
     * Checks if the current device is an Android TV.
     */
    private fun isAndroidTv(context: Context): Boolean {
        val uiModeManager = context.getSystemService(Context.UI_MODE_SERVICE) as UiModeManager
        return uiModeManager.currentModeType == Configuration.UI_MODE_TYPE_TELEVISION
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
