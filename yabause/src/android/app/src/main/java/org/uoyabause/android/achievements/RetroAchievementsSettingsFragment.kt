package org.uoyabause.android.achievements

import android.content.Context
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.Switch
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import com.google.firebase.auth.FirebaseAuth
import kotlinx.coroutines.launch
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.auth.RetroAchievementsAuthManager

/**
 * RetroAchievements Settings Fragment
 * Provides UI for configuring RetroAchievements options
 */
class RetroAchievementsSettingsFragment : Fragment() {
    companion object {
        private const val TAG = "RASettingsFragment"

        fun newInstance(): RetroAchievementsSettingsFragment = RetroAchievementsSettingsFragment()
    }

    private lateinit var authManager: RetroAchievementsAuthManager
    private lateinit var retroAchievementsManager: RetroAchievementsManager

    // UI Elements
    private lateinit var loginSection: LinearLayout
    private lateinit var loggedInSection: LinearLayout
    private lateinit var usernameEditText: EditText
    private lateinit var passwordEditText: EditText
    private lateinit var loginButton: Button
    private lateinit var logoutButton: Button
    private lateinit var statusTextView: TextView
    private lateinit var hardcoreModeSwitch: Switch
    private lateinit var autoLoginSwitch: Switch
    private lateinit var currentUserTextView: TextView

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View? = createSettingsView(requireContext())

    override fun onViewCreated(
        view: View,
        savedInstanceState: Bundle?,
    ) {
        super.onViewCreated(view, savedInstanceState)

        initializeManagers()
        setupEventListeners()
        updateUI()
    }

    /**
     * Create the settings view programmatically
     */
    private fun createSettingsView(context: Context): View {
        val scrollView =
            ScrollView(context).apply {
                layoutParams =
                    ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.MATCH_PARENT,
                    )
                setPadding(32, 32, 32, 32)
            }

