package org.uoyabause.android

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.uoyabause.android.backup.GameBackupManager.BackupGameInfo
import java.util.Date

class CloudGameInfoTest {
    @Test
    fun testToGameInfo() {
        // Create a BackupGameInfo
        val backupInfo =
            BackupGameInfo(
                id = "test-id",
                filename = "test-game.iso",
                hash = "test-hash",
                uploadedAt = Date(),
                size = 1024L,
                gameTitle = "Test Game",
                productNumber = "T-12345",
                downloadUrl = "https://example.com/test-game.iso",
            )

        // Create a CloudGameInfo
        val cloudGameInfo = CloudGameInfo(backupInfo)

        // Convert to GameInfo
        val gameInfo = cloudGameInfo.toGameInfo()

        // Verify the conversion
        assertEquals(-backupInfo.id.hashCode(), gameInfo.id)
        assertEquals("", gameInfo.file_path)
        assertEquals("", gameInfo.iso_file_path)
        assertEquals("Test Game", gameInfo.game_title)
        assertEquals("T-12345", gameInfo.product_number)
        assertTrue(gameInfo.isCloudOnly)
        assertEquals(backupInfo, gameInfo.cloudBackupInfo)
    }
}
