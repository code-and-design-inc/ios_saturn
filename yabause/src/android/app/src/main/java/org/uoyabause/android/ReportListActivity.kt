/*  Copyright 2025 devMiyax(smiyaxdev@gmail.com)

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

import android.os.Bundle
import android.util.Log
import android.view.View
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.RecyclerView
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout
import com.google.android.material.appbar.MaterialToolbar
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.firebase.firestore.FirebaseFirestore
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.tasks.await
import kotlinx.coroutines.withContext
import org.devmiyax.yabasanshiro.R

/**
 * Activity to display list of reports for a specific game
 */
class ReportListActivity : AppCompatActivity() {
    private lateinit var toolbar: MaterialToolbar
    private lateinit var recyclerView: RecyclerView
    private lateinit var progressBar: ProgressBar
    private lateinit var emptyStateLayout: LinearLayout
    private lateinit var swipeRefreshLayout: SwipeRefreshLayout

    private lateinit var productNumber: String
    private lateinit var gameTitle: String
    private lateinit var filePath: String
    private lateinit var isoFilePath: String

    private val reports = mutableListOf<ReportData>()
    private lateinit var adapter: ReportListAdapter

    // Store original preferences to restore after reproduction
    private var originalPreferences: Map<String, String>? = null
    private var isReproducing = false

    companion object {
        const val EXTRA_PRODUCT_NUMBER = "extra_product_number"
        const val EXTRA_GAME_TITLE = "extra_game_title"
        const val EXTRA_FILE_PATH = "extra_file_path"
        const val EXTRA_ISO_FILE_PATH = "extra_iso_file_path"
        private const val TAG = "ReportListActivity"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_report_list)

        // Get extras
        productNumber = intent.getStringExtra(EXTRA_PRODUCT_NUMBER) ?: run {
            Log.e(TAG, "Product number not provided")
            finish()
            return
        }

        gameTitle = intent.getStringExtra(EXTRA_GAME_TITLE) ?: "Unknown Game"
        filePath = intent.getStringExtra(EXTRA_FILE_PATH) ?: ""
        isoFilePath = intent.getStringExtra(EXTRA_ISO_FILE_PATH) ?: ""

        // Initialize views
        toolbar = findViewById(R.id.toolbar)
        recyclerView = findViewById(R.id.report_recycler_view)
        progressBar = findViewById(R.id.progress_bar)
        emptyStateLayout = findViewById(R.id.empty_state_layout)
        swipeRefreshLayout = findViewById(R.id.swipe_refresh_layout)

        // Setup toolbar
        toolbar.title = getString(R.string.report_list_title) + " - " + gameTitle
        toolbar.setNavigationOnClickListener {
            finish()
        }

        // Setup RecyclerView
        adapter =
            ReportListAdapter(reports) { report ->
                handleReportClick(report)
            }
        recyclerView.adapter = adapter

        // Setup swipe refresh
        swipeRefreshLayout.setOnRefreshListener {
            loadReports()
        }

