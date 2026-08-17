/*
    Copyright 2024 devMiyax(smiyaxdev@gmail.com)

    This file is part of YabaSanshiro.

    YabaSanshiro is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    YabaSanshiro is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with YabaSanshiro; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

package org.uoyabause.android

import android.app.AlertDialog
import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.widget.Button
import android.widget.ImageButton
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.preference.PreferenceDialogFragmentCompat
import androidx.preference.PreferenceManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.devmiyax.yabasanshiro.R

/**
 * Dialog fragment for BIOS file selection using Storage Access Framework.
 * Supports multiple BIOS files with selection and deletion.
 */
class BiosFilePickerFragment : PreferenceDialogFragmentCompat() {
    companion object {
        fun newInstance(key: String): BiosFilePickerFragment = BiosFilePickerFragment().apply {
            arguments = Bundle().apply { putString(ARG_KEY, key) }
        }
    }

    private val filePickerLauncher =
        registerForActivityResult(
            ActivityResultContracts.OpenDocument(),
        ) { uri ->
            uri?.let { handleBiosSelected(it) }
        }

    private lateinit var radioGroup: RadioGroup
    private lateinit var radioBuiltin: RadioButton
    private lateinit var radioFile: RadioButton
    private lateinit var layoutFileSelection: LinearLayout
    private lateinit var containerBiosFiles: LinearLayout
    private lateinit var textNoBiosFiles: TextView
    private lateinit var buttonAddBios: Button
    private lateinit var textWarning: TextView
    private lateinit var progressBar: ProgressBar

    private var selectedBiosFile: String? = null
    private var copyJob: Job? = null

    override fun onBindDialogView(view: View) {
        super.onBindDialogView(view)

        radioGroup = view.findViewById(R.id.radio_group_bios_type)
        radioBuiltin = view.findViewById(R.id.radio_builtin)
        radioFile = view.findViewById(R.id.radio_file)
        layoutFileSelection = view.findViewById(R.id.layout_file_selection)
        containerBiosFiles = view.findViewById(R.id.container_bios_files)
        textNoBiosFiles = view.findViewById(R.id.text_no_bios_files)
        buttonAddBios = view.findViewById(R.id.button_add_bios)
        textWarning = view.findViewById(R.id.text_extended_memory_warning)
        progressBar = view.findViewById(R.id.progress_bar)

        // Load current state from preferences
        val prefs = PreferenceManager.getDefaultSharedPreferences(requireContext())
        val currentType = prefs.getString(BiosManager.KEY_BIOS_TYPE, BiosManager.BIOS_TYPE_BUILTIN)
        selectedBiosFile = prefs.getString(BiosManager.KEY_BIOS_SELECTED_FILE, null)

        // Set initial radio button state
        if (currentType == BiosManager.BIOS_TYPE_FILE) {
            radioFile.isChecked = true
            layoutFileSelection.visibility = View.VISIBLE
            textWarning.visibility = View.VISIBLE
        } else {
            radioBuiltin.isChecked = true
            layoutFileSelection.visibility = View.GONE
            textWarning.visibility = View.GONE
        }

        // Load BIOS file list
        refreshBiosList()

        // Handle radio group changes
        radioGroup.setOnCheckedChangeListener { _, checkedId ->
            when (checkedId) {
                R.id.radio_builtin -> {
                    layoutFileSelection.visibility = View.GONE
                    textWarning.visibility = View.GONE
                }
                R.id.radio_file -> {
                    layoutFileSelection.visibility = View.VISIBLE
                    textWarning.visibility = View.VISIBLE
                    refreshBiosList()
                }
            }
        }

        // Handle add BIOS button
        buttonAddBios.setOnClickListener {
            openFilePicker()
        }
    }

    private fun refreshBiosList() {
        val biosFiles = BiosManager.getBiosFiles(requireContext())

        // Clear existing views
        containerBiosFiles.removeAllViews()

        if (biosFiles.isEmpty()) {
            containerBiosFiles.visibility = View.GONE
            textNoBiosFiles.visibility = View.VISIBLE
        } else {
            containerBiosFiles.visibility = View.VISIBLE
            textNoBiosFiles.visibility = View.GONE

            val inflater = LayoutInflater.from(requireContext())
            for (biosInfo in biosFiles) {
                val itemView = inflater.inflate(R.layout.item_bios_file, containerBiosFiles, false)
                bindBiosItem(itemView, biosInfo)
                containerBiosFiles.addView(itemView)
            }
        }
    }

    private fun bindBiosItem(
        view: View,
        biosInfo: BiosFileInfo,
    ) {
        val radioSelect = view.findViewById<RadioButton>(R.id.radio_bios_select)
        val textName = view.findViewById<TextView>(R.id.text_bios_name)
        val textStatus = view.findViewById<TextView>(R.id.text_bios_status)
        val buttonDelete = view.findViewById<ImageButton>(R.id.button_delete_bios)

        textName.text = biosInfo.filename
        textStatus.text =
            if (biosInfo.isKnown) {
                getString(R.string.bios_verified)
            } else {
                getString(R.string.bios_unverified)
            }
        textStatus.setTextColor(
            if (biosInfo.isKnown) {
                requireContext().getColor(R.color.colorPrimary)
            } else {
                requireContext().getColor(R.color.colorWarning)
            },
        )

        radioSelect.isChecked = biosInfo.filename == selectedBiosFile

        view.setOnClickListener {
            selectedBiosFile = biosInfo.filename
            refreshBiosList()
        }

        buttonDelete.setOnClickListener {
            showDeleteConfirmation(biosInfo)
        }
    }

