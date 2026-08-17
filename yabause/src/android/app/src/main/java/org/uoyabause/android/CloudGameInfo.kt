package org.uoyabause.android

import org.uoyabause.android.backup.GameBackupManager.BackupGameInfo

/**
 * Represents a game that is backed up in the cloud but not downloaded locally
 */
class CloudGameInfo(
    val backupInfo: BackupGameInfo,
) {
    // Convert to GameInfo for display in the adapter
    fun toGameInfo(): GameInfo = GameInfo(
        id = -backupInfo.id.hashCode(), // Negative ID to distinguish from local games
        file_path = "", // Empty as it's not downloaded yet
        iso_file_path = "",
        game_title = backupInfo.gameTitle,
        maker_id = "",
        product_number = backupInfo.productNumber,
        version = "",
        release_date = "",
        device_infomation = "",
        area = "",
        input_device = "",
        last_playdate = null,
        update_at = backupInfo.uploadedAt,
        image_url = "https://d3edktb2n8l35b.cloudfront.net/BOXART/${backupInfo.productNumber}.PNG",
        rating = 0,
        lastplay_date = null,
        isCloudOnly = true,
        cloudBackupInfo = backupInfo,
    )
}
