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
package org.uoyabause.android

import android.app.AlertDialog
import android.os.Bundle
import android.view.KeyEvent
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.ItemTouchHelper
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import org.devmiyax.yabasanshiro.R
import java.io.File
import java.util.ArrayList
import java.util.Collections
import java.util.Comparator
import java.util.Date

class StateItem {
    @JvmField
    var filename: String

    @JvmField
    var imageFilename: String

    @JvmField
    var savedate: Date? = null

    constructor() {
        filename = ""
        imageFilename = ""
    }

    constructor(filename: String, imageFilename: String, savedate: Date?) {
        this.filename = filename
        this.imageFilename = imageFilename
        this.savedate = savedate
    }
}

internal class StateItemComparator : Comparator<StateItem> {
    override fun compare(
        p1: StateItem,
        p2: StateItem,
    ): Int {
        val diff = p1.savedate!!.compareTo(p2.savedate)
        return if (diff > 0) {
            -1
        } else {
            1
        }
    }
}

internal class StateItemInvComparator : Comparator<StateItem> {
    override fun compare(
        p1: StateItem,
        p2: StateItem,
    ): Int {
        val diff = p1.savedate!!.compareTo(p2.savedate)
        return if (diff > 0) {
            1
        } else {
            -1
        }
    }
}

