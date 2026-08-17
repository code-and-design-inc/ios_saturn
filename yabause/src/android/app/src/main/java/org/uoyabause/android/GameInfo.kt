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

import android.content.Context
import android.net.Uri
import android.os.Bundle
import android.provider.DocumentsContract
import android.util.Log
import androidx.documentfile.provider.DocumentFile
import androidx.room.ColumnInfo
import androidx.room.Dao
import androidx.room.Database
import androidx.room.Entity
import androidx.room.Ignore
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.PrimaryKey
import androidx.room.Query
import androidx.room.RoomDatabase
import androidx.room.TypeConverter
import androidx.room.TypeConverters
import androidx.room.Update
import com.google.firebase.Firebase
import com.google.firebase.analytics.FirebaseAnalytics
import com.google.firebase.firestore.firestore
import kotlinx.coroutines.tasks.await
import org.devmiyax.yabasanshiro.BuildConfig
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.YabauseApplication.Companion.appContext
import org.uoyabause.android.YabauseStorage.Companion.dao
import org.uoyabause.android.backup.GameBackupManager.BackupGameInfo
import org.uoyabause.android.cheat.Cheat
import org.uoyabause.android.cheat.CheatDao
import java.io.BufferedInputStream
import java.io.BufferedReader
import java.io.DataInputStream
import java.io.File
import java.io.FileInputStream
import java.io.FileNotFoundException
import java.io.FileReader
import java.io.IOException
import java.io.InputStreamReader
import java.lang.Exception
import java.net.Authenticator
import java.net.PasswordAuthentication
import java.nio.charset.Charset
import java.text.SimpleDateFormat
import java.util.ArrayList
import java.util.Date
import java.util.Locale
import java.util.regex.Pattern

class Converters {
    @TypeConverter
    fun fromTimestamp(value: Long?): Date? = value?.let { Date(it) }

    @TypeConverter
    fun dateToTimestamp(date: Date?): Long? = date?.time
}

@Database(entities = [GameInfo::class, GameStatus::class, Cheat::class], version = 3)
@TypeConverters(Converters::class)
abstract class GameInfoDatabase : RoomDatabase() {
    abstract fun gameInfoDao(): GameInfoDao

    abstract fun gameStatusDao(): GameStatusDao

    abstract fun cheatDao(): CheatDao

    companion object {
        val MIGRATION_1_2 =
            object : androidx.room.migration.Migration(1, 2) {
                override fun migrate(database: androidx.sqlite.db.SupportSQLiteDatabase) {
                    // Add RetroAchievements related columns
                    database.execSQL("ALTER TABLE GameInfo ADD COLUMN ra_game_id INTEGER")
                    database.execSQL("ALTER TABLE GameInfo ADD COLUMN ra_hash TEXT")
                    database.execSQL("ALTER TABLE GameInfo ADD COLUMN ra_unlocked INTEGER NOT NULL DEFAULT 0")
                    database.execSQL("ALTER TABLE GameInfo ADD COLUMN ra_total INTEGER NOT NULL DEFAULT 0")
                    database.execSQL("ALTER TABLE GameInfo ADD COLUMN ra_last_update INTEGER")
                    database.execSQL("ALTER TABLE GameInfo ADD COLUMN ra_not_supported INTEGER NOT NULL DEFAULT 0")
                }
            }

        val MIGRATION_2_3 =
            object : androidx.room.migration.Migration(2, 3) {
                override fun migrate(database: androidx.sqlite.db.SupportSQLiteDatabase) {
                    database.execSQL("ALTER TABLE GameInfo ADD COLUMN ra_unlocked_hardcore INTEGER NOT NULL DEFAULT 0")
                }
            }
    }
}

@Dao
interface GameInfoDao {
    @Query("SELECT * FROM GameInfo")
    fun getAll(): List<GameInfo>

    @Query("SELECT * FROM GameInfo WHERE file_path = :file_path")
    fun findByFilePath(file_path: String): GameInfo?

    @Query("SELECT * FROM GameInfo WHERE file_path LIKE '%' || :documentIdSuffix")
    fun findByDocumentIdSuffix(documentIdSuffix: String): GameInfo?

    @Query("SELECT * FROM GameInfo WHERE product_number = :product_number AND device_infomation = :device_infomation")
    fun findByProductId(
        product_number: String,
        device_infomation: String,
    ): GameInfo

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    fun insertAll(vararg gameInfo: GameInfo)

