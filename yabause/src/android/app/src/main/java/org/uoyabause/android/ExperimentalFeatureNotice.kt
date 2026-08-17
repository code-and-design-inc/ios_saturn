package org.uoyabause.android

import android.content.Context
import android.content.DialogInterface
import android.content.SharedPreferences
import androidx.preference.PreferenceManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.devmiyax.yabasanshiro.R

/**
 * Shows a one-time informational dialog when the user enables an experimental
 * rendering feature (the VDP1 Compute Rasterizer). The dialog explains the feature
 * is experimental and teaches how to report bugs. It is shown at most once per
 * toggle key (persisted in SharedPreferences).
 */
object ExperimentalFeatureNotice {
    const val KEY_VDP1_COMPUTE_RASTERIZER = "pref_polygon_generation_compute_rasterizer"

    fun prefKey(toggleKey: String): String = "shown_experimental_notice_$toggleKey"

    fun shouldShow(prefs: SharedPreferences, toggleKey: String): Boolean =
        !prefs.getBoolean(prefKey(toggleKey), false)

    fun markShown(prefs: SharedPreferences, toggleKey: String) {
        prefs.edit().putBoolean(prefKey(toggleKey), true).apply()
    }

    /**
     * Shows the notice once for the given toggle if it was just turned on. No-op if
     * already shown for that toggle.
     */
    fun maybeShow(context: Context, toggleKey: String) {
        val prefs = PreferenceManager.getDefaultSharedPreferences(context)
        if (!shouldShow(prefs, toggleKey)) return

        val featureName =
            when (toggleKey) {
                KEY_VDP1_COMPUTE_RASTERIZER -> context.getString(R.string.poly_compute_rasterizer)
                else -> ""
            }

        val dialog =
            MaterialAlertDialogBuilder(context)
                .setTitle(R.string.experimental_notice_title)
                .setMessage(context.getString(R.string.experimental_notice_message, featureName))
                .setPositiveButton(R.string.experimental_notice_ok, null)
                .create()
        dialog.setOnShowListener {
            val button = dialog.getButton(DialogInterface.BUTTON_POSITIVE)
            button?.post {
                button.isFocusable = true
                button.isFocusableInTouchMode = true
                button.requestFocus()
            }
        }
        dialog.show()
        markShown(prefs, toggleKey)
    }
}
