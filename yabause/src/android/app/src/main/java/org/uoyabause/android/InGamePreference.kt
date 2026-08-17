@file:JvmName("GameSharedPreference")

package org.uoyabause.android

import android.app.ActivityManager
import android.content.Context
import android.content.SharedPreferences
import android.content.pm.PackageManager
import android.os.Bundle
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AlertDialog
import androidx.core.content.ContextCompat
import androidx.preference.CheckBoxPreference
import androidx.preference.ListPreference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.PreferenceManager
import com.frybits.harmony.getHarmonySharedPreferences
import org.devmiyax.yabasanshiro.R

/**
 * Safe getString that handles ClassCastException when a preference value
 * was stored as a different type (e.g., Int) by an older version of the app.
 */
fun SharedPreferences.getStringSafe(key: String, defValue: String): String = try {
    getString(key, defValue) ?: defValue
} catch (e: ClassCastException) {
    try {
        getInt(key, defValue.toIntOrNull() ?: 0).toString()
    } catch (e2: ClassCastException) {
        defValue
    }
}

/**
 * Convert a legacy String-typed SharedPreferences entry to Int.
 * Used when a key changes from ListPreference/EditTextPreference (String) to
 * SeekBarPreference (Int) and existing installs still have the old String value,
 * which would otherwise throw ClassCastException during SeekBarPreference init.
 */
fun SharedPreferences.migrateStringToIntPreference(
    key: String,
    defaultInt: Int,
    min: Int = Int.MIN_VALUE,
    max: Int = Int.MAX_VALUE,
) {
    if (!contains(key)) return
    val current = all[key] ?: return
    if (current is String) {
        val parsed = current.toIntOrNull() ?: defaultInt
        val coerced = parsed.coerceIn(min, max)
        edit().remove(key).putInt(key, coerced).apply()
    }
}

fun setupInGamePreferences(
    context: Context,
    gameCode: String?,
) {
    if (gameCode.isNullOrEmpty()) {
        Log.d("setupInGamePreferences", "gamecode is null or empty. can not read game setting")
        return
    }
    val key = gameCode.replace(" ", "-")
    val gamePreference = context.getHarmonySharedPreferences(key)
    val defaultPreference = PreferenceManager.getDefaultSharedPreferences(context)
    if (!gamePreference.contains("pref_fps")) {
        val editor = gamePreference.edit()
        editor.putBoolean("pref_fps", defaultPreference.getBoolean("pref_fps", false))
        editor.apply()
    }

    if (!gamePreference.contains("pref_frameskip")) {
        val editor = gamePreference.edit()
        editor.putBoolean("pref_frameskip", defaultPreference.getBoolean("pref_frameskip", false))
        editor.apply()
    }

    if (!gamePreference.contains("pref_rotate_screen")) {
        val editor = gamePreference.edit()
        editor.putBoolean(
            "pref_rotate_screen",
            defaultPreference.getBoolean("pref_rotate_screen", false),
        )
        editor.apply()
    }

    if (!gamePreference.contains("pref_polygon_generation")) {
        val editor = gamePreference.edit()
        val globalValue = defaultPreference.getStringSafe("pref_polygon_generation", "0")
        val initial =
            if (defaultPreference.getStringSafe("pref_video", "0") == "4") {
                // Vulkan accepts only GPU_TESSERATION (2) and COMPUTE_RASTERIZER (3).
                if (globalValue in setOf("2", "3")) globalValue else "2"
            } else {
                globalValue
            }
        editor.putString("pref_polygon_generation", initial)
        editor.apply()
    }

    if (!gamePreference.contains("pref_frameLimit")) {
        val editor = gamePreference.edit()
        editor.putString(
            "pref_frameLimit",
            defaultPreference.getStringSafe("pref_frameLimit", "0"),
        )
        editor.apply()
    }

    if (!gamePreference.contains("pref_resolution")) {
        val editor = gamePreference.edit()
        editor.putString("pref_resolution", defaultPreference.getStringSafe("pref_resolution", "0"))
        editor.apply()
    }

    if (!gamePreference.contains("pref_aspect_rate")) {
        val editor = gamePreference.edit()
/*
            val displayMetrics = DisplayMetrics()
            windowManager.defaultDisplay.getMetrics(displayMetrics)
            val height = displayMetrics.heightPixels
            val width = displayMetrics.widthPixels
            val arate = width.toDouble() / height.toDouble()


            if( arate >= 1.2 && arate <= 1.34 ){
                // for 4:3 display force default setting is 4:3
                val v = defaultPreference.getString("pref_aspect_rate","1")
                editor.putString("pref_aspect_rate", v)
            }else{
                val v = defaultPreference.getString("pref_aspect_rate","0")
                editor.putString("pref_aspect_rate", v)
            }
*/
        val v = defaultPreference.getStringSafe("pref_aspect_rate", "0")
        editor.putString("pref_aspect_rate", v)
        editor.apply()
    }

    if (!gamePreference.contains("pref_rbg_resolution")) {
        val editor = gamePreference.edit()
        editor.putString(
            "pref_rbg_resolution",
            defaultPreference.getStringSafe("pref_rbg_resolution", "0"),
        )
        editor.apply()
    }

    if (!gamePreference.contains("pref_use_compute_shader")) {
        val editor = gamePreference.edit()
        editor.putBoolean(
            "pref_use_compute_shader",
            defaultPreference.getBoolean("pref_use_compute_shader", false),
        )
        editor.apply()
    }

    if (!gamePreference.contains("pref_video")) {
        val editor = gamePreference.edit()
        editor.putString(
            "pref_video",
            defaultPreference.getStringSafe("pref_video", "1"),
        )
        editor.apply()
    }

    val gameSharedPreference = context.getSharedPreferences(gameCode, 0)
    val editor = gameSharedPreference.edit()
    editor.putBoolean("pref_fps", gamePreference.getBoolean("pref_fps", false))
    editor.putBoolean("pref_frameskip", gamePreference.getBoolean("pref_frameskip", false))
    editor.putBoolean("pref_rotate_screen", gamePreference.getBoolean("pref_rotate_screen", false))
    editor.putString(
        "pref_polygon_generation",
        gamePreference.getStringSafe("pref_polygon_generation", "0"),
    )
    editor.putBoolean("pref_use_compute_shader", gamePreference.getBoolean("pref_use_compute_shader", false))
    editor.putString("pref_frameLimit", gamePreference.getStringSafe("pref_frameLimit", "0"))
    val v = gamePreference.getStringSafe("pref_aspect_rate", "0")
    editor.putString("pref_aspect_rate", v)
    editor.putString("pref_rbg_resolution", gamePreference.getStringSafe("pref_rbg_resolution", "0"))
    editor.putString("pref_video", gamePreference.getStringSafe("pref_video", "1"))
    editor.apply()
}

