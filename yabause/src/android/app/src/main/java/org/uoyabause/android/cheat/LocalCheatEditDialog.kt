/*  Copyright 2019 devMiyax(smiyaxdev@gmail.com)

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
package org.uoyabause.android.cheat

import android.app.Dialog
import android.content.Context
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.inputmethod.EditorInfo
import android.widget.TextView.OnEditorActionListener
import androidx.core.os.bundleOf
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.setFragmentResult
import com.google.android.material.button.MaterialButton
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.textfield.TextInputEditText
import org.devmiyax.yabasanshiro.R

class LocalCheatEditDialog : DialogFragment() {
    var contentView: View? = null
    var target: CheatItem? = null
    private var requestKey: String = REQUEST_KEY_NEW

    fun setEditTarget(cheat: CheatItem?) {
        target = cheat
    }

    fun setRequestKey(key: String) {
        requestKey = key
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val builder = MaterialAlertDialogBuilder(requireActivity())
        val inflater =
            requireActivity().getSystemService(Context.LAYOUT_INFLATER_SERVICE) as LayoutInflater
        contentView = inflater.inflate(R.layout.edit_cheat, null)
        if (contentView == null) {
            return builder.create()
        }

        builder.setView(contentView)
        val desc = contentView!!.findViewById<TextInputEditText>(R.id.editText_cheat_desc)
        val code = contentView!!.findViewById<TextInputEditText>(R.id.editText_code)
        if (target != null) {
            desc.setText(target!!.description)
            code.setText(target!!.cheat_code)
        }

        // Apply Button
        val buttonApply = contentView!!.findViewById<MaterialButton>(R.id.button_cheat_edit_apply)
        buttonApply.setOnClickListener {
            val cheatDesc = contentView!!.findViewById<TextInputEditText>(R.id.editText_cheat_desc)
            val cheatCode = contentView!!.findViewById<TextInputEditText>(R.id.editText_code)
            val sdesc = cheatDesc.text.toString()
            val scode = cheatCode.text.toString()
            val result =
                bundleOf(
                    RESULT_CODE to APPLY,
                    DESC to sdesc,
                    CODE to scode,
                )
            if (target != null) {
                result.putString(KEY, target!!.key)
            }
            setFragmentResult(requestKey, result)
            dialog!!.dismiss()
        }
        val buttonCancel = contentView!!.findViewById<MaterialButton>(R.id.button_edit_cheat_cancel)
        buttonCancel.setOnClickListener {
            setFragmentResult(requestKey, bundleOf(RESULT_CODE to CANCEL))
            dismiss()
        }
        code.setOnEditorActionListener(
            OnEditorActionListener { _, actionId, _ ->
                // For ShieldTV
                if (actionId == EditorInfo.IME_ACTION_NEXT) {
                    val editView = contentView!!.findViewById<TextInputEditText>(R.id.editText_code)
                    var currentstr: String? = editView.text.toString()
                    currentstr += System.lineSeparator()
                    editView.setText(currentstr)
                    return@OnEditorActionListener true
                }
                false
            },
        )
        return builder.create()
    }

    companion object {
        const val APPLY = 0
        const val CANCEL = 1
        const val KEY = "key"
        const val DESC = "desc"
        const val CODE = "code"
        const val RESULT_CODE = "result_code"
        const val REQUEST_KEY_NEW = "local_cheat_new"
        const val REQUEST_KEY_EDIT = "local_cheat_edit"
    }
}