    @androidx.room.Delete
    fun delete(gameInfo: GameInfo)

    @Query("SELECT * FROM GameInfo WHERE iso_file_path = :file_path")
    fun findByInDirectFilePath(file_path: String): GameInfo

    @Query("DELETE FROM GameInfo")
    fun deleteAll()

    @Update
    fun update(gameInfo: GameInfo)

    @Query("SELECT * FROM GameInfo ORDER BY lastplay_date DESC LIMIT 5")
    fun getRecentGames(): List<GameInfo>

    @Query("SELECT COUNT(*) FROM GameInfo")
    fun getRowCount(): Int

    @Query("SELECT * FROM GameInfo ORDER BY game_title ASC")
    fun getAllSortedByTitle(): List<GameInfo>

    @Query("SELECT * FROM GameInfo WHERE product_number = :product_number LIMIT 1")
    fun findByProductNumber(product_number: String): GameInfo?
}

/**
 * Interface for handling permission requests during file deletion
 */
interface PermissionRequestCallback {
    fun onPermissionRequired(
        uri: Uri,
        message: String,
    ): Boolean
}

/**
 * Created by shinya on 2015/12/30.
 */
@Entity
data class GameInfo(
    @PrimaryKey(autoGenerate = true) var id: Int = 0,
    @ColumnInfo(name = "file_path", index = true) var file_path: String = "",
    @ColumnInfo(name = "iso_file_path", index = true) var iso_file_path: String = "",
    @ColumnInfo(name = "game_title") var game_title: String = "",
    @ColumnInfo(name = "maker_id") var maker_id: String = "",
    @ColumnInfo(name = "product_number", index = true) var product_number: String = "",
    @ColumnInfo(name = "version") var version: String = "",
    @ColumnInfo(name = "release_date") var release_date: String = "",
    @ColumnInfo(name = "device_infomation") var device_infomation: String = "",
    @ColumnInfo(name = "area") var area: String = "",
    @ColumnInfo(name = "input_device") var input_device: String = "",
    @ColumnInfo(name = "last_playdate") var last_playdate: Date? = null,
    @ColumnInfo(name = "update_at") var update_at: Date? = Date(),
    @ColumnInfo(name = "image_url") var image_url: String? = "",
    @ColumnInfo(name = "rating") var rating: Int = 0,
    @ColumnInfo(name = "lastplay_date") var lastplay_date: Date? = null,
    // RetroAchievements related fields
    @ColumnInfo(name = "ra_game_id") var raGameId: Int? = null, // RA Game ID (numeric)
    @ColumnInfo(name = "ra_hash") var raHash: String? = null, // MD5 hash (32 characters)
    @ColumnInfo(name = "ra_unlocked") var raUnlocked: Int = 0, // Unlocked achievements count
    @ColumnInfo(name = "ra_total") var raTotal: Int = 0, // Total achievements count
    @ColumnInfo(name = "ra_last_update") var raLastUpdate: Date? = null, // Last update timestamp
    @ColumnInfo(name = "ra_not_supported") var raNotSupported: Boolean = false, // RA not registered flag
    @ColumnInfo(name = "ra_unlocked_hardcore") var raUnlockedHardcore: Int = 0, // Hardcore unlocked achievements count
    // These fields are not stored in the database but used for display
    @Ignore var isCloudOnly: Boolean = false,
    @Ignore var cloudBackupInfo: BackupGameInfo? = null,
) {
    companion object {
        /*
        fun getFromFileName(file_path: String?): GameInfo? {
            Log.d("GameInfo :", file_path!!)
            return Select()
                .from(GameInfo::class.java)
                .where("file_path = ?", file_path)
                .executeSingle()
        }
         */

/*
        fun getFromInDirectFileName(file_path: String): GameInfo? {
            var lfile_path = file_path
            Log.d("GameInfo direct:", lfile_path)
            lfile_path = file_path.uppercase(Locale.getDefault())
            return Select()
                .from(GameInfo::class.java)
                .where("iso_file_path = ?", lfile_path)
                .executeSingle()
        }
*/

/*
        fun deleteAll() {
            Delete().from(GameInfo::class.java).execute<Model>()
        }
         */

        var sigin = ""

        fun initSigin(context: Context) {
            sigin = context.getString(R.string.boxart_sigin).replace("%26", "&")
        }

        fun genGameInfoFromCUE(file_path: String?): GameInfo? {
            if (file_path == null) return null
            val file = File(file_path)
            val dirName = file.parent
            var isoFileName = ""
            try {
                val filereader = FileReader(file)
                val br = BufferedReader(filereader)
                var str = br.readLine()
                while (str != null) {
                    // System.out.println(str);
                    val p = Pattern.compile("FILE \"(.*)\"")
                    val m = p.matcher(str)
                    if (m.find()) {
                        isoFileName = m.group(1) as String
                        break
                    }
                    str = br.readLine()
                }
                br.close()
                if (isoFileName == "") {
                    return null // bad file format;
                }
            } catch (e: FileNotFoundException) {
                println(e)
                return null
            } catch (e: IOException) {
                println(e)
                return null
            }
            if (dirName != null) {
                isoFileName = dirName + File.separator + isoFileName
            }
            var tmp: GameInfo? = genGameInfoFromIso(isoFileName)
            if (tmp == null) return null
            tmp.file_path = file_path
            tmp.iso_file_path = isoFileName.uppercase(Locale.getDefault())
            return tmp
        }

        fun genGameInfoFromMDS(file_path: String?): GameInfo? {
            if (file_path == null) return null
            val isoFileName = file_path.replace(".mds", ".mdf")
            var tmp = genGameInfoFromIso(isoFileName)
            if (tmp == null) return null
            tmp.file_path = file_path
            tmp.iso_file_path = isoFileName.uppercase(Locale.getDefault())

            // read mdf
            return tmp
        }

        fun genGameInfoFromCCD(file_path: String?): GameInfo? {
            if (file_path == null) return null
            val isoFileName = file_path.replace(".ccd", ".img")
            var tmp = genGameInfoFromIso(isoFileName)
            if (tmp == null) return null
            tmp.file_path = file_path
            tmp.iso_file_path = isoFileName.uppercase(Locale.getDefault())
            return tmp
        }

        fun getGimeInfoFromBuf(
            file_path: String?,
            header: ByteArray,
        ): GameInfo? {
            if (file_path == null) return null
            var startindex = -1
            val checkStr =
                byteArrayOf(
                    'S'.code.toByte(),
                    'E'.code.toByte(),
                    'G'.code.toByte(),
                    'A'.code.toByte(),
                    ' '.code.toByte(),
                )
            for (i in 0 until header.size - checkStr.size) {
                if (header[i + 0] == checkStr[0] &&
                    header[i + 1] == checkStr[1] &&
                    header[i + 2] == checkStr[2] &&
                    header[i + 3] == checkStr[3] &&
                    header[i + 4] == checkStr[4]
                ) {
                    startindex = i
                    break
                }
            }
            if (startindex == -1) return null
            var tmp = GameInfo()
            try {
                val charaset = Charset.forName("MS932")
                tmp.file_path = file_path
                tmp.iso_file_path = file_path.uppercase(Locale.getDefault())
                tmp.maker_id = String(header, startindex + 0x10, 0x10)
                tmp.maker_id = tmp.maker_id.trim { it <= ' ' }
                tmp.product_number = String(header, startindex + 0x20, 0xA, charaset)
                tmp.product_number = tmp.product_number.trim { it <= ' ' }
                tmp.version = String(header, startindex + 0x2A, 0x10, charaset)
                tmp.version = tmp.version.trim { it <= ' ' }
                tmp.release_date = String(header, startindex + 0x30, 0x8, charaset)
                tmp.release_date = tmp.release_date.trim { it <= ' ' }
                tmp.area = String(header, startindex + 0x40, 0xA, charaset)
                tmp.area = tmp.area.trim { it <= ' ' }
                tmp.input_device = String(header, startindex + 0x50, 0x10, charaset)
                tmp.input_device = tmp.input_device.trim { it <= ' ' }
                tmp.device_infomation = String(header, startindex + 0x38, 0x8, charaset)
                tmp.device_infomation = tmp.device_infomation.trim { it <= ' ' }
                tmp.game_title = String(header, startindex + 0x60, 0x70, charaset)
                tmp.game_title = tmp.game_title.trim { it <= ' ' }
                tmp.image_url = "https://d3edktb2n8l35b.cloudfront.net/BOXART/" + tmp.product_number + ".PNG?" + sigin
            } catch (e: Exception) {
                e.localizedMessage?.let { Log.e("GameInfo", it) }
                return null
            }
            return tmp
        }

        fun genGameInfoFromCHD(file_path: String?): GameInfo? {
            if (file_path == null) return null
            Log.d("yabause", file_path)
            val header = YabauseRunnable.getGameinfoFromChd(file_path) ?: return null
            return getGimeInfoFromBuf(file_path, header)
        }

        @JvmStatic
        fun genGameInfoFromIso(file_path: String?): GameInfo? {
            if (file_path == null) return null
            return try {
                val buff = ByteArray(0xFF)
                val dataInStream =
                    DataInputStream(
                        BufferedInputStream(FileInputStream(file_path)),
                    )
                dataInStream.read(buff, 0x0, 0xFF)
                dataInStream.close()
                getGimeInfoFromBuf(file_path, buff)
            } catch (e: FileNotFoundException) {
                println(e)
                null
            } catch (e: IOException) {
                println(e)
                null
            }
        }

        init {
            System.loadLibrary("yabause_native")
        }
    }

    /**
     * Check if the app has write permission for the given content URI
     */
    private fun hasWritePermissionForUri(uri: Uri): Boolean {
        val context = YabauseApplication.appContext
        return try {
            // For content URIs, we need to check if we have permission for the parent tree URI
            val persistedUris = context.contentResolver.persistedUriPermissions

            // First check if we have direct permission for this URI
            val hasDirectPermission =
                persistedUris.any { permission ->
                    permission.uri == uri && permission.isWritePermission
                }

            if (hasDirectPermission) {
                Log.d("GameInfo", "Has direct persisted write permission for URI: $uri")
                return true
            }

            // For single file URIs, check if we have permission for any parent tree URI
            val hasTreePermission =
                persistedUris.any { permission ->
                    permission.isWritePermission &&
                        DocumentsContract.isTreeUri(permission.uri) &&
                        isUriUnderTree(uri, permission.uri)
                }

            if (hasTreePermission) {
                Log.d("GameInfo", "Has tree write permission covering URI: $uri")
                return true
            }

            // Check if DocumentFile can write as a fallback
            val documentFile = DocumentFile.fromSingleUri(context, uri)
            val canWrite = documentFile?.canWrite() ?: false
            Log.d("GameInfo", "DocumentFile canWrite: $canWrite for URI: $uri")

            canWrite
        } catch (e: Exception) {
            Log.e("GameInfo", "Error checking write permission for URI: $uri, error: ${e.message}")
            false
        }
    }

    /**
     * Check if a file URI is under a tree URI
     */
    private fun isUriUnderTree(
        fileUri: Uri,
        treeUri: Uri,
    ): Boolean = try {
        val context = YabauseApplication.appContext

        // Get the document ID from the file URI
        val fileDocumentId = DocumentsContract.getDocumentId(fileUri)

        // Get the tree document ID from the tree URI
        val treeDocumentId = DocumentsContract.getTreeDocumentId(treeUri)

        // Check if the file document ID starts with the tree document ID
        val isUnderTree = fileDocumentId.startsWith(treeDocumentId)

        Log.d("GameInfo", "Checking if file URI is under tree:")
        Log.d("GameInfo", "  File URI: $fileUri")
        Log.d("GameInfo", "  Tree URI: $treeUri")
        Log.d("GameInfo", "  File document ID: $fileDocumentId")
        Log.d("GameInfo", "  Tree document ID: $treeDocumentId")
        Log.d("GameInfo", "  Is under tree: $isUnderTree")

        isUnderTree
    } catch (e: Exception) {
        Log.e("GameInfo", "Error checking if URI is under tree: ${e.message}")
        // Fallback: try to use DocumentFile to check if the tree can access the file
        try {
            val context = YabauseApplication.appContext
            val treeDocumentFile = DocumentFile.fromTreeUri(context, treeUri)
            val fileDocumentFile = DocumentFile.fromSingleUri(context, fileUri)

            // If both can be resolved, there's a good chance they're related
            treeDocumentFile != null && fileDocumentFile != null
        } catch (fallbackException: Exception) {
            Log.e("GameInfo", "Fallback check also failed: ${fallbackException.message}")
            false
        }
    }

    /**
     * Check if the app has write permission for the parent directory of the given content URI
     */
    private fun hasWritePermissionForParentUri(parentUri: Uri): Boolean {
        val context = YabauseApplication.appContext
        return try {
            // Check if we have persistable permission for the parent URI
            val persistedUris = context.contentResolver.persistedUriPermissions
            val hasPersistedPermission =
                persistedUris.any { permission ->
                    permission.uri == parentUri && permission.isWritePermission
                }

            if (hasPersistedPermission) {
                Log.d("GameInfo", "Has persisted write permission for parent URI: $parentUri")
                return true
            }

            // Check if DocumentFile can write
            val parentDir = DocumentFile.fromTreeUri(context, parentUri)
            val canWrite = parentDir?.canWrite() ?: false
            Log.d("GameInfo", "Parent DocumentFile canWrite: $canWrite for URI: $parentUri")

            canWrite
        } catch (e: Exception) {
            Log.e("GameInfo", "Error checking write permission for parent URI: $parentUri, error: ${e.message}")
            false
        }
    }

    /**
     * Request write permission for the given URI by launching document tree picker
     * This method should be called from an Activity context
     */
    private fun requestWritePermission(
        uri: Uri,
        callback: PermissionRequestCallback? = null,
    ): Boolean {
        Log.w("GameInfo", "Write permission not available for URI: $uri")

        val message = "Write permission is required to delete files. Please grant permission through the file picker."
        return callback?.onPermissionRequired(uri, message) ?: run {
            Log.w("GameInfo", "No permission callback provided. Cannot request permission.")
            false
        }
    }

    fun removeInstance(permissionCallback: PermissionRequestCallback? = null) {
        val isContentUri = file_path.startsWith("content://")
        val fname = file_path.uppercase(Locale.getDefault())

        if (isContentUri) {
            // Handle content:// URIs
            val uri = Uri.parse(file_path)
            val context = YabauseApplication.appContext

            // Check write permission before attempting deletion
            if (!hasWritePermissionForUri(uri)) {
                Log.e("GameInfo", "No write permission for URI: $uri")
                if (!requestWritePermission(uri, permissionCallback)) {
                    Log.e("GameInfo", "Cannot delete file without write permission: $uri")
                    // Still remove from database even if file deletion fails
                    YabauseStorage.dao.delete(this)
                    return
                }
            }

            if (fname.endsWith("CHD")) {
                // For CHD files, just delete the main file
                try {
                    val documentFile = DocumentFile.fromSingleUri(context, uri)
                    if (documentFile != null && documentFile.exists()) {
                        if (documentFile.canWrite()) {
                            val deleted = documentFile.delete()
                            Log.d("GameInfo", "Content URI deletion result: $deleted")
                            if (!deleted) {
                                Log.w("GameInfo", "Failed to delete file via DocumentFile, trying ContentResolver")
                                val deletedViaResolver = context.contentResolver.delete(uri, null, null)
                                Log.d("GameInfo", "ContentResolver deletion result: $deletedViaResolver")
                            }
                        } else {
                            Log.e("GameInfo", "No write permission for file: $uri")
                        }
                    } else {
                        Log.e("GameInfo", "File does not exist or cannot be accessed: $uri")
                    }
                } catch (e: Exception) {
                    Log.e("GameInfo", "Failed to delete content URI: ${e.message}")
                }
                YabauseStorage.dao.delete(this)
            } else if (fname.endsWith("CCD") || fname.endsWith("MDS")) {
                // For CCD/MDS files, find and delete related files
                try {
                    val documentFile = DocumentFile.fromSingleUri(context, uri)
                    val parentUri = Uri.parse(iso_file_path) // iso_file_path contains the directory URI
                    val parentDir = DocumentFile.fromTreeUri(context, parentUri)

                    // Check write permission for parent directory
                    if (!hasWritePermissionForParentUri(parentUri)) {
                        Log.e("GameInfo", "No write permission for parent directory: $parentUri")
                        if (!requestWritePermission(parentUri, permissionCallback)) {
                            Log.e("GameInfo", "Cannot delete CCD/MDS files without write permission")
                            YabauseStorage.dao.delete(this)
                            return
                        }
                    }

                    if (parentDir != null && documentFile != null && parentDir.canWrite()) {
                        val baseName =
                            documentFile.name?.let { name ->
                                name
                                    .replace(".(?i)ccd".toRegex(), "")
                                    .replace(".(?i)mds".toRegex(), "")
                            }

                        if (baseName != null) {
                            // Find and delete all files with the same base name
                            val filesToDelete =
                                parentDir.listFiles().filter { file ->
                                    file.name?.startsWith(baseName) == true
                                }

                            for (fileToDelete in filesToDelete) {
                                try {
                                    if (fileToDelete.canWrite()) {
                                        val deleted = fileToDelete.delete()
                                        Log.d("GameInfo", "Deleted related file ${fileToDelete.name}: $deleted")
                                        if (!deleted) {
                                            Log.w(
                                                "GameInfo",
                                                "Failed to delete ${fileToDelete.name} via DocumentFile, trying ContentResolver",
                                            )
                                            val deletedViaResolver = context.contentResolver.delete(fileToDelete.uri, null, null)
                                            Log.d(
                                                "GameInfo",
                                                "ContentResolver deletion result for ${fileToDelete.name}: $deletedViaResolver",
                                            )
                                        }
                                    } else {
                                        Log.e("GameInfo", "No write permission for related file: ${fileToDelete.name}")
                                    }
                                } catch (e: Exception) {
                                    Log.e("GameInfo", "Failed to delete related file ${fileToDelete.name}: ${e.message}")
                                }
                            }
                        }
                    } else {
                        Log.e("GameInfo", "Parent directory not accessible or no write permission")
                    }
                } catch (e: Exception) {
                    Log.e("GameInfo", "Failed to delete CCD/MDS files: ${e.message}")
                }
                YabauseStorage.dao.delete(this)
            } else if (fname.endsWith("CUE")) {
                // For CUE files, read the content and delete referenced files
                try {
                    val deleteFiles: MutableList<String> = ArrayList()

                    context.contentResolver.openInputStream(uri)?.use { inputStream ->
                        BufferedReader(InputStreamReader(inputStream)).use { reader ->
                            var line = reader.readLine()
                            while (line != null) {
                                val p = Pattern.compile("FILE \"(.*)\"")
                                val m = p.matcher(line)
                                if (m.find()) {
                                    m.group(1)?.let { deleteFiles.add(it) }
                                }
                                line = reader.readLine()
                            }
                        }
                    }

                    // Delete referenced files
                    val parentUri = Uri.parse(iso_file_path) // iso_file_path contains the directory URI
                    val parentDir = DocumentFile.fromTreeUri(context, parentUri)

                    // Check write permission for parent directory
                    if (!hasWritePermissionForParentUri(parentUri)) {
                        Log.e("GameInfo", "No write permission for parent directory: $parentUri")
                        if (!requestWritePermission(parentUri, permissionCallback)) {
                            Log.e("GameInfo", "Cannot delete CUE files without write permission")
                            YabauseStorage.dao.delete(this)
                            return
                        }
                    }

                    if (parentDir != null && parentDir.canWrite()) {
                        for (fileName in deleteFiles) {
                            val fileToDelete = parentDir.findFile(fileName)
                            if (fileToDelete != null) {
                                try {
                                    if (fileToDelete.canWrite()) {
                                        val deleted = fileToDelete.delete()
                                        Log.d("GameInfo", "Deleted referenced file $fileName: $deleted")
                                        if (!deleted) {
                                            Log.w("GameInfo", "Failed to delete $fileName via DocumentFile, trying ContentResolver")
                                            val deletedViaResolver = context.contentResolver.delete(fileToDelete.uri, null, null)
                                            Log.d("GameInfo", "ContentResolver deletion result for $fileName: $deletedViaResolver")
                                        }
                                    } else {
                                        Log.e("GameInfo", "No write permission for referenced file: $fileName")
                                    }
                                } catch (e: Exception) {
                                    Log.e("GameInfo", "Failed to delete referenced file $fileName: ${e.message}")
                                }
                            } else {
                                Log.w("GameInfo", "Referenced file not found: $fileName")
                            }
                        }
                    } else {
                        Log.e("GameInfo", "Parent directory not accessible or no write permission")
                    }

                    // Delete the CUE file itself
                    val cueDocumentFile = DocumentFile.fromSingleUri(context, uri)
                    if (cueDocumentFile != null && cueDocumentFile.exists()) {
                        if (cueDocumentFile.canWrite()) {
                            val deleted = cueDocumentFile.delete()
                            Log.d("GameInfo", "Deleted CUE file: $deleted")
                            if (!deleted) {
                                Log.w("GameInfo", "Failed to delete CUE file via DocumentFile, trying ContentResolver")
                                val deletedViaResolver = context.contentResolver.delete(uri, null, null)
                                Log.d("GameInfo", "ContentResolver deletion result for CUE file: $deletedViaResolver")
                            }
                        } else {
                            Log.e("GameInfo", "No write permission for CUE file")
                        }
                    } else {
                        Log.e("GameInfo", "CUE file does not exist or cannot be accessed")
                    }
                } catch (e: Exception) {
                    Log.e("GameInfo", "Failed to delete CUE files: ${e.message}")
                }
                YabauseStorage.dao.delete(this)
            } else {
                // Handle other content:// file types
                try {
                    val documentFile = DocumentFile.fromSingleUri(context, uri)
                    if (documentFile != null && documentFile.exists()) {
                        if (documentFile.canWrite()) {
                            val deleted = documentFile.delete()
                            Log.d("GameInfo", "Content URI deletion result: $deleted")
                            if (!deleted) {
                                Log.w("GameInfo", "Failed to delete file via DocumentFile, trying ContentResolver")
                                val deletedViaResolver = context.contentResolver.delete(uri, null, null)
                                Log.d("GameInfo", "ContentResolver deletion result: $deletedViaResolver")
                            }
                        } else {
                            Log.e("GameInfo", "No write permission for file: $uri")
                        }
                    } else {
                        Log.e("GameInfo", "File does not exist or cannot be accessed: $uri")
                    }
                } catch (e: Exception) {
                    Log.e("GameInfo", "Failed to delete content URI: ${e.message}")
                }
                YabauseStorage.dao.delete(this)
            }
        } else {
            // Handle regular file:// paths (existing logic)
            if (fname.endsWith("CHD")) {
                val file = File(file_path)
                if (file.exists()) {
                    file.delete()
                }
                YabauseStorage.dao.delete(this)
            } else if (fname.endsWith("CCD") || fname.endsWith("MDS")) {
                val file = File(file_path)
                val dir = file.parentFile
                var searchName = file.name
                searchName = searchName.replace(".(?i)ccd".toRegex(), "")
                searchName = searchName.replace(".(?i)mds".toRegex(), "")
                val searchNamef = searchName
                val matchingFiles = dir?.listFiles { _, name -> name.startsWith(searchNamef) }
                if (matchingFiles != null) {
                    for (removefile in matchingFiles) {
                        if (removefile.exists()) {
                            removefile.delete()
                        }
                    }
                }
                YabauseStorage.dao.delete(this)
            } else if (fname.endsWith("CUE")) {
                val deleteFiles: MutableList<String> = ArrayList()
                try {
                    val filereader = FileReader(file_path)
                    val br = BufferedReader(filereader)
                    var str = br.readLine()
                    while (str != null) {
                        // System.out.println(str);
                        val p = Pattern.compile("FILE \"(.*)\"")
                        val m = p.matcher(str)
                        if (m.find()) {
                            m.group(1)?.let { deleteFiles.add(it) }
                        }
                        str = br.readLine()
                    }
                    br.close()
                    val file = File(file_path)
                    for (removefile in deleteFiles) {
                        val delname = file.parentFile?.absolutePath + "/" + removefile
                        val f = File(delname)
                        if (f.exists()) {
                            f.delete()
                        }
                    }
                    file.delete()
                    YabauseStorage.dao.delete(this)
                } catch (e: FileNotFoundException) {
                } catch (e: IOException) {
                }
            } else {
                // Handle other file types (ISO, BIN, IMG, etc.)
                val file = File(file_path)
                if (file.exists()) {
                    Log.d("GameInfo", "Deleting file: ${file.absolutePath}")
                    val deleted = file.delete()
                    Log.d("GameInfo", "File deletion result: $deleted")
                } else {
                    Log.d("GameInfo", "File does not exist: ${file.absolutePath}")
                }
                YabauseStorage.dao.delete(this)
            }
        }
    }

    internal inner class BasicAuthenticator(
        private val user: String,
        private val password: String,
    ) : Authenticator() {
        override fun getPasswordAuthentication(): PasswordAuthentication = PasswordAuthentication(user, password.toCharArray())
    }

    suspend fun updateState(): Int {
        val ctx = appContext

        if (product_number == "") return -1

        // デフォルト値を設定
        //
        rating = 0
        try {
            val sdf = SimpleDateFormat("yyyy-MM-dd HH:mm:ss")
            update_at = sdf.parse("2001-01-01 00:00:00")
        } catch (e: Exception) {
            e.printStackTrace()
        }

        // Firestoreのインスタンスを取得
        val db = Firebase.firestore

        try {
            // Firestoreから検索（await を使用して同期的に処理）
            val documents =
                db
                    .collection("games")
                    .whereEqualTo("product_number", product_number)
                    .get()
                    .await()

            if (!documents.isEmpty) {
                // ドキュメントが見つかった場合
                val document = documents.documents[0]

                // update_atフィールドがあれば取得
                document.getTimestamp("update_at")?.toDate()?.let {
                    update_at = it
                }

                // games/{id}/summary/ratings/averageRating から rating を取得（await を使用して同期的に処理）
                val documentId = document.id
                val ratingsDoc =
                    db
                        .collection("games")
                        .document(documentId)
                        .collection("summary")
                        .document("ratings")
                        .get()
                        .await()

                ratingsDoc.getLong("averageRating")?.let {
                    rating = it.toInt()
                }
            } else {
                // ドキュメントが見つからない場合
                var mFirebaseAnalytics = FirebaseAnalytics.getInstance(ctx)
                val bundle = Bundle()
                bundle.putString(FirebaseAnalytics.Param.ITEM_ID, product_number)
                bundle.putString(FirebaseAnalytics.Param.ITEM_NAME, game_title)
                mFirebaseAnalytics.logEvent(
                    "yab_game_not_found",
                    bundle,
                )

                // 新しいゲーム情報をFirestoreに追加（デバッグモードのみ）
                try {
                    Log.i(
                        "GameInfo",
                        product_number + "( " + game_title + " ) is not found ",
                    )

                    // automatic update
                    if (BuildConfig.DEBUG) {
                        val gameData =
                            hashMapOf(
                                "maker_id" to maker_id,
                                "product_number" to product_number,
                                "version" to version,
                                "release_date" to release_date,
                                "device_infomation" to device_infomation,
                                "area" to area,
                                "game_title" to game_title,
                                "input_device" to input_device,
                                "rating" to 0,
                                "update_at" to Date(),
                            )

                        // Firestoreに追加（await を使用して同期的に処理）
                        val documentReference =
                            db
                                .collection("games")
                                .add(gameData)
                                .await()

                        Log.i("GameInfo", "Game added with ID: ${documentReference.id}")
                    }
                } catch (e: Exception) {
                    e.printStackTrace()
                    Log.e("GameInfo", product_number + "( " + game_title + " ) " + e.localizedMessage)
                }
            }
        } catch (exception: Exception) {
            Log.e("GameInfo", "Error getting documents: ", exception)
        }

        return 0
    }

/*
    constructor(parcel: Parcel) : this() {
        file_path = parcel.readString()
        iso_file_path = parcel.readString()
        game_title = parcel.readString()
        maker_id = parcel.readString()
        product_number = parcel.readString()
        version = parcel.readString()
        release_date = parcel.readString()
        device_infomation = parcel.readString()
        area = parcel.readString()
        input_device = parcel.readString()
        image_url = parcel.readString()
        rating = parcel.readInt()
    }


    override fun writeToParcel(parcel: Parcel, flags: Int) {
        parcel.writeString(file_path)
        parcel.writeString(iso_file_path)
        parcel.writeString(game_title)
        parcel.writeString(maker_id)
        parcel.writeString(product_number)
        parcel.writeString(version)
        parcel.writeString(release_date)
        parcel.writeString(device_infomation)
        parcel.writeString(area)
        parcel.writeString(input_device)
        parcel.writeString(image_url)
        parcel.writeInt(rating)
    }

    override fun describeContents(): Int {
        return 0
    }

    companion object CREATOR : Parcelable.Creator<GameInfo> {
        override fun createFromParcel(parcel: Parcel): GameInfo {
            return GameInfo(parcel)
        }

        override fun newArray(size: Int): Array<GameInfo?> {
            return arrayOfNulls(size)
        }
    }
 */
}
