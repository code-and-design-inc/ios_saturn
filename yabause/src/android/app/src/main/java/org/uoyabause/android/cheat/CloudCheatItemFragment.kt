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
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.firebase.database.DataSnapshot
import com.google.firebase.database.DatabaseError
import com.google.firebase.database.FirebaseDatabase
import com.google.firebase.database.Transaction
import com.google.firebase.database.ValueEventListener
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.YabauseApplication
import org.uoyabause.android.auth.AuthState
import java.util.Collections

/**
 * A fragment representing a list of cloud cheat items.
 * Activities containing this fragment MUST implement the [OnListFragmentInteractionListener]
 * interface.
 */
class CloudCheatItemFragment :
    BaseCheatItemFragment(),
    CloudCheatItemRecyclerViewAdapter.OnItemClickListener {
    private var listener: OnListFragmentInteractionListener? = null
    var adapter: CloudCheatItemRecyclerViewAdapter? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View? {
        val view = inflater.inflate(R.layout.fragment_cloudcheatitem_list, container, false)
        listView = view.findViewById<View>(R.id.list) as RecyclerView
        rootView = view
        setupAuthRequired(view, view.findViewById(R.id.list))
        updateCheatList()
        return view
    }

    override fun onAttach(context: Context) {
        super.onAttach(context)
        if (context is OnListFragmentInteractionListener) {
            listener = context
        }
    }

    override fun onDetach() {
        super.onDetach()
        listener = null
    }

    override fun updateCheatList() {
        if (AuthState.realUser() == null) {
            showAuthRequired()
            return
        }
        if (!YabauseApplication.isPro()) {
            YabauseApplication.checkDonated(requireActivity())
            return
        }
        hideAuthRequired()
        items = ArrayList()
        val baseref = FirebaseDatabase.getInstance().reference
        val baseurl = "/shared-cheats/$gameCode"
        database = baseref.child(baseurl)
        if (database == null) {
            return
        }
        adapter = CloudCheatItemRecyclerViewAdapter(items, this@CloudCheatItemFragment)
        listView!!.adapter = adapter
        val dataListener: ValueEventListener =
            object : ValueEventListener {
                override fun onDataChange(dataSnapshot: DataSnapshot) {
                    if (dataSnapshot.hasChildren()) {
                        items!!.clear()
                        for (child in dataSnapshot.children) {
                            try {
                                val newitem = child.getValue(CheatItem::class.java)
                                newitem!!.key = child.key!!
                                val frag = tabCheatFragmentInstance
                                if (frag != null) {
                                    newitem.enable = frag.isActive(newitem.cheat_code)
                                }
                                items!!.add(newitem)
                            } catch (e: Exception) {
                            }
                        }
                        items?.let { Collections.reverse(it) }
                        adapter =
                            CloudCheatItemRecyclerViewAdapter(items, this@CloudCheatItemFragment)
                        listView!!.adapter = adapter
                        listView!!.post {
                            adapter!!.notifyDataSetChanged()
                        }
                    } else {
                        Log.e(TAG, "Bad Data " + dataSnapshot.key)
                    }
                }

                override fun onCancelled(databaseError: DatabaseError) {
                    Log.e(TAG, "onCancelled " + databaseError.message)
                }
            }
        database!!.orderByChild("star_count").addValueEventListener(dataListener)
    }

    override fun onItemClick(
        position: Int,
        item: CheatItem?,
        v: View?,
        isLikeButton: Boolean,
    ) {
        if (item == null) return

        if (isLikeButton) {
            toggleLike(item)
        } else {
            toggleEnable(item)
        }
    }

    override fun onItemDelete(
        position: Int,
        item: CheatItem?,
    ) {
        if (item == null) return
        removeCheat(position, item)
    }

    private fun toggleEnable(item: CheatItem) {
        toggleCheatEnable(item)
        listView?.post {
            adapter?.notifyDataSetChanged()
        }
    }

    private fun toggleLike(item: CheatItem) {
        val userId = AuthState.realUser()?.uid ?: return
        val likeRef = database!!.child(item.key).child("like_users").child(userId)
        val itemRef = database!!.child(item.key)

        likeRef.get().addOnSuccessListener { dataSnapshot ->
            if (dataSnapshot.exists()) {
                // Unlike
                likeRef.removeValue()
                itemRef.child("star_count").runTransaction(
                    object : Transaction.Handler {
                        override fun doTransaction(mutableData: com.google.firebase.database.MutableData): Transaction.Result {
                            val value = (mutableData.value as? Long ?: 0).toInt()
                            mutableData.value = Math.max(0, value - 1)
                            return Transaction.success(mutableData)
                        }

                        override fun onComplete(
                            error: DatabaseError?,
                            committed: Boolean,
                            currentData: DataSnapshot?,
                        ) {
                            listView?.post {
                                adapter?.notifyDataSetChanged()
                            }
                        }
                    },
                )
            } else {
                // Like
                likeRef.setValue(true)
                itemRef.child("star_count").runTransaction(
                    object : Transaction.Handler {
                        override fun doTransaction(mutableData: com.google.firebase.database.MutableData): Transaction.Result {
                            val value = (mutableData.value as? Long ?: 0).toInt()
                            mutableData.value = value + 1
                            return Transaction.success(mutableData)
                        }

                        override fun onComplete(
                            error: DatabaseError?,
                            committed: Boolean,
                            currentData: DataSnapshot?,
                        ) {
                            listView?.post {
                                adapter?.notifyDataSetChanged()
                            }
                        }
                    },
                )
            }
        }
    }

    private fun removeCheat(
        position: Int,
        cheatitem: CheatItem,
    ) {
        val dialog =
            MaterialAlertDialogBuilder(requireContext())
                .setMessage(getString(R.string.are_you_sure_to_delete) + cheatitem.description + "?")
                .setPositiveButton(
                    R.string.yes,
                    DialogInterface.OnClickListener { _, _ ->
                        if (database == null) {
                            return@OnClickListener
                        }
                        // 共有チートから削除
                        database!!.child(cheatitem.key).removeValue()

                        // ローカルリストから位置ベースで削除してUIを即座に更新
                        if (position >= 0 && position < (items?.size ?: 0)) {
                            items?.removeAt(position)
                            listView?.post {
                                adapter?.notifyItemRemoved(position)
                            }
                        }

                        // Clear shared_key in own local cheats
                        val userId = AuthState.realUser()?.uid
                        if (userId != null) {
                            val baseref = FirebaseDatabase.getInstance().reference
                            val localUrl = "/user-posts/$userId/cheat/$gameCode"
                            val localDb = baseref.child(localUrl)
                            localDb
                                .orderByChild("shared_key")
                                .equalTo(cheatitem.key)
                                .addListenerForSingleValueEvent(
                                    object : com.google.firebase.database.ValueEventListener {
                                        override fun onDataChange(snapshot: com.google.firebase.database.DataSnapshot) {
                                            for (child in snapshot.children) {
                                                child.ref.child("shared_key").setValue("")
                                            }
                                        }

                                        override fun onCancelled(error: com.google.firebase.database.DatabaseError) {
                                            Log.e(TAG, "Failed to clear shared_key: ${error.message}")
                                        }
                                    },
                                )
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

    override fun moveCursor(direction: Int) {
        adapter?.moveCursor(direction)
    }

    override fun selectCurrentItem() {
        adapter?.selectFocused()
    }

    companion object {
        private const val TAG = "CloudCheatItemFragment"

        @JvmStatic
        fun newInstance(
            gameid: String?,
            columnCount: Int,
        ): CloudCheatItemFragment {
            val fragment = CloudCheatItemFragment()
            val args = Bundle()
            args.putString(ARG_GAME_ID, gameid)
            args.putInt(ARG_COLUMN_COUNT, columnCount)
            fragment.arguments = args
            return fragment
        }
    }
}