        // Load reports
        loadReports()
    }

    /**
     * Loads reports from Firestore
     */
    private fun loadReports() {
        lifecycleScope.launch {
            try {
                // Show loading
                if (!swipeRefreshLayout.isRefreshing) {
                    progressBar.visibility = View.VISIBLE
                }
                emptyStateLayout.visibility = View.GONE
                recyclerView.visibility = View.GONE

                // Query Firestore
                val db = FirebaseFirestore.getInstance()

                // First, find the game document
                val gamesQuery =
                    db
                        .collection("games")
                        .whereEqualTo("product_number", productNumber)
                        .get()
                        .await()

                if (gamesQuery.isEmpty) {
                    // No game document found
                    withContext(Dispatchers.Main) {
                        progressBar.visibility = View.GONE
                        swipeRefreshLayout.isRefreshing = false
                        emptyStateLayout.visibility = View.VISIBLE
                        recyclerView.visibility = View.GONE
                    }
                    return@launch
                }

                // Get the game document
                val gameDoc = gamesQuery.documents[0]

                // Query both the signed-in ratings and the anonymous (guest) reports.
                val ratingsQuery =
                    gameDoc.reference
                        .collection("ratings")
                        .whereEqualTo("isVisible", true)
                        .get()
                        .await()
                val anonRatingsQuery =
                    gameDoc.reference
                        .collection("anon_ratings")
                        .whereEqualTo("isVisible", true)
                        .get()
                        .await()

                // Convert to ReportData objects and merge, newest first.
                val reportList =
                    (ratingsQuery.documents + anonRatingsQuery.documents)
                        .mapNotNull { doc ->
                            try {
                                doc.toObject(ReportData::class.java)
                            } catch (e: Exception) {
                                Log.e(TAG, "Error parsing report: ${doc.id}", e)
                                null
                            }
                        }.sortedByDescending { it.timestamp?.toDate()?.time ?: 0L }

                // Update UI
                withContext(Dispatchers.Main) {
                    progressBar.visibility = View.GONE
                    swipeRefreshLayout.isRefreshing = false

                    reports.clear()
                    reports.addAll(reportList)
                    adapter.notifyDataSetChanged()

                    if (reports.isEmpty()) {
                        emptyStateLayout.visibility = View.VISIBLE
                        recyclerView.visibility = View.GONE
                    } else {
                        emptyStateLayout.visibility = View.GONE
                        recyclerView.visibility = View.VISIBLE
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error loading reports", e)
                withContext(Dispatchers.Main) {
                    progressBar.visibility = View.GONE
                    swipeRefreshLayout.isRefreshing = false
                    Toast
                        .makeText(
                            this@ReportListActivity,
                            "Error loading reports: ${e.message}",
                            Toast.LENGTH_LONG,
                        ).show()

                    // Show empty state on error
                    emptyStateLayout.visibility = View.VISIBLE
                    recyclerView.visibility = View.GONE
                }
            }
        }
    }

    /**
     * Handles report item click
     */
    private fun handleReportClick(report: ReportData) {
        // Check if report is reproducible
        if (!report.isReproducible()) {
            Toast
                .makeText(
                    this,
                    R.string.report_not_reproducible,
                    Toast.LENGTH_SHORT,
                ).show()
            return
        }

        // Check if we have game file path
        if (filePath.isEmpty()) {
            Toast
                .makeText(
                    this,
                    "Game information not available",
                    Toast.LENGTH_SHORT,
                ).show()
            return
        }

        // Show confirmation dialog
        AlertDialog
            .Builder(this)
            .setTitle(R.string.reproduce_report)
            .setMessage(R.string.reproduce_report_confirm)
            .setPositiveButton(R.string.ok) { _, _ ->
                reproduceReport(report)
            }.setNegativeButton(R.string.cancel, null)
            .show()
    }

    /**
     * Creates a progress dialog using AlertDialog with custom layout
     */
    private fun createProgressDialog(message: String): Pair<AlertDialog, TextView> {
        val dialogView =
            LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                setPadding(48, 32, 48, 32)

                addView(
                    ProgressBar(this@ReportListActivity).apply {
                        isIndeterminate = true
                    },
                )

                addView(
                    TextView(this@ReportListActivity).apply {
                        text = message
                        setPadding(32, 0, 0, 0)
                        layoutParams =
                            LinearLayout
                                .LayoutParams(
                                    LinearLayout.LayoutParams.WRAP_CONTENT,
                                    LinearLayout.LayoutParams.WRAP_CONTENT,
                                ).apply {
                                    gravity = android.view.Gravity.CENTER_VERTICAL
                                }
                    },
                )
            }

        val messageTextView = dialogView.getChildAt(1) as TextView

        val dialog =
            MaterialAlertDialogBuilder(this)
                .setView(dialogView)
                .setCancelable(false)
                .create()

        return Pair(dialog, messageTextView)
    }

    /**
     * Downloads attachments and launches emulator to reproduce the report
     */
    private fun reproduceReport(report: ReportData) {
        // Show progress dialog using AlertDialog
        val (progressDialog, messageTextView) = createProgressDialog(getString(R.string.downloading_attachments))
        progressDialog.show()

        lifecycleScope.launch {
            try {
                val reproduceManager = ReportReproduceManager(this@ReportListActivity)

                // Download and prepare files
                val result =
                    reproduceManager.prepareReproduction(
                        reportData = report,
                        productNumber = productNumber,
                        progressListener =
                            object : ReportReproduceManager.DownloadProgressListener {
                                override fun onProgress(
                                    bytesTransferred: Long,
                                    totalBytes: Long,
                                ) {
                                    val progress = (bytesTransferred * 100 / totalBytes).toInt()
                                    lifecycleScope.launch(Dispatchers.Main) {
                                        messageTextView.text = "${getString(R.string.downloading_attachments)}... $progress%"
                                    }
                                }

                                override fun onComplete() {
                                    Log.d(TAG, "Download completed")
                                }

                                override fun onError(error: Exception) {
                                    Log.e(TAG, "Download error", error)
                                }
                            },
                    )

                // Dismiss progress dialog
                withContext(Dispatchers.Main) {
                    progressDialog.dismiss()
                }

                if (result.success) {
                    // Show info dialog before launching
                    withContext(Dispatchers.Main) {
                        val attachmentInfo =
                            buildString {
                                append(getString(R.string.download_complete))
                                append("\n\n")
                                if (result.savestateFile != null) {
                                    append("• ${getString(R.string.savestate)}: ${result.savestateFile.name}\n")
                                }
                                if (result.memoryFile != null) {
                                    append("• ${getString(R.string.memory)}: ${result.memoryFile.name}\n")
                                }
                                if (result.screenshotFile != null) {
                                    append("• ${getString(R.string.screenshot)}: ${result.screenshotFile.name}\n")
                                }
                            }

                        AlertDialog
                            .Builder(this@ReportListActivity)
                            .setTitle(R.string.ready_to_reproduce)
                            .setMessage(attachmentInfo)
                            .setPositiveButton(R.string.start_emulator) { _, _ ->
                                // Apply reproduction preferences
                                if (result.preferences != null) {
                                    originalPreferences = reproduceManager.applyReproductionPreferences(productNumber, result.preferences)
                                    isReproducing = true
                                    Log.d(TAG, "Applied ${result.preferences.size} reproduction preferences (gameCode: $productNumber)")
                                }

                                // Create temporary GameInfo object for launching emulator
                                val tempGameInfo =
                                    GameInfo(
                                        file_path = filePath,
                                        iso_file_path = isoFilePath,
                                        game_title = gameTitle,
                                        product_number = productNumber,
                                    )

                                // Launch emulator
                                val launchIntent =
                                    reproduceManager.createLaunchIntent(
                                        tempGameInfo,
                                        result.savestateFile,
                                        result.memoryFile,
                                    )
                                startActivity(launchIntent)
                            }.setNegativeButton(R.string.cancel, null)
                            .show()
                    }
                } else {
                    // Show error
                    withContext(Dispatchers.Main) {
                        Toast
                            .makeText(
                                this@ReportListActivity,
                                result.errorMessage ?: "Failed to download attachments",
                                Toast.LENGTH_LONG,
                            ).show()
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error reproducing report", e)
                withContext(Dispatchers.Main) {
                    progressDialog.dismiss()
                    Toast
                        .makeText(
                            this@ReportListActivity,
                            "Error: ${e.message}",
                            Toast.LENGTH_LONG,
                        ).show()
                }
            }
        }
    }

    override fun onResume() {
        super.onResume()

        // Restore original preferences when returning from emulator
        if (isReproducing && originalPreferences != null) {
            val reproduceManager = ReportReproduceManager(this)
            reproduceManager.restoreOriginalPreferences(productNumber, originalPreferences!!)
            Log.d(TAG, "Restored ${originalPreferences!!.size} original preferences (gameCode: $productNumber)")

            // Reset flags
            originalPreferences = null
            isReproducing = false
        }
    }

    override fun onDestroy() {
        super.onDestroy()

        // Safety: restore preferences if activity is destroyed while reproducing
        if (isReproducing && originalPreferences != null) {
            val reproduceManager = ReportReproduceManager(this)
            reproduceManager.restoreOriginalPreferences(productNumber, originalPreferences!!)
            Log.d(TAG, "Restored preferences on destroy (gameCode: $productNumber)")
        }
    }
}
