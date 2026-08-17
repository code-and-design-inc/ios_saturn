package org.uoyabause.android.backup

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.RadioButton
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import org.devmiyax.yabasanshiro.R
import java.text.SimpleDateFormat
import java.util.Locale

/**
 * Adapter for displaying a list of backed up games in the replacement dialog
 */
class BackupReplaceAdapter(
    private val backupList: List<GameBackupManager.BackupGameInfo>,
) : RecyclerView.Adapter<BackupReplaceAdapter.ViewHolder>() {
    private var selectedPosition = -1
    private var onItemSelectedListener: ((GameBackupManager.BackupGameInfo?) -> Unit)? = null

    /**
     * Set a listener to be notified when an item is selected
     */
    fun setOnItemSelectedListener(listener: (GameBackupManager.BackupGameInfo?) -> Unit) {
        onItemSelectedListener = listener
    }

    /**
     * Get the currently selected backup item, or null if none is selected
     */
    fun getSelectedBackup(): GameBackupManager.BackupGameInfo? = if (selectedPosition >= 0 && selectedPosition < backupList.size) {
        backupList[selectedPosition]
    } else {
        null
    }

    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int,
    ): ViewHolder {
        val view =
            LayoutInflater
                .from(parent.context)
                .inflate(R.layout.item_backup_replace, parent, false)
        return ViewHolder(view)
    }

    override fun onBindViewHolder(
        holder: ViewHolder,
        position: Int,
    ) {
        val backup = backupList[position]

        // Set game title
        holder.gameTitleView.text = backup.gameTitle

        // Format the date
        val dateFormat = SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.getDefault())
        val formattedDate = dateFormat.format(backup.uploadedAt)
        holder.gameDetailsView.text =
            holder.itemView.context.getString(
                R.string.backup_date,
                formattedDate,
            )

        // Set radio button state
        holder.radioButton.isChecked = position == selectedPosition

        // Set click listeners
        val clickListener =
            View.OnClickListener {
                val previousSelected = selectedPosition
                selectedPosition = holder.bindingAdapterPosition

                // Notify previous item and current item changed
                if (previousSelected != -1) {
                    notifyItemChanged(previousSelected)
                }
                notifyItemChanged(selectedPosition)

                // Notify listener
                onItemSelectedListener?.invoke(getSelectedBackup())
            }

        holder.itemView.setOnClickListener(clickListener)
        holder.radioButton.setOnClickListener(clickListener)
    }

    override fun getItemCount(): Int = backupList.size

    inner class ViewHolder(
        view: View,
    ) : RecyclerView.ViewHolder(view) {
        val radioButton: RadioButton = view.findViewById(R.id.radio_select)
        val gameTitleView: TextView = view.findViewById(R.id.game_title)
        val gameDetailsView: TextView = view.findViewById(R.id.game_details)
    }
}
