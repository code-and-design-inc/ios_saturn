package org.uoyabause.android.backup

import android.content.Context
import android.net.Uri
import android.util.Log
import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.firestore.FirebaseFirestore
import com.google.firebase.storage.FirebaseStorage
import com.google.firebase.storage.StorageException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.tasks.await
import kotlinx.coroutines.withContext
import org.uoyabause.android.GameInfo
import org.uoyabause.android.YabauseStorage
import org.uoyabause.android.util.FileHashUtil
import java.io.File
import java.io.FileInputStream
import java.util.Date

/**
 * Manages game ROM backup and restoration to/from Firebase Cloud Storage
 */
class GameBackupManager(
    private val context: Context,
) {
    companion object {
        private const val TAG = "GameBackupManager"
        private const val BACKUP_LIMIT = 1 // Maximum number of games a user can backup
    }

    private val auth = FirebaseAuth.getInstance()
    private val storage = FirebaseStorage.getInstance()
    private val firestore = FirebaseFirestore.getInstance()

    /**
     * Calculate SHA256 hash of a file
     * @param file The file to calculate hash for
     * @return The SHA256 hash as a hex string
     */
    suspend fun calculateSHA256(file: File): String = FileHashUtil.calculateSHA256(file)

    /**
     * Calculate SHA256 hash for a content URI
     * @param uri The content URI to calculate hash for
     * @return The SHA256 hash as a hex string
     */
    suspend fun calculateSHA256(uri: Uri): String = FileHashUtil.calculateSHA256(context, uri)

    /**
     * Check if the user has reached their backup limit and return the list of current backups
     * @return A Pair where the first value is true if the user has reached their backup limit, and the second value is the list of current backups
     */
    suspend fun checkBackupLimitWithList(): Pair<Boolean, List<BackupGameInfo>> =
        withContext(Dispatchers.IO) {
            val currentUser = auth.currentUser ?: return@withContext Pair(true, emptyList())

            try {
                // Get the list of backed up games
                val backupsList = getBackedUpGames()

                // Check if the limit is reached
                val limitReached = backupsList.size >= BACKUP_LIMIT

                return@withContext Pair(limitReached, backupsList)
            } catch (e: Exception) {
                // For errors, log and return true as a safety measure with empty list
                Log.e(TAG, "Error checking backup limit with list: ${e.message}")
                return@withContext Pair(true, emptyList())
            }
        }

    /**
     * Check if the user has reached their backup limit
     * @return true if the user has reached their backup limit, false otherwise
     */
    suspend fun hasReachedBackupLimit(): Boolean =
        withContext(Dispatchers.IO) {
            val (limitReached, _) = checkBackupLimitWithList()
            return@withContext limitReached
        }

    /**
     * Backup a game ROM to Firebase Cloud Storage
     * @param gameInfo The GameInfo object for the backup
     * @return A Result object indicating success or failure with a message
     */
    suspend fun backupGame(gameInfo: GameInfo): Result =
        withContext(Dispatchers.IO) {
            val currentUser = auth.currentUser ?: return@withContext Result(false, "User not signed in")

            try {
                // Check if user has reached backup limit and get the list of current backups
                val (limitReached, backupsList) = checkBackupLimitWithList()

                // If limit is reached, return a special result with the list of current backups
                if (limitReached) {
                    return@withContext Result(
                        success = false,
                        message = "Backup limit reached (maximum $BACKUP_LIMIT game)",
                        backupList = backupsList,
                        limitReached = true,
                    )
                }

                // Proceed with normal backup
                return@withContext performBackup(gameInfo, null)
            } catch (e: Exception) {
                Log.e(TAG, "Error backing up game: ${e.message}")
                return@withContext Result(false, "Error: ${e.message}")
            }
        }

    /**
     * Replace an existing backup with a new game
     * @param gameInfo The GameInfo object for the new game to backup
     * @param backupToReplace The BackupGameInfo object to replace
     * @return A Result object indicating success or failure with a message
     */
    suspend fun replaceBackup(
        gameInfo: GameInfo,
        backupToReplace: BackupGameInfo,
    ): Result =
        withContext(Dispatchers.IO) {
            val currentUser = auth.currentUser ?: return@withContext Result(false, "User not signed in")

            try {
                // First delete the existing backup
                val deleteResult = deleteBackup(backupToReplace)
                if (!deleteResult.success) {
                    return@withContext Result(false, "Failed to delete existing backup: ${deleteResult.message}")
                }

                // Then perform the new backup
                return@withContext performBackup(gameInfo, backupToReplace.id)
            } catch (e: Exception) {
                Log.e(TAG, "Error replacing backup: ${e.message}")
                return@withContext Result(false, "Error replacing backup: ${e.message}")
            }
        }

    /**
     * Internal method to perform the actual backup operation
     * @param gameInfo The GameInfo object for the game to backup
     * @param documentIdOverride Optional document ID to use instead of gameInfo.id
     * @return A Result object indicating success or failure with a message
     */
    private suspend fun performBackup(
        gameInfo: GameInfo,
        documentIdOverride: String?,
    ): Result =
        withContext(Dispatchers.IO) {
            val currentUser = auth.currentUser ?: return@withContext Result(false, "User not signed in")

            try {
                // Get the file path or URI
                val filePath = gameInfo.file_path
                val fileUri =
                    if (filePath.startsWith("content://")) {
                        Uri.parse(filePath)
                    } else {
                        Uri.fromFile(File(filePath))
                    }

                // Calculate hash
                val hash =
                    if (filePath.startsWith("content://")) {
                        calculateSHA256(Uri.parse(filePath))
                    } else {
                        calculateSHA256(File(filePath))
                    }

                // Ensure user document exists in Firestore
                val userDocRef = firestore.collection("users").document(currentUser.uid)

                // Create user document if it doesn't exist
                try {
                    val userDoc = userDocRef.get().await()
                    if (!userDoc.exists()) {
                        // Create the user document with basic info
                        val userData =
                            hashMapOf(
                                "uid" to currentUser.uid,
                                "email" to (currentUser.email ?: ""),
                                "displayName" to (currentUser.displayName ?: ""),
                                "photoUrl" to (currentUser.photoUrl?.toString() ?: ""),
                                "createdAt" to Date(),
                            )
                        userDocRef.set(userData).await()
                        Log.d(TAG, "Created user document for ${currentUser.uid}")
                    }
                } catch (e: Exception) {
                    Log.w(TAG, "Error checking/creating user document: ${e.message}")
                    // Continue anyway, as the document might be created during the backup operation
                }

                // Check if this hash already exists in the user's backups
                try {
                    val existingBackup =
                        userDocRef
                            .collection("backups")
                            .whereEqualTo("hash", hash)
                            .get()
                            .await()

                    if (!existingBackup.isEmpty) {
                        return@withContext Result(true, "Game already backed up")
                    }
                } catch (e: Exception) {
                    // If there's an error checking for existing backups (like collection doesn't exist),
                    // we'll just continue with the backup process
                    Log.d(TAG, "Error checking for existing backups: ${e.message}")
                }

                // Upload file to Firebase Storage
                val storageRef =
                    storage.reference
                        .child(currentUser.uid)
                        .child("game_backups")
                        .child(hash)

                // Get input stream
                val inputStream =
                    if (filePath.startsWith("content://")) {
                        context.contentResolver.openInputStream(Uri.parse(filePath))
                    } else {
                        FileInputStream(File(filePath))
                    }

                // Upload file
                val uploadTask = storageRef.putStream(inputStream!!)
                val uploadResult = uploadTask.await()

                // Get download URL
                val downloadUrl = storageRef.downloadUrl.await().toString()

                // Save metadata to Firestore
                val backupData =
                    hashMapOf(
                        "filename" to (gameInfo.game_title + getFileExtension(filePath)),
                        "hash" to hash,
                        "uploadedAt" to Date(),
                        "size" to uploadResult.totalByteCount,
                        "gameTitle" to gameInfo.game_title,
                        "productNumber" to gameInfo.product_number,
                        "downloadUrl" to downloadUrl,
                    )

                // Create the backups collection and document
                // Use the provided document ID if available, otherwise use the game ID
                val documentId = documentIdOverride ?: gameInfo.id.toString()
                userDocRef
                    .collection("backups")
                    .document(documentId)
                    .set(backupData)
                    .await()

                return@withContext Result(true, "Game backed up successfully")
            } catch (e: Exception) {
                Log.e(TAG, "Error performing backup: ${e.message}")
                return@withContext Result(false, "Error: ${e.message}")
            }
        }

    /**
     * Get a list of backed up games for the current user
     * @return A list of BackupGameInfo objects
     */
    suspend fun getBackedUpGames(): List<BackupGameInfo> =
        withContext(Dispatchers.IO) {
            val currentUser = auth.currentUser ?: return@withContext emptyList()

            try {
                val backupsSnapshot =
                    firestore
                        .collection("users")
                        .document(currentUser.uid)
                        .collection("backups")
                        .get()
                        .await()

                // Process each backup document and verify it exists in Storage
                val validBackups = mutableListOf<BackupGameInfo>()
                val invalidBackupIds = mutableListOf<String>()

                for (document in backupsSnapshot.documents) {
                    val data = document.data ?: continue
                    val hash = data["hash"] as? String ?: continue

                    val backupInfo =
                        BackupGameInfo(
                            id = document.id,
                            filename = data["filename"] as? String ?: "",
                            hash = hash,
                            uploadedAt = data["uploadedAt"] as? Date ?: Date(),
                            size = (data["size"] as? Long) ?: 0L,
                            gameTitle = data["gameTitle"] as? String ?: "",
                            productNumber = data["productNumber"] as? String ?: "",
                            downloadUrl = data["downloadUrl"] as? String ?: "",
                        )

                    // Verify the file exists in Storage
                    try {
                        val storageRef =
                            storage.reference
                                .child(currentUser.uid)
                                .child("game_backups")
                                .child(hash)

                        // Just check metadata to see if file exists
                        storageRef.metadata.await()

                        // File exists, add to valid backups
                        validBackups.add(backupInfo)
                    } catch (e: StorageException) {
                        if (e.errorCode == -13010 || e.httpResultCode == 404) {
                            // File doesn't exist in Storage, mark for cleanup
                            Log.w(TAG, "Backup file doesn't exist in Storage but has Firestore record: $hash")
                            invalidBackupIds.add(document.id)
                        } else {
                            // For other errors, assume the file exists and add to valid backups
                            Log.e(TAG, "Error checking backup file existence: ${e.message}")
                            validBackups.add(backupInfo)
                        }
                    } catch (e: Exception) {
                        // For other errors, assume the file exists and add to valid backups
                        Log.e(TAG, "Error checking backup file existence: ${e.message}")
                        validBackups.add(backupInfo)
                    }
                }

                // Clean up invalid backup records in the background
                if (invalidBackupIds.isNotEmpty()) {
                    CoroutineScope(Dispatchers.IO).launch {
                        for (id in invalidBackupIds) {
                            try {
                                firestore
                                    .collection("users")
                                    .document(currentUser.uid)
                                    .collection("backups")
                                    .document(id)
                                    .delete()
                                    .await()
                                Log.d(TAG, "Cleaned up invalid backup record: $id")
                            } catch (e: Exception) {
                                Log.e(TAG, "Failed to clean up invalid backup record: $id, ${e.message}")
                            }
                        }
                    }
                }

                return@withContext validBackups
            } catch (e: Exception) {
                Log.e(TAG, "Error getting backed up games: ${e.message}")
                return@withContext emptyList()
            }
        }

    /**
     * Restore a game from Firebase Cloud Storage
     * @param backupGameInfo The BackupGameInfo object for the game to restore
     * @return A Result object indicating success or failure with a message
     */
    suspend fun restoreGame(backupGameInfo: BackupGameInfo): Result =
        withContext(Dispatchers.IO) {
            val currentUser = auth.currentUser ?: return@withContext Result(false, "User not signed in")

            try {
                // Get reference to the file in Firebase Storage
                val storageRef =
                    storage.reference
                        .child(currentUser.uid)
                        .child("game_backups")
                        .child(backupGameInfo.hash)

                // Create a local file to download to
                val localFile = File(YabauseStorage.storage.gamePath, backupGameInfo.filename)

                try {
                    // First check if the file exists in Storage
                    val metadata = storageRef.metadata.await()
                    Log.d(TAG, "File exists in Storage with size: ${metadata.sizeBytes} bytes")

                    // Download file
                    storageRef.getFile(localFile).await()

                    // Update game database
                    YabauseStorage.storage.generateGameDB(YabauseStorage.REFRESH_LEVEL_REBUILD)

                    return@withContext Result(true, "Game restored successfully")
                } catch (e: StorageException) {
                    // Handle specific Storage exceptions
                    if (e.errorCode == -13010 || e.httpResultCode == 404) {
                        Log.e(TAG, "File does not exist in Storage: ${backupGameInfo.hash}")

                        // Clean up the inconsistent Firestore record
                        try {
                            firestore
                                .collection("users")
                                .document(currentUser.uid)
                                .collection("backups")
                                .document(backupGameInfo.id)
                                .delete()
                                .await()
                            Log.d(TAG, "Deleted inconsistent backup record from Firestore")
                        } catch (firestoreEx: Exception) {
                            Log.e(TAG, "Failed to delete inconsistent backup record: ${firestoreEx.message}")
                        }

                        return@withContext Result(false, "The backup file no longer exists in cloud storage. The record has been removed.")
                    } else {
                        // Other storage exception
                        Log.e(TAG, "Storage error restoring game: ${e.message}, code: ${e.errorCode}")
                        return@withContext Result(false, "Storage error: ${e.message}")
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error restoring game: ${e.message}")
                return@withContext Result(false, "Error: ${e.message}")
            }
        }

    /**
     * Delete a backed up game from Firebase Cloud Storage
     * @param backupGameInfo The BackupGameInfo object for the game to delete
     * @return A Result object indicating success or failure with a message
     */
    suspend fun deleteBackup(backupGameInfo: BackupGameInfo): Result =
        withContext(Dispatchers.IO) {
            val currentUser = auth.currentUser ?: return@withContext Result(false, "User not signed in")

            try {
                // Delete from Firestore first
                firestore
                    .collection("users")
                    .document(currentUser.uid)
                    .collection("backups")
                    .document(backupGameInfo.id)
                    .delete()
                    .await()

                Log.d(TAG, "Deleted backup record from Firestore")

                // Then try to delete from Storage
                try {
                    val storageRef =
                        storage.reference
                            .child(currentUser.uid)
                            .child("game_backups")
                            .child(backupGameInfo.hash)

                    // Check if file exists before attempting to delete
                    storageRef.metadata.await()

                    // File exists, delete it
                    storageRef.delete().await()
                    Log.d(TAG, "Deleted backup file from Storage")
                } catch (e: StorageException) {
                    // If file doesn't exist, that's okay since we're trying to delete it anyway
                    if (e.errorCode == -13010 || e.httpResultCode == 404) {
                        Log.d(TAG, "Backup file already doesn't exist in Storage: ${backupGameInfo.hash}")
                    } else {
                        // For other storage errors, log but don't fail the operation
                        // since we've already deleted from Firestore
                        Log.e(TAG, "Storage error while deleting backup: ${e.message}, code: ${e.errorCode}")
                    }
                }

                return@withContext Result(true, "Backup deleted successfully")
            } catch (e: Exception) {
                Log.e(TAG, "Error deleting backup: ${e.message}")
                return@withContext Result(false, "Error: ${e.message}")
            }
        }

    /**
     * Get file extension from a file path or URI
     * @param path The file path or URI
     * @return The file extension with dot (e.g., ".bin")
     */
    private fun getFileExtension(path: String): String {
        val lastDot = path.lastIndexOf(".")
        return if (lastDot >= 0) {
            path.substring(lastDot)
        } else {
            ""
        }
    }

    /**
     * Result class for backup/restore operations
     */
    data class Result(
        val success: Boolean,
        val message: String,
        val backupList: List<BackupGameInfo> = emptyList(),
        val limitReached: Boolean = false,
    )

    /**
     * Data class for backed up game information
     */
    data class BackupGameInfo(
        val id: String,
        val filename: String,
        val hash: String,
        val uploadedAt: Date,
        val size: Long,
        val gameTitle: String,
        val productNumber: String,
        val downloadUrl: String,
    )
}