    private fun showDeleteConfirmation(biosInfo: BiosFileInfo) {
        AlertDialog
            .Builder(requireContext())
            .setTitle(R.string.delete)
            .setMessage(getString(R.string.confirm_delete_bios))
            .setPositiveButton(android.R.string.ok) { _, _ ->
                deleteBiosFile(biosInfo)
            }.setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    private fun deleteBiosFile(biosInfo: BiosFileInfo) {
        if (BiosManager.deleteBiosFile(requireContext(), biosInfo.filename)) {
            Toast.makeText(requireContext(), R.string.bios_deleted, Toast.LENGTH_SHORT).show()

            // If deleted file was selected, clear selection
            if (selectedBiosFile == biosInfo.filename) {
                selectedBiosFile = null
            }

            refreshBiosList()
        }
    }

    private fun openFilePicker() {
        filePickerLauncher.launch(
            arrayOf(
                "application/octet-stream",
                "application/zip",
                "*/*",
            ),
        )
    }

    private fun handleBiosSelected(uri: Uri) {
        val context = requireContext()

        // Show progress
        progressBar.visibility = View.VISIBLE
        buttonAddBios.isEnabled = false

        copyJob =
            CoroutineScope(Dispatchers.Main).launch {
                val importResult =
                    withContext(Dispatchers.IO) {
                        BiosManager.importBiosFromUri(context, uri)
                    }

                if (importResult.success && importResult.filenames.isNotEmpty()) {
                    // Verify all imported files
                    var verifiedCount = 0
                    var firstVerifiedFile: String? = null

                    for (filename in importResult.filenames) {
                        val file = java.io.File(BiosManager.getBiosDir(context), filename)
                        val checksum =
                            withContext(Dispatchers.IO) {
                                BiosManager.calculateMD5(file)
                            }
                        val isValid = BiosManager.isKnownChecksum(checksum)
                        if (isValid) {
                            verifiedCount++
                            if (firstVerifiedFile == null) {
                                firstVerifiedFile = filename
                            }
                        }
                    }

                    // Select the first verified file, or the first file if none are verified
                    val fileToSelect = firstVerifiedFile ?: importResult.filenames.first()
                    val selectedFile = java.io.File(BiosManager.getBiosDir(context), fileToSelect)
                    val checksum =
                        withContext(Dispatchers.IO) {
                            BiosManager.calculateMD5(selectedFile)
                        }
                    val isValid = BiosManager.isKnownChecksum(checksum)

                    // Save to preferences
                    PreferenceManager.getDefaultSharedPreferences(context).edit().apply {
                        putString(BiosManager.KEY_BIOS_FILENAME, fileToSelect)
                        putString(BiosManager.KEY_BIOS_SELECTED_FILE, fileToSelect)
                        putString(BiosManager.KEY_BIOS_CHECKSUM, checksum)
                        putBoolean(BiosManager.KEY_BIOS_VALID, isValid)
                        apply()
                    }

                    // Update selection
                    selectedBiosFile = fileToSelect

                    // Refresh list
                    refreshBiosList()

                    // Show result message
                    val totalCount = importResult.filenames.size
                    if (totalCount > 1) {
                        // Multiple files imported (from ZIP)
                        val message = getString(R.string.bios_zip_extracted, totalCount, verifiedCount)
                        Toast.makeText(context, message, Toast.LENGTH_LONG).show()
                    } else {
                        // Single file imported
                        if (!isValid) {
                            Toast.makeText(context, R.string.bios_unknown_warning, Toast.LENGTH_LONG).show()
                        } else {
                            Toast.makeText(context, R.string.bios_copy_success, Toast.LENGTH_SHORT).show()
                        }
                    }
                } else {
                    // Import failed
                    val errorMsg = importResult.error ?: getString(R.string.bios_copy_failed)
                    Toast.makeText(context, errorMsg, Toast.LENGTH_LONG).show()
                }

                // Hide progress
                progressBar.visibility = View.GONE
                buttonAddBios.isEnabled = true
            }
    }

    override fun onDialogClosed(positiveResult: Boolean) {
        if (positiveResult) {
            val prefs = PreferenceManager.getDefaultSharedPreferences(requireContext())
            prefs.edit().apply {
                putString(
                    BiosManager.KEY_BIOS_TYPE,
                    if (radioFile.isChecked) BiosManager.BIOS_TYPE_FILE else BiosManager.BIOS_TYPE_BUILTIN,
                )
                if (selectedBiosFile != null) {
                    putString(BiosManager.KEY_BIOS_SELECTED_FILE, selectedBiosFile)
                    putString(BiosManager.KEY_BIOS_FILENAME, selectedBiosFile)
                }
                apply()
            }

            // Refresh the preference summary
            (preference as? BiosFilePickerPreference)?.refreshSummary()
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        copyJob?.cancel()
    }
}
