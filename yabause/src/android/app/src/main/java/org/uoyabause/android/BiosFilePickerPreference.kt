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

import android.content.Context
import android.util.AttributeSet
import androidx.preference.DialogPreference
import androidx.preference.PreferenceManager
import org.devmiyax.yabasanshiro.R

/**
 * Custom DialogPreference for BIOS file selection.
 * Shows current BIOS selection status in the summary.
 */
class BiosFilePickerPreference
    @JvmOverloads
    constructor(
        context: Context,
        attrs: AttributeSet? = null,
        defStyleAttr: Int = androidx.preference.R.attr.dialogPreferenceStyle,
        defStyleRes: Int = 0,
    ) : DialogPreference(context, attrs, defStyleAttr, defStyleRes) {
        init {
            dialogLayoutResource = R.layout.dialog_bios_picker
        }

        override fun getSummary(): CharSequence {
            val prefs = PreferenceManager.getDefaultSharedPreferences(context)
            val type = prefs.getString(BiosManager.KEY_BIOS_TYPE, BiosManager.BIOS_TYPE_BUILTIN)
            val filename = prefs.getString(BiosManager.KEY_BIOS_FILENAME, "")

            return when {
                type == BiosManager.BIOS_TYPE_FILE && !filename.isNullOrEmpty() -> {
                    val biosFile = BiosManager.getBiosFile(context)
                    if (biosFile.exists()) {
                        filename
                    } else {
                        context.getString(R.string.bios_file_not_found)
                    }
                }
                else -> context.getString(R.string.builtin_bios)
            }
        }

        /**
         * Refreshes the summary to reflect current preference values.
         * Call this after BIOS selection changes.
         */
        fun refreshSummary() {
            notifyChanged()
        }
    }
