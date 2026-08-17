package org.uoyabause.android.backup.viewmodel

import org.uoyabause.android.backup.model.BackupItem
import org.uoyabause.android.backup.model.DeviceType

/**
 * Test fixtures for BackupItem used in unit tests.
 */
object BackupItemTestFixtures {
    fun createItem(
        id: String = "0",
        filename: String = "SaveData",
        comment: String = "",
        gameTitle: String? = null,
        saveDate: String = "2024-01-01 00:00:00",
        dataSize: Int = 1024,
        blockSize: Int = 1,
        deviceType: DeviceType = DeviceType.INTERNAL,
    ): BackupItem = BackupItem(
        id = id,
        filename = filename,
        comment = comment,
        gameTitle = gameTitle,
        saveDate = saveDate,
        dataSize = dataSize,
        blockSize = blockSize,
        deviceType = deviceType,
    )

    /** 5 items with distinct filenames, gameTitles, comments, and dates */
    val fiveItems: List<BackupItem> = listOf(
        createItem(id = "0", filename = "Game1_Save", gameTitle = "Sonic", comment = "Stage 1", saveDate = "2024-01-15 10:00:00"),
        createItem(id = "1", filename = "Game2_Save", gameTitle = "NiGHTS", comment = "Boss clear", saveDate = "2024-03-20 15:30:00"),
        createItem(id = "2", filename = "Game3_Save", gameTitle = "Panzer Dragoon", comment = "Episode 5", saveDate = "2024-02-10 08:45:00"),
        createItem(id = "3", filename = "RPG_Data", gameTitle = "Shining Force III", comment = "Chapter 2", saveDate = "2024-04-05 20:00:00"),
        createItem(id = "4", filename = "Action_Save", gameTitle = "Guardian Heroes", comment = "Final boss", saveDate = "2024-01-01 12:00:00"),
    )

    /** 3 items for simple sort tests */
    val sortItems: List<BackupItem> = listOf(
        createItem(id = "0", filename = "Charlie", saveDate = "2024-01-01 00:00:00"),
        createItem(id = "1", filename = "Alpha", saveDate = "2024-03-01 00:00:00"),
        createItem(id = "2", filename = "Bravo", saveDate = "2024-02-01 00:00:00"),
    )

    /** Item with null gameTitle */
    val nullGameTitleItem: BackupItem = createItem(
        id = "0",
        filename = "SaveA",
        gameTitle = null,
        comment = "Test comment",
    )

    /** Item with all empty string fields */
    val emptyFieldsItem: BackupItem = createItem(
        id = "0",
        filename = "",
        gameTitle = "",
        comment = "",
    )

    /** 3 items with identical filenames for stable sort test */
    val duplicateNameItems: List<BackupItem> = listOf(
        createItem(id = "0", filename = "Alpha", saveDate = "2024-03-01 00:00:00"),
        createItem(id = "1", filename = "Alpha", saveDate = "2024-01-01 00:00:00"),
        createItem(id = "2", filename = "Alpha", saveDate = "2024-02-01 00:00:00"),
    )

    /** Generate a large list of items for performance tests */
    fun generateLargeList(count: Int): List<BackupItem> = (0 until count).map { i ->
        createItem(
            id = i.toString(),
            filename = "Save_${String.format("%04d", i)}",
            gameTitle = "Game_${i % 100}",
            comment = "Comment $i",
            saveDate = "2024-${String.format("%02d", (i % 12) + 1)}-${String.format("%02d", (i % 28) + 1)} 00:00:00",
        )
    }
}