class StateListFragment :
    Fragment(),
    StateItemAdapter.OnItemClickListener,
    View.OnKeyListener {
    protected var mRecyclerView: RecyclerView? = null
    protected var mLayoutManager: RecyclerView.LayoutManager? = null
    protected lateinit var emptyView: View
    protected var mAdapter: StateItemAdapter? = null
    lateinit var stateItems: ArrayList<StateItem>
    var mHelper: ItemTouchHelper? = null
    var mSelectedItem = 0
    protected var basepath = ""

    fun setBasePath(basepath: String) {
        this.basepath = basepath
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        stateItems = ArrayList()
        val folder = File(basepath)
        val listOfFiles = folder.listFiles() ?: return
        for (i in listOfFiles.indices) {
            if (listOfFiles[i].isFile) {
                val filename = listOfFiles[i].name
                val point = filename.lastIndexOf(".")
                if (point != -1) {
                    val ext = filename.substring(point, point + 4)
                    if (ext == ".yss") {
                        val item = StateItem()
                        item.filename = "$basepath/$filename"
                        item.imageFilename =
                            basepath + "/" + filename.substring(0, point) + ".png"
                        item.savedate = Date(listOfFiles[i].lastModified())
                        stateItems.add(item)
                    }
                }
                // System.out.println("File " + listOfFiles[i].getName());
            } else if (listOfFiles[i].isDirectory) {
                // System.out.println("Directory " + listOfFiles[i].getName());
            }
        }

        if (stateItems.size > 1) {
            Collections.sort(stateItems, StateItemComparator())
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View? {
        val rootView = inflater.inflate(R.layout.state_list_frag, container, false)
        rootView.tag = TAG
        emptyView = rootView.findViewById(R.id.textView_empty)
        mRecyclerView = rootView.findViewById<View>(R.id.recyclerView) as RecyclerView
        if (stateItems.size == 0) {
            mRecyclerView!!.visibility = View.GONE
            return rootView
        }
        emptyView.setVisibility(View.GONE)
        mAdapter = StateItemAdapter()
        mAdapter!!.setStateItems(stateItems)
        mAdapter!!.setOnItemClickListener(this)
        mRecyclerView!!.adapter = mAdapter
/*
        var scrollPosition = 0
        // If a layout manager has already been set, get current scroll position.
        if (mRecyclerView!!.layoutManager != null) {
            scrollPosition = (mRecyclerView!!.layoutManager as LinearLayoutManager?)
                ?.findFirstCompletelyVisibleItemPosition() ?: 0
        }
 */
        mLayoutManager = LinearLayoutManager(activity)
        mRecyclerView!!.layoutManager = mLayoutManager
        selectItem(0)
        rootView.isFocusableInTouchMode = true
        rootView.requestFocus()
        rootView.setOnKeyListener(this)
/*
        val callback: ItemTouchHelper.SimpleCallback = object :
            ItemTouchHelper.SimpleCallback(0, ItemTouchHelper.LEFT or ItemTouchHelper.RIGHT) {
            override fun onMove(
                recyclerView: RecyclerView,
                viewHolder: RecyclerView.ViewHolder,
                target: RecyclerView.ViewHolder
            ): Boolean {
                return false
            }

            override fun onSwiped(viewHolder: RecyclerView.ViewHolder, direction: Int) {

                // 横にスワイプされたら要素を消す
                val swipedPosition = viewHolder.adapterPosition
                mAdapter!!.remove(swipedPosition)
                if (mAdapter!!.itemCount == 0) {
                    emptyView.setVisibility(View.VISIBLE)
                    mRecyclerView!!.visibility = View.GONE
                }
            }
        }
 */
        mHelper =
            ItemTouchHelper(
                object : ItemTouchHelper.Callback() {
                    override fun getMovementFlags(
                        recyclerView: RecyclerView,
                        viewHolder: RecyclerView.ViewHolder,
                    ): Int = makeFlag(
                        ItemTouchHelper.ACTION_STATE_IDLE,
                        ItemTouchHelper.RIGHT,
                    ) or
                        makeFlag(
                            ItemTouchHelper.ACTION_STATE_SWIPE,
                            ItemTouchHelper.LEFT or ItemTouchHelper.RIGHT,
                        ) or
                        makeFlag(
                            ItemTouchHelper.ACTION_STATE_DRAG,
                            ItemTouchHelper.DOWN or ItemTouchHelper.UP,
                        )

                    // ドラッグで場所を移動した際の処理を記述します
                    override fun onMove(
                        recyclerView: RecyclerView,
                        viewHolder: RecyclerView.ViewHolder,
                        viewHolder1: RecyclerView.ViewHolder,
                    ): Boolean {
                        // ((StateItemAdapter) recyclerView.getAdapter()).move(viewHolder.getAdapterPosition(), viewHolder1.getAdapterPosition());
                        return true
                    }

                    // 選択ステータスが変更された場合の処理を指定します
                    // この例ではAdapterView内のcontainerViewを表示にしています
                    // containerViewには背景色を指定しており、ドラッグが開始された際に見やすくなるようにしています
                    override fun onSelectedChanged(
                        viewHolder: RecyclerView.ViewHolder?,
                        actionState: Int,
                    ) {
                        super.onSelectedChanged(viewHolder, actionState)

                        // if (actionState == ItemTouchHelper.ACTION_STATE_DRAG)
                        //   ((StateItemAdapter.holder) viewHolder).container.setVisibility(View.VISIBLE);
                    }

                    // 選択が終わった時（Dragが終わった時など）の処理を指定します
                    // 今回はアイテムをDropした際にcontainerViewを非表示にして通常表示に戻しています
                    override fun clearView(
                        recyclerView: RecyclerView,
                        viewHolder: RecyclerView.ViewHolder,
                    ) {
                        super.clearView(recyclerView, viewHolder)
                        // ((StateItemAdapter.holder) viewHolder).container.setVisibility(View.GONE);
                    }

                    // Swipeされた際の処理です。
                    //
                    override fun onSwiped(
                        viewHolder: RecyclerView.ViewHolder,
                        i: Int,
                    ) {
                        (mRecyclerView!!.adapter as StateItemAdapter?)!!.remove(viewHolder.bindingAdapterPosition)
                        if (mAdapter!!.itemCount == 0) {
                            emptyView.setVisibility(View.VISIBLE)
                            mRecyclerView!!.visibility = View.GONE
                        }
                    }
                },
            )
        mHelper!!.attachToRecyclerView(mRecyclerView)
        mRecyclerView!!.addItemDecoration(mHelper!!)
        return rootView
    }

    override fun onItemClick(
        adapter: StateItemAdapter?,
        position: Int,
        item: StateItem?,
    ) {
        val main = activity as Yabause?
        main?.loadState(item?.filename)
    }

    fun selectPrevious() {
        if (mSelectedItem < 1) return
        selectItem(mSelectedItem - 1)
    }

    fun selectNext() {
        if (mSelectedItem >= mAdapter!!.itemCount - 1) return
        selectItem(mSelectedItem + 1)
    }

    private fun selectItem(position: Int) {
        var lposition = position
        if (lposition < 0) lposition = 0
        if (lposition >= mAdapter!!.itemCount) lposition = mAdapter!!.itemCount - 1
        val pre = mSelectedItem
        mSelectedItem = lposition
        mAdapter!!.setSelected(mSelectedItem)
        mAdapter!!.notifyItemChanged(pre)
        mAdapter!!.notifyItemChanged(mSelectedItem)
        mRecyclerView!!.stopScroll()
        mRecyclerView!!.smoothScrollToPosition(lposition)

/*
        mRecyclerView.post(new Runnable() {
            @Override
            public void run() {
                View v;
                for (int i = 0 ; i< mAdapter.getItemCount() ; ++i){
                    v = mLayoutManager.findViewByPosition(i);
                    if (v != null) {
                        v.setSelected(i == mSelectedItem);
                     }
                }
            }
        });
*/
    }

    override fun onKey(
        v: View,
        keyCode: Int,
        event: KeyEvent,
    ): Boolean {
        if (event.action != KeyEvent.ACTION_DOWN) {
            return false
        }
        if (stateItems.size == 0) {
            return false
        }
        when (keyCode) {
            KeyEvent.KEYCODE_MEDIA_PLAY, KeyEvent.KEYCODE_MEDIA_PAUSE, KeyEvent.KEYCODE_SPACE, KeyEvent.KEYCODE_BUTTON_A -> {
                val main = activity as Yabause?
                main?.loadState(stateItems[mSelectedItem].filename)
            }
            KeyEvent.KEYCODE_BUTTON_X ->
                AlertDialog
                    .Builder(activity)
                    .setMessage(getString(R.string.delete_confirm))
                    .setPositiveButton(getString(R.string.ok)) { _, _ ->
                        mAdapter!!.remove(mSelectedItem)
                        if (mAdapter!!.itemCount == 0) {
                            emptyView.visibility = View.VISIBLE
                            mRecyclerView!!.visibility = View.GONE
                        } else {
                            var newsel = mSelectedItem
                            if (newsel >= mAdapter!!.itemCount - 1) {
                                newsel -= 1
                            }
                            selectItem(newsel)
                        }
                    }.setNegativeButton(getString(R.string.cancel), null)
                    .show()
            KeyEvent.KEYCODE_DPAD_UP -> {
                selectPrevious()
                return true
            }
            KeyEvent.KEYCODE_DPAD_DOWN -> {
                selectNext()
                return true
            }
        }
        return false
    }

    override fun onResume() {
        super.onResume()
    }

    companion object {
        const val TAG = "StateListFragment"

        fun checkMaxFileCount(basepath: String) {
            val stateItems = ArrayList<StateItem>()
            val folder = File(basepath)
            val listOfFiles = folder.listFiles() ?: return
            for (i in listOfFiles.indices) {
                if (listOfFiles[i].isFile) {
                    val filename = listOfFiles[i].name
                    val point = filename.lastIndexOf(".")
                    if (point != -1) {
                        val ext = filename.substring(point, point + 4)
                        if (ext == ".yss") {
                            val item = StateItem()
                            item.filename = "$basepath/$filename"
                            item.imageFilename =
                                basepath + "/" + filename.substring(0, point) + ".png"
                            item.savedate = Date(listOfFiles[i].lastModified())
                            stateItems.add(item)
                        }
                    }
                }
            }
            val filecount = stateItems.size
            if (filecount > 10) {
                for (i in 0 until filecount - 10) {
                    Collections.sort(stateItems, StateItemInvComparator())
                    val saveFile = File(stateItems[i].filename)
                    saveFile.delete()
                    val imgFile = File(stateItems[i].imageFilename)
                    imgFile.delete()
                }
            }
        }
    }
}
