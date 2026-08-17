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

import android.content.Context
import android.content.DialogInterface
import android.os.Bundle
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.fragment.app.setFragmentResultListener
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.button.MaterialButton
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.firebase.database.DataSnapshot
import com.google.firebase.database.DatabaseError
import com.google.firebase.database.FirebaseDatabase
import com.google.firebase.database.ValueEventListener
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.auth.AuthState

/**
 * A fragment representing a list of local cheat items.
 * Activities containing this fragment MUST implement the [OnListFragmentInteractionListener]
 * interface.
 */
class LocalCheatItemFragment :
    BaseCheatItemFragment(),
    LocalCheatItemRecyclerViewAdapter.OnItemClickListener {
    private var listener: OnListFragmentInteractionListener? = null
    private var backcode = ""
    var adapter: LocalCheatItemRecyclerViewAdapter? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val cnv = CheatConverter()
        if (cnv.hasOldVersion()) {
            cnv.execute()
        }

        // Set up fragment result listeners
        setupFragmentResultListeners()
    }

    private fun setupFragmentResultListeners() {
        // Listen for new item results
        setFragmentResultListener(LocalCheatEditDialog.REQUEST_KEY_NEW) { _, bundle ->
            val resultCode = bundle.getInt(LocalCheatEditDialog.RESULT_CODE)
            if (resultCode == LocalCheatEditDialog.APPLY) {
                if (database == null) {
                    showErrorMessage()
                    return@setFragmentResultListener
                }
                val key = database!!.push().key
                val desc = bundle.getString(LocalCheatEditDialog.DESC)
                val code = bundle.getString(LocalCheatEditDialog.CODE)
                database!!.child(key!!).child("description").setValue(desc)
                database!!.child(key).child("cheat_code").setValue(code)
                adapter!!.notifyDataSetChanged()
            }
        }

        // Listen for edit item results
        setFragmentResultListener(LocalCheatEditDialog.REQUEST_KEY_EDIT) { _, bundle ->
            val resultCode = bundle.getInt(LocalCheatEditDialog.RESULT_CODE)
            if (resultCode == LocalCheatEditDialog.APPLY) {
                if (database == null) {
                    showErrorMessage()
                    return@setFragmentResultListener
                }
                val frag = tabCheatFragmentInstance
                frag?.removeActiveCheat(backcode)
                val key = bundle.getString(LocalCheatEditDialog.KEY)
                val desc = bundle.getString(LocalCheatEditDialog.DESC)
                val code = bundle.getString(LocalCheatEditDialog.CODE)
                database!!.child(key!!).child("description").setValue(desc)
                database!!.child(key).child("cheat_code").setValue(code)
                adapter!!.notifyDataSetChanged()
            }
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View? {
        val view = inflater.inflate(R.layout.fragment_localcheatitem_list, container, false)
        listView = view.findViewById<View>(R.id.list) as RecyclerView
        val context = view.context
        listView!!.layoutManager = LinearLayoutManager(context)
        rootView = view
        setupAuthRequired(view, view.findViewById(R.id.content_layout))
        updateCheatList()
        val add = view.findViewById<View>(R.id.button_add) as MaterialButton
        add.setOnClickListener { _ -> onAddItem() }
        return view
    }

    @Deprecated("Deprecated in Java")
    override fun setUserVisibleHint(isVisibleToUser: Boolean) {
        @Suppress("DEPRECATION")
        super.setUserVisibleHint(isVisibleToUser)
        if (isVisibleToUser) {
            // updateCheatList();
        } else {
        }
    }

    override fun onAttach(context: Context) {
        super.onAttach(context)
        if (context is OnListFragmentInteractionListener) {
            listener = context
        } else {
//            throw new RuntimeException(context.toString()
//                    + " must implement OnListFragmentInteractionListener");
        }
    }

    override fun onDetach() {
        super.onDetach()
        listener = null
    }

    fun showErrorMessage() {
        Toast
            .makeText(context, "You need to Sign in before use this function", Toast.LENGTH_LONG)
            .show()
    }

    override fun updateCheatList() {
        if (AuthState.realUser() == null) {
            showAuthRequired()
            return
        }
        hideAuthRequired()
        if (listView == null) {
            return
        }
        items = ArrayList()
        val baseref = FirebaseDatabase.getInstance().reference
        val baseurl = "/user-posts/" + AuthState.realUser()!!.uid + "/cheat/" + gameCode
        database = baseref.child(baseurl)
        if (database == null) {
            showErrorMessage()
            return
        }
        adapter = LocalCheatItemRecyclerViewAdapter(items, this@LocalCheatItemFragment)
        listView!!.adapter = adapter
        val dataListener: ValueEventListener =
            object : ValueEventListener {
                override fun onDataChange(dataSnapshot: DataSnapshot) {
                    if (dataSnapshot.hasChildren()) {
                        items!!.clear()
                        for (child in dataSnapshot.children) {
                            val newitem = child.getValue(CheatItem::class.java)
                            newitem!!.key = child.key!!
                            val frag = tabCheatFragmentInstance
                            if (frag != null) {
                                newitem.enable = frag.isActive(newitem.cheat_code)
                            }
                            items!!.add(newitem)
                        }
                        adapter =
                            LocalCheatItemRecyclerViewAdapter(items, this@LocalCheatItemFragment)
                        listView!!.adapter = adapter
                    } else {
                        items!!.clear()
                        adapter =
                            LocalCheatItemRecyclerViewAdapter(items, this@LocalCheatItemFragment)
                        listView!!.adapter = adapter
                    }
                }

                override fun onCancelled(databaseError: DatabaseError) {
                    Log.e("CheatEditDialog", "Bad Data " + databaseError.message)
                }
            }
        database!!.addValueEventListener(dataListener)
    }

    override fun onItemClick(
        position: Int,
        item: CheatItem?,
        v: View?,
    ) {
        val cheatitem = item ?: return
        val activateText = if (cheatitem.enable) getString(R.string.disable) else getString(R.string.enable)
        val shareText = if (cheatitem.sharedKey != "") getString(R.string.unsahre) else getString(R.string.share)
        val options = arrayOf(activateText, getString(R.string.edit), shareText, getString(R.string.delete))

        MaterialAlertDialogBuilder(requireContext())
            .setTitle(cheatitem.description)
            .setItems(options) { _, which ->
                when (which) {
                    0 -> { // Enable/Disable
                        toggleCheatEnable(cheatitem)
                        adapter?.notifyDataSetChanged()
                    }
                    1 -> { // Edit
                        backcode = cheatitem.cheat_code
                        val newFragment = LocalCheatEditDialog()
                        newFragment.setEditTarget(cheatitem)
                        newFragment.setRequestKey(LocalCheatEditDialog.REQUEST_KEY_EDIT)
                        newFragment.show(parentFragmentManager, "Cheat")
                    }
                    2 -> { // Share/Unshare
                        if (cheatitem.sharedKey != "") unShare(cheatitem) else share(cheatitem)
                    }
                    3 -> { // Delete
                        removeCheat(cheatitem)
                    }
                }
            }.show()
    }

    fun onAddItem() {
        val newFragment = LocalCheatEditDialog()
        newFragment.setRequestKey(LocalCheatEditDialog.REQUEST_KEY_NEW)
        newFragment.show(parentFragmentManager, "Cheat")
    }

    fun removeCheat(cheatitem: CheatItem) {
        val dialog =
            MaterialAlertDialogBuilder(requireContext())
                .setMessage(getString(R.string.are_you_sure_to_delete) + cheatitem.description + "?")
                .setPositiveButton(
                    R.string.yes,
                    DialogInterface.OnClickListener { _, _ ->
                        if (database == null) {
                            showErrorMessage()
                            return@OnClickListener
                        }
                        database!!.child(cheatitem.key).removeValue()
                        if (cheatitem.sharedKey !== "") {
                            val baseref = FirebaseDatabase.getInstance().reference
                            val baseurl = "/shared-cheats/$gameCode"
                            val sharedb = baseref.child(baseurl)
                            sharedb.child(cheatitem.sharedKey).removeValue()
                        }
                    },
                ).setNegativeButton(R.string.no, null)
                .create()

        // ゲームパッド対応: 破壊的操作なのでCancelボタンにフォーカス
        dialog.setOnShowListener {
            val button = dialog.getButton(android.content.DialogInterface.BUTTON_NEGATIVE)
            button?.post {
                button.isFocusable = true
                button.isFocusableInTouchMode = true
                button.requestFocus()
            }
        }
        dialog.show()
    }

    fun remove(index: Int) {
        if (database == null) {
            showErrorMessage()
            return
        }
        database!!.child(items!![index]!!.key).removeValue()
    }

    fun share(cheatitem: CheatItem) {
        if (database == null) {
            showErrorMessage()
            return
        }
        if (cheatitem.sharedKey != "") {
            return
        }
        val userId = AuthState.realUser()?.uid ?: return
        val baseref = FirebaseDatabase.getInstance().reference
        val baseurl = "/shared-cheats/$gameCode"
        val sharedb = baseref.child(baseurl)
        val key = sharedb.push().key
        sharedb.child(key!!).child("description").setValue(cheatitem.description)
        sharedb.child(key).child("cheat_code").setValue(cheatitem.cheat_code)
        sharedb.child(key).child("user_id").setValue(userId)
        database!!.child(cheatitem.key).child("shared_key").setValue(key)
    }

    fun unShare(cheatitem: CheatItem) {
        if (database == null) {
            showErrorMessage()
            return
        }
        if (cheatitem.sharedKey == "") {
            return
        }
        val baseref = FirebaseDatabase.getInstance().reference
        val baseurl = "/shared-cheats/" + gameCode + "/" + cheatitem.sharedKey
        val sharedb = baseref.child(baseurl)
        sharedb.removeValue()
        database!!.child(cheatitem.key).child("shared_key").setValue("")
    }

    override fun moveCursor(direction: Int) {
        adapter?.moveCursor(direction)
    }

    override fun selectCurrentItem() {
        adapter?.selectFocused()
    }

    companion object {
        @JvmStatic
        fun newInstance(
            gameid: String?,
            columnCount: Int,
        ): LocalCheatItemFragment {
            val fragment = LocalCheatItemFragment()
            val args = Bundle()
            args.putString(ARG_GAME_ID, gameid)
            args.putInt(ARG_COLUMN_COUNT, columnCount)
            fragment.arguments = args
            return fragment
        }
    }
}