class InGamePreference :
    PreferenceFragmentCompat(),
    SharedPreferences.OnSharedPreferenceChangeListener {
    companion object {
        const val TAG = "InGamePreference"
        private const val ARG_GAMECODE = "gamecode"

        fun newInstance(gamecode: String): InGamePreference =
            InGamePreference().apply {
                arguments = Bundle().apply { putString(ARG_GAMECODE, gamecode) }
            }
    }

    private val gamecode: String
        get() = arguments?.getString(ARG_GAMECODE).orEmpty()

    private var onEndCallback: (() -> Unit)? = null
    private lateinit var activityContext: Context
    private var previousVideoValue: String? = null
    private var needsRestart = false

    private fun showSummary(listPreference: ListPreference) {
        listPreference.summary = listPreference.entry
        listPreference.setOnPreferenceChangeListener { preference, newValue ->
            if (preference is ListPreference) {
                val index = preference.findIndexOfValue(newValue.toString())
                val entry = preference.entries.get(index)
                preference.summary = entry
            }
            true
        }
    }

    private fun setSummaries() {
        showSummary(findPreference<ListPreference?>("pref_video")!!)
        showSummary(findPreference<ListPreference?>("pref_polygon_generation")!!)
        showSummary(findPreference<ListPreference?>("pref_resolution")!!)
        showSummary(findPreference<ListPreference?>("pref_rbg_resolution")!!)
        showSummary(findPreference<ListPreference?>("pref_aspect_rate")!!)
        showSummary(findPreference<ListPreference?>("pref_frameLimit")!!)
    }

    override fun onAttach(context: Context) {
        this.activityContext = context
        super.onAttach(context)
    }

    override fun onCreatePreferences(
        savedInstanceState: Bundle?,
        rootKey: String?,
    ) {
        setupInGamePreferences(activityContext, gamecode)
        if (gamecode.isNotEmpty()) {
            preferenceManager.sharedPreferencesName = gamecode
        }
        setPreferencesFromResource(R.xml.in_game_preferences, rootKey)

        val defaultPreference = PreferenceManager.getDefaultSharedPreferences(activityContext)
        val res = activityContext.resources

        // Setup video core preference options
        val videoPref = findPreference<ListPreference?>("pref_video")
        if (videoPref != null) {
            val videoLabels: MutableList<CharSequence> = ArrayList()
            val videoValues: MutableList<CharSequence> = ArrayList()

            val activityManager = activityContext.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
            val configurationInfo = activityManager.deviceConfigurationInfo
            val supportsEs3 = configurationInfo.reqGlEsVersion >= 0x30000

            if (supportsEs3) {
                videoLabels.add(res.getString(R.string.opengl_video_interface))
                videoValues.add("1")
            }

            videoLabels.add(res.getString(R.string.software_video_interface))
            videoValues.add("2")

            if (activityContext.packageManager.hasSystemFeature(PackageManager.FEATURE_VULKAN_HARDWARE_LEVEL)) {
                videoLabels.add(res.getString(R.string.vulkan_video_interface))
                videoValues.add("4")
            }

            videoPref.entries = videoLabels.toTypedArray()
            videoPref.entryValues = videoValues.toTypedArray()
        }

        val currentVideoValue = defaultPreference.getStringSafe("pref_video", "0")

        // Store the initial video value
        previousVideoValue = preferenceScreen.sharedPreferences?.getStringSafe("pref_video", currentVideoValue)

        // Update polygon generation and compute shader settings based on current video core
        updateVideoRelatedSettings(previousVideoValue)

        setSummaries()

        // Pro gate must be installed AFTER setSummaries(), because showSummary() inside
        // setSummaries() overwrites OnPreferenceChangeListener on each ListPreference.
        // This listener subsumes showSummary's summary-update behavior on accepted changes.
        findPreference<ListPreference?>("pref_polygon_generation")?.setOnPreferenceChangeListener { preference, newValue ->
            if (newValue == "3" && !YabauseApplication.isPro()) {
                YabauseApplication.checkDonated(
                    requireActivity(),
                    getString(R.string.pro_gate_compute_rasterizer_message),
                )
                false
            } else {
                if (newValue == "3") {
                    // VDP1 Compute Rasterizer is experimental: show the report-bugs notice once.
                    ExperimentalFeatureNotice.maybeShow(
                        requireContext(),
                        ExperimentalFeatureNotice.KEY_VDP1_COMPUTE_RASTERIZER,
                    )
                }
                if (preference is ListPreference) {
                    val index = preference.findIndexOfValue(newValue.toString())
                    if (index >= 0) preference.summary = preference.entries[index]
                }
                true
            }
        }

        this.preferenceScreen.sharedPreferences!!.registerOnSharedPreferenceChangeListener(this)
    }

    override fun onPause() {
        super.onPause()
        this.preferenceScreen.sharedPreferences!!.unregisterOnSharedPreferenceChangeListener(this)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        val view = super.onCreateView(inflater, container, savedInstanceState)
        view.setBackgroundColor(ContextCompat.getColor(activityContext, R.color.default_background))
        return view
    }

    fun setOnEndCallback(callback: () -> Unit) {
        this.onEndCallback = callback
    }

    fun onBackPressed() {
        onEndCallback?.invoke()
    }

    override fun onSharedPreferenceChanged(
        sharedPreferences: SharedPreferences?,
        key: String?,
    ) {
        if (sharedPreferences == null) {
            return
        }

        // Check if pref_video changed
        if (key == "pref_video") {
            val newVideoValue = sharedPreferences.getStringSafe("pref_video", "1")

            // Update polygon generation and compute shader settings based on video core
            updateVideoRelatedSettings(newVideoValue)

            if (newVideoValue != previousVideoValue) {
                // Video setting changed, show restart confirmation dialog
                showRestartConfirmationDialog(sharedPreferences)
                return
            }
        }

        val prefKey = gamecode.replace(" ", "-")
        val gamePreference = requireContext().getHarmonySharedPreferences(prefKey)

        val editor = gamePreference.edit()
        editor.putBoolean("pref_fps", sharedPreferences.getBoolean("pref_fps", false))
        editor.putBoolean("pref_frameskip", sharedPreferences.getBoolean("pref_frameskip", false))
        editor.putBoolean("pref_rotate_screen", sharedPreferences.getBoolean("pref_rotate_screen", false))
        editor.putString("pref_polygon_generation", sharedPreferences.getStringSafe("pref_polygon_generation", "0"))
        editor.putString("pref_frameLimit", sharedPreferences.getStringSafe("pref_frameLimit", "0"))
        val v = sharedPreferences.getStringSafe("pref_aspect_rate", "0")
        editor.putString("pref_aspect_rate", v)
        editor.putString(
            "pref_resolution",
            sharedPreferences.getStringSafe("pref_resolution", "0"),
        )
        editor.putString(
            "pref_rbg_resolution",
            sharedPreferences.getStringSafe("pref_rbg_resolution", "0"),
        )
        editor.putBoolean(
            "pref_use_compute_shader",
            sharedPreferences.getBoolean("pref_use_compute_shader", false),
        )
        editor.putString("pref_video", sharedPreferences.getStringSafe("pref_video", "1"))
        editor.apply()
    }

    private fun updateVideoRelatedSettings(videoValue: String?) {
        val polygonPref = findPreference<ListPreference?>("pref_polygon_generation")
        val computeShaderPref = findPreference<CheckBoxPreference?>("pref_use_compute_shader")

        when (videoValue) {
            "1" -> {
                // OpenGL: Triangles (0) and GPU_TESSERATION (2) only.
                // COMPUTE_RASTERIZER (3) is Vulkan-only and must not appear here.
                polygonPref?.isEnabled = true
                polygonPref?.entries =
                    arrayOf(
                        resources.getString(R.string.poly_triangle),
                        resources.getString(R.string.poly_gpu_tess),
                    )
                polygonPref?.entryValues = arrayOf("0", "2")
                if (polygonPref?.value !in setOf("0", "2")) {
                    polygonPref?.value = "0"
                }
                computeShaderPref?.isEnabled = true
            }
            "4" -> {
                // Vulkan: GPU_TESSERATION (2) または COMPUTE_RASTERIZER (3) を
                // 選択可能。compute shader (VDP2 layer) は強制 ON のままにする。
                polygonPref?.isEnabled = true
                polygonPref?.entries =
                    arrayOf(
                        resources.getString(R.string.poly_gpu_tess),
                        resources.getString(R.string.poly_compute_rasterizer),
                    )
                polygonPref?.entryValues = arrayOf("2", "3")
                if (polygonPref?.value !in setOf("2", "3")) {
                    polygonPref?.value = "2"
                }
                computeShaderPref?.isEnabled = false
                computeShaderPref?.isChecked = true
            }
            else -> {
                // Software or other: Disable both. Keep full entry set so the
                // user can switch to OpenGL/Vulkan later without losing options.
                polygonPref?.isEnabled = false
                polygonPref?.entries = resources.getStringArray(R.array.entries_polygon_generation_type)
                polygonPref?.entryValues = resources.getStringArray(R.array.entryvalues_polygon_generation_type)
                computeShaderPref?.isEnabled = false
                computeShaderPref?.isChecked = false
            }
        }
    }

    private fun showRestartConfirmationDialog(sharedPreferences: SharedPreferences) {
        val newVideoValue = sharedPreferences.getStringSafe("pref_video", "1")

        AlertDialog
            .Builder(requireContext())
            .setTitle(R.string.restart_required_title)
            .setMessage(R.string.restart_required_message)
            .setPositiveButton(R.string.yes) { _, _ ->
                // Save the setting and restart
                saveVideoSetting(sharedPreferences)
                needsRestart = true
                restartEmulation()
            }.setNegativeButton(R.string.no) { _, _ ->
                // Just save the setting without restarting
                saveVideoSetting(sharedPreferences)
                // Revert the preference UI back to previous value to avoid confusion
                val videoPref = findPreference<ListPreference?>("pref_video")
                videoPref?.value = previousVideoValue
                // Also revert the related settings UI
                updateVideoRelatedSettings(previousVideoValue)
            }.setCancelable(false)
            .show()
    }

    private fun saveVideoSetting(sharedPreferences: SharedPreferences) {
        val prefKey = gamecode.replace(" ", "-")
        val gamePreference = requireContext().getHarmonySharedPreferences(prefKey)

        val editor = gamePreference.edit()
        editor.putBoolean("pref_fps", sharedPreferences.getBoolean("pref_fps", false))
        editor.putBoolean("pref_frameskip", sharedPreferences.getBoolean("pref_frameskip", false))
        editor.putBoolean("pref_rotate_screen", sharedPreferences.getBoolean("pref_rotate_screen", false))
        editor.putString("pref_polygon_generation", sharedPreferences.getStringSafe("pref_polygon_generation", "0"))
        editor.putString("pref_frameLimit", sharedPreferences.getStringSafe("pref_frameLimit", "0"))
        val v = sharedPreferences.getStringSafe("pref_aspect_rate", "0")
        editor.putString("pref_aspect_rate", v)
        editor.putString(
            "pref_resolution",
            sharedPreferences.getStringSafe("pref_resolution", "0"),
        )
        editor.putString(
            "pref_rbg_resolution",
            sharedPreferences.getStringSafe("pref_rbg_resolution", "0"),
        )
        editor.putBoolean(
            "pref_use_compute_shader",
            sharedPreferences.getBoolean("pref_use_compute_shader", false),
        )
        editor.putString("pref_video", sharedPreferences.getStringSafe("pref_video", "1"))
        editor.apply()

        // Update previous value
        previousVideoValue = sharedPreferences.getStringSafe("pref_video", "1")
    }

    private fun restartEmulation() {
        // Trigger activity restart
        requireActivity().recreate()
    }
}
