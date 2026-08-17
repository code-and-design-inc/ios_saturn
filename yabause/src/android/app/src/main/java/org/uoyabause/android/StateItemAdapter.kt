package org.uoyabause.android

import android.content.Context
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import androidx.cardview.widget.CardView
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.RecyclerView
import com.bumptech.glide.Glide
import com.bumptech.glide.load.resource.bitmap.CenterCrop
import org.devmiyax.yabasanshiro.R
import java.io.File
import java.text.SimpleDateFormat
import java.util.ArrayList

class StateItemAdapter :
    RecyclerView.Adapter<StateItemAdapter.ViewHolder>(),
    View.OnClickListener {
    private val yabause: Yabause? = null
    private var stateItems: ArrayList<StateItem>? = null
    private var mRecycler: RecyclerView? = null
    private var selectpos = 0

    fun setSelected(pos: Int) {
        selectpos = pos
    }

    fun setStateItems(state_items: ArrayList<StateItem>?) {
        stateItems = state_items
    }

    override fun onAttachedToRecyclerView(recyclerView: RecyclerView) {
        super.onAttachedToRecyclerView(recyclerView)
        mRecycler = recyclerView
    }

    override fun onDetachedFromRecyclerView(recyclerView: RecyclerView) {
        super.onDetachedFromRecyclerView(recyclerView)
        mRecycler = null
    }

    class ViewHolder(
        v: View,
    ) : RecyclerView.ViewHolder(v) {
        val textView: TextView
        val imageView: ImageView

        // public Toolbar getToolBar() {
        val cardView: CardView

        //    return toolbar;
        // }
        // private final Toolbar toolbar;
        init {
            v.isClickable = true
            textView = v.findViewById<View>(R.id.textView) as TextView
            /*
            toolbar = (Toolbar) v.findViewById(R.id.card_toolbar);
            if (toolbar != null) {
                // inflate your menu
                toolbar.inflateMenu(R.menu.state);
                toolbar.setOnMenuItemClickListener(new Toolbar.OnMenuItemClickListener() {
                    @Override
                    public boolean onMenuItemClick(MenuItem menuItem) {
                        return true;
                    }
                });
            }
*/
            imageView = v.findViewById<View>(R.id.imageView) as ImageView
            cardView = v.findViewById<View>(R.id.cardview) as CardView
        }
    }

    private var mListener: OnItemClickListener? = null

    fun setOnItemClickListener(listener: OnItemClickListener?) {
        mListener = listener
    }

    interface OnItemClickListener {
        fun onItemClick(
            adapter: StateItemAdapter?,
            position: Int,
            item: StateItem?,
        )
    }

    override fun onClick(view: View) {
        if (mRecycler == null) {
            return
        }
        if (mListener != null) {
            val position = mRecycler!!.getChildAdapterPosition(view)
            val item = stateItems!![position]
            mListener!!.onItemClick(this, position, item)
        }
    }

    override fun onCreateViewHolder(
        viewGroup: ViewGroup,
        viewType: Int,
    ): ViewHolder {
        // Create a new view.
        val v =
            LayoutInflater
                .from(viewGroup.context)
                .inflate(R.layout.state_item, viewGroup, false)
        v.setOnClickListener(this)
        v.isFocusable = true
        v.isFocusableInTouchMode = true
        return ViewHolder(v)
    }

    fun dp2px(
        cx: Context,
        dp: Int,
    ): Int {
        val displaymetrics = cx.resources.displayMetrics
        return (dp * displaymetrics.density + 0.5f).toInt()
    }

    override fun onBindViewHolder(
        viewHolder: ViewHolder,
        position: Int,
    ) {
        Log.d(TAG, "Element $position set.")
        viewHolder.textView.text =
            stateItems!![position].savedate?.let {
                SimpleDateFormat(DATE_PATTERN).format(it)
            }
        // viewHolder.getToolBar().setTitle(new SimpleDateFormat(DATE_PATTERN).format(stateItems.get(position).savedate));
        val cx = viewHolder.imageView.context
        Glide
            .with(cx)
            .load(File(stateItems!![position].imageFilename))
            .transform(CenterCrop())
            .into(viewHolder.imageView)

        // .override(dp2px(cx,220),dp2px(cx,220)*3/4)
        if (selectpos == position) {
            viewHolder.cardView.setBackgroundColor(ContextCompat.getColor(cx, R.color.selected_background))
            viewHolder.cardView.isSelected = true
        } else {
            viewHolder.cardView.setBackgroundColor(ContextCompat.getColor(cx, R.color.default_background))
            viewHolder.cardView.isSelected = false
        }
    }

    override fun getItemCount(): Int = stateItems!!.size

    fun remove(position: Int) {
        var file = File(stateItems!![position].filename)
        file.delete()
        file = File(stateItems!![position].imageFilename)
        file.delete()
        stateItems!!.removeAt(position)
        notifyItemRemoved(position)
    }

    companion object {
        private const val TAG = "CustomAdapter"
        const val DATE_PATTERN = "yyyy/MM/dd HH:mm:ss"
    }
}