        val mainLayout =
            LinearLayout(context).apply {
                orientation = LinearLayout.VERTICAL
                layoutParams =
                    ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                    )
            }

        // Title
        val titleText =
            TextView(context).apply {
                text = getString(R.string.retroachievements_settings_title)
                textSize = 24f
                setTypeface(null, android.graphics.Typeface.BOLD)
                setPadding(0, 0, 0, 32)
            }
        mainLayout.addView(titleText)

        // Status section
        statusTextView =
            TextView(context).apply {
                textSize = 16f
                setPadding(0, 0, 0, 16)
            }
        mainLayout.addView(statusTextView)

        // Current user section
        currentUserTextView =
            TextView(context).apply {
                textSize = 14f
                setPadding(0, 0, 0, 24)
            }
        mainLayout.addView(currentUserTextView)

        // Login section
        loginSection =
            LinearLayout(context).apply {
                orientation = LinearLayout.VERTICAL
                setPadding(0, 0, 0, 24)
            }

        val loginTitle =
            TextView(context).apply {
                text = getString(R.string.retroachievements_login_section)
                textSize = 18f
                setTypeface(null, android.graphics.Typeface.BOLD)
                setPadding(0, 0, 0, 16)
            }
        loginSection.addView(loginTitle)

        usernameEditText =
            EditText(context).apply {
                hint = getString(R.string.retroachievements_username_hint)
                inputType = android.text.InputType.TYPE_TEXT_VARIATION_PERSON_NAME
                setPadding(16, 16, 16, 16)
                setMargins(0, 0, 0, 16)
            }
        loginSection.addView(usernameEditText)

        passwordEditText =
            EditText(context).apply {
                hint = getString(R.string.retroachievements_password_hint)
                inputType = android.text.InputType.TYPE_CLASS_TEXT or android.text.InputType.TYPE_TEXT_VARIATION_PASSWORD
                setPadding(16, 16, 16, 16)
                setMargins(0, 0, 0, 16)
            }
        loginSection.addView(passwordEditText)

        loginButton =
            Button(context).apply {
                text = getString(R.string.retroachievements_login)
                setPadding(16, 16, 16, 16)
            }
        loginSection.addView(loginButton)

        mainLayout.addView(loginSection)

        // Logged in section
        loggedInSection =
            LinearLayout(context).apply {
                orientation = LinearLayout.VERTICAL
                setPadding(0, 0, 0, 24)
                visibility = View.GONE
            }

        logoutButton =
            Button(context).apply {
                text = getString(R.string.retroachievements_logout)
                setPadding(16, 16, 16, 16)
            }
        loggedInSection.addView(logoutButton)

        mainLayout.addView(loggedInSection)

        // Settings section
        val settingsTitle =
            TextView(context).apply {
                text = getString(R.string.retroachievements_settings_section)
                textSize = 18f
                setTypeface(null, android.graphics.Typeface.BOLD)
                setPadding(0, 24, 0, 16)
            }
        mainLayout.addView(settingsTitle)

        // Hardcore mode switch
        val hardcoreLayout =
            LinearLayout(context).apply {
                orientation = LinearLayout.HORIZONTAL
                setPadding(0, 8, 0, 8)
            }

        val hardcoreLabel =
            TextView(context).apply {
                text = getString(R.string.retroachievements_hardcore_mode)
                textSize = 16f
                layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
            }
        hardcoreLayout.addView(hardcoreLabel)

        hardcoreModeSwitch = Switch(context)
        hardcoreLayout.addView(hardcoreModeSwitch)

        mainLayout.addView(hardcoreLayout)

        // Hardcore mode description
        val hardcoreDesc =
            TextView(context).apply {
                text = getString(R.string.retroachievements_hardcore_description)
                textSize = 12f
                setTextColor(android.graphics.Color.GRAY)
                setPadding(0, 0, 0, 16)
            }
        mainLayout.addView(hardcoreDesc)

        // Auto-login switch
        val autoLoginLayout =
            LinearLayout(context).apply {
                orientation = LinearLayout.HORIZONTAL
                setPadding(0, 8, 0, 8)
            }

        val autoLoginLabel =
            TextView(context).apply {
                text = getString(R.string.retroachievements_auto_login)
                textSize = 16f
                layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
            }
        autoLoginLayout.addView(autoLoginLabel)

        autoLoginSwitch = Switch(context)
        autoLoginLayout.addView(autoLoginSwitch)

        mainLayout.addView(autoLoginLayout)

        // Auto-login description
        val autoLoginDesc =
            TextView(context).apply {
                text = getString(R.string.retroachievements_auto_login_description)
                textSize = 12f
                setTextColor(android.graphics.Color.GRAY)
                setPadding(0, 0, 0, 16)
            }
        mainLayout.addView(autoLoginDesc)

        scrollView.addView(mainLayout)
        return scrollView
    }

    /**
     * Helper extension for setting margins
     */
    private fun View.setMargins(
        left: Int,
        top: Int,
        right: Int,
        bottom: Int,
    ) {
        if (layoutParams is ViewGroup.MarginLayoutParams) {
            val params = layoutParams as ViewGroup.MarginLayoutParams
            params.setMargins(left, top, right, bottom)
            requestLayout()
        }
    }

    /**
     * Initialize managers
     */
    private fun initializeManagers() {
        authManager = RetroAchievementsAuthManager.getInstance(requireContext())
        retroAchievementsManager = RetroAchievementsManager.getInstance(requireContext())

        // Setup auth state listener
        authManager.onAuthStateChanged = { isLoggedIn, username ->
            activity?.runOnUiThread {
                updateUI()
            }
        }
    }

    /**
     * Setup event listeners
     */
    private fun setupEventListeners() {
        loginButton.setOnClickListener {
            performLogin()
        }

        logoutButton.setOnClickListener {
            performLogout()
        }

        hardcoreModeSwitch.setOnCheckedChangeListener { _, isChecked ->
            RetroAchievementsManager.getInstance(requireContext()).setHardcoreEnabled(isChecked)
        }

        autoLoginSwitch.setOnCheckedChangeListener { _, isChecked ->
            authManager.setAutoLoginEnabled(isChecked)
        }
    }

    /**
     * Perform login
     */
    private fun performLogin() {
        // Check if Firebase user is logged in first
        if (FirebaseAuth.getInstance().currentUser == null) {
            showFirebaseLoginRequiredDialog()
            return
        }

        val username = usernameEditText.text.toString().trim()
        val password = passwordEditText.text.toString().trim()

        if (username.isEmpty() || password.isEmpty()) {
            Toast.makeText(requireContext(), getString(R.string.retroachievements_login_required), Toast.LENGTH_SHORT).show()
            return
        }

        // Disable login button during login attempt
        loginButton.isEnabled = false
        loginButton.text = getString(R.string.retroachievements_logging_in)

        authManager.onLoginResult = { success, error ->
            activity?.runOnUiThread {
                loginButton.isEnabled = true
                loginButton.text = getString(R.string.retroachievements_login)

                if (success) {
                    Toast.makeText(requireContext(), getString(R.string.retroachievements_login_success), Toast.LENGTH_SHORT).show()
                    // Clear password field for security
                    passwordEditText.text.clear()
                } else {
                    Toast
                        .makeText(
                            requireContext(),
                            getString(R.string.retroachievements_login_failed, error ?: "Unknown error"),
                            Toast.LENGTH_LONG,
                        ).show()
                }
            }
        }

        authManager.loginRetroAchievements(username, password, autoLoginSwitch.isChecked)
    }

    /**
     * Show dialog prompting user to sign in to Firebase before using RetroAchievements
     */
    private fun showFirebaseLoginRequiredDialog() {
        AlertDialog
            .Builder(requireContext())
            .setTitle(getString(R.string.firebase_login_required_title))
            .setMessage(getString(R.string.firebase_login_required_for_ra_message))
            .setPositiveButton(getString(R.string.ok)) { dialog, _ -> dialog.dismiss() }
            .show()
    }

    /**
     * Perform logout
     */
    private fun performLogout() {
        authManager.logoutRetroAchievements()
        Toast.makeText(requireContext(), getString(R.string.retroachievements_logout_success), Toast.LENGTH_SHORT).show()
    }

    /**
     * Update UI based on current state
     */
    private fun updateUI() {
        lifecycleScope.launch {
            val authStatus = authManager.getAuthenticationStatus()

            // Update status text
            statusTextView.text =
                when {
                    authStatus.isRetroAchievementsLoggedIn -> getString(R.string.retroachievements_connected)
                    else -> getString(R.string.retroachievements_not_connected)
                }

            // Update current user text
            currentUserTextView.text =
                if (authStatus.isRetroAchievementsLoggedIn) {
                    getString(R.string.retroachievements_logged_in_as, authStatus.retroAchievementsUsername)
                } else {
                    ""
                }

            // Show/hide login sections
            if (authStatus.isRetroAchievementsLoggedIn) {
                loginSection.visibility = View.GONE
                loggedInSection.visibility = View.VISIBLE
            } else {
                loginSection.visibility = View.VISIBLE
                loggedInSection.visibility = View.GONE
            }

            // Update switch states
            hardcoreModeSwitch.isChecked = RetroAchievementsManager.getInstance(requireContext()).isHardcoreEnabled()
            autoLoginSwitch.isChecked = authManager.isAutoLoginEnabled()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        authManager.onAuthStateChanged = null
        authManager.onLoginResult = null
    }
}
