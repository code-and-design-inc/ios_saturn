package org.uoyabause.android.backup

import android.app.Dialog
import android.content.Context
import android.os.Bundle
import android.view.LayoutInflater
import android.widget.Button
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import org.devmiyax.yabasanshiro.R

/**
 * Dialog for selecting a backup to replace when the backup limit is reached
 */
class BackupReplaceDialog(
    context: Context,
    private val backupList: List<GameBackupManager.BackupGameInfo>,
    private val onReplaceSelected: (GameBackupManager.BackupGameInfo) -> Unit,
) : Dialog(context) {
    private lateinit var adapter: BackupReplaceAdapter
    private lateinit var replaceButton: Button

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Inflate the dialog layout
        val view = LayoutInflater.from(context).inflate(R.layout.dialog_backup_replace, null)
        setContentView(view)

        // Set up the RecyclerView
        val recyclerView = view.findViewById<RecyclerView>(R.id.backup_list)
        recyclerView.layoutManager = LinearLayoutManager(context)
        adapter = BackupReplaceAdapter(backupList)
        recyclerView.adapter = adapter

        // Set up buttons
        replaceButton = view.findViewById(R.id.btn_replace)
        val cancelButton = view.findViewById<Button>(R.id.btn_cancel)

        // Set up listeners
        adapter.setOnItemSelectedListener { backup ->
            replaceButton.isEnabled = backup != null
        }

        replaceButton.setOnClickListener {
            val selectedBackup = adapter.getSelectedBackup()
            if (selectedBackup != null) {
                onReplaceSelected(selectedBackup)
                dismiss()
            } else {
                Toast.makeText(context, R.string.no_game_selected, Toast.LENGTH_SHORT).show()
            }
        }

        cancelButton.setOnClickListener {
            dismiss()
        }

        // Set dialog title and properties
        setTitle(R.string.backup_limit_dialog_title)
        setCancelable(true)
        setCanceledOnTouchOutside(true)
    }

    companion object {
        /**
         * Show the backup replace dialog
         * @param context The context
         * @param backupList The list of current backups
         * @param onReplaceSelected Callback when a backup is selected for replacement
         */
        fun show(
            context: Context,
            backupList: List<GameBackupManager.BackupGameInfo>,
            onReplaceSelected: (GameBackupManager.BackupGameInfo) -> Unit,
        ) {
            if (backupList.isEmpty()) {
                // This shouldn't happen, but just in case
                AlertDialog
                    .Builder(context)
                    .setTitle(R.string.error)
                    .setMessage(R.string.backup_limit_reached)
                    .setPositiveButton(R.string.ok, null)
                    .show()
                return
            }

            val dialog = BackupReplaceDialog(context, backupList, onReplaceSelected)
            dialog.show()
        }
    }
}
