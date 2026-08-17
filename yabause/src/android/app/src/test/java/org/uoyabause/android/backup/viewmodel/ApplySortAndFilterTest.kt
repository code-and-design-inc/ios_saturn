package org.uoyabause.android.backup.viewmodel

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.uoyabause.android.backup.model.BackupSortMode

/**
 * Unit tests for BackupManagerViewModel.applySortAndFilter().
 * Covers UT-001~011 (normal), UT-E01~E05 (error), UT-B01~B05 (boundary).
 */
class ApplySortAndFilterTest {
    private lateinit var viewModel: TestableBackupManagerViewModel

    @Before
    fun setup() {
        viewModel = TestableBackupManagerViewModel()
    }

    // =============================================
    // 3.1 Normal cases (UT-001 ~ UT-011)
    // =============================================

    @Test
    fun `UT-001 empty query returns all items`() {
        val items = BackupItemTestFixtures.fiveItems
        val result = viewModel.applySortAndFilter(items, BackupSortMode.DATE_DESC, "")
        assertEquals(5, result.size)
    }

    @Test
    fun `UT-002 search by filename hits`() {
        val items = BackupItemTestFixtures.fiveItems
        val result = viewModel.applySortAndFilter(items, BackupSortMode.DATE_DESC, "Game1")
        assertEquals(1, result.size)
        assertEquals("Game1_Save", result[0].filename)
    }

    @Test
    fun `UT-003 search by gameTitle case-insensitive`() {
        val items = BackupItemTestFixtures.fiveItems
        val result = viewModel.applySortAndFilter(items, BackupSortMode.DATE_DESC, "sonic")
        assertEquals(1, result.size)
        assertEquals("Sonic", result[0].gameTitle)
    }

    @Test
    fun `UT-004 search does not match comment field`() {
        val items = BackupItemTestFixtures.fiveItems
        val result = viewModel.applySortAndFilter(items, BackupSortMode.DATE_DESC, "Boss")
        // "Boss clear" and "Final boss" are comments, but search only matches filename and gameTitle
        assertTrue(result.isEmpty())
    }

    @Test
    fun `UT-005 search no hits returns empty`() {
        val items = BackupItemTestFixtures.fiveItems
        val result = viewModel.applySortAndFilter(items, BackupSortMode.DATE_DESC, "NonExistentGame")
        assertTrue(result.isEmpty())
    }

    @Test
    fun `UT-006 null gameTitle still searchable by filename`() {
        val items = listOf(BackupItemTestFixtures.nullGameTitleItem)
        val result = viewModel.applySortAndFilter(items, BackupSortMode.DATE_DESC, "SaveA")
        assertEquals(1, result.size)
    }

    @Test
    fun `UT-007 NAME_ASC sorts alphabetically ascending`() {
        val items = BackupItemTestFixtures.sortItems
        val result = viewModel.applySortAndFilter(items, BackupSortMode.NAME_ASC, "")
        assertEquals("Alpha", result[0].filename)
        assertEquals("Bravo", result[1].filename)
        assertEquals("Charlie", result[2].filename)
    }

    @Test
    fun `UT-008 NAME_DESC sorts alphabetically descending`() {
        val items = BackupItemTestFixtures.sortItems
        val result = viewModel.applySortAndFilter(items, BackupSortMode.NAME_DESC, "")
        assertEquals("Charlie", result[0].filename)
        assertEquals("Bravo", result[1].filename)
        assertEquals("Alpha", result[2].filename)
    }

    @Test
    fun `UT-009 DATE_DESC sorts newest first`() {
        val items = BackupItemTestFixtures.sortItems
        val result = viewModel.applySortAndFilter(items, BackupSortMode.DATE_DESC, "")
        assertEquals("Alpha", result[0].filename) // 2024-03
        assertEquals("Bravo", result[1].filename) // 2024-02
        assertEquals("Charlie", result[2].filename) // 2024-01
    }

    @Test
    fun `UT-010 DATE_ASC sorts oldest first`() {
        val items = BackupItemTestFixtures.sortItems
        val result = viewModel.applySortAndFilter(items, BackupSortMode.DATE_ASC, "")
        assertEquals("Charlie", result[0].filename) // 2024-01
        assertEquals("Bravo", result[1].filename) // 2024-02
        assertEquals("Alpha", result[2].filename) // 2024-03
    }

    @Test
    fun `UT-011 search and sort combined`() {
        val items = BackupItemTestFixtures.fiveItems
        val result = viewModel.applySortAndFilter(items, BackupSortMode.NAME_ASC, "Save")
        // Items containing "Save" in filename: Game1_Save, Game2_Save, Game3_Save, Action_Save
        assertEquals(4, result.size)
        assertEquals("Action_Save", result[0].filename)
        assertEquals("Game1_Save", result[1].filename)
        assertEquals("Game2_Save", result[2].filename)
        assertEquals("Game3_Save", result[3].filename)
    }

    // =============================================
    // 3.2 Error cases (UT-E01 ~ UT-E05)
    // =============================================

    @Test
    fun `UT-E01 search on empty list returns empty`() {
        val result = viewModel.applySortAndFilter(emptyList(), BackupSortMode.DATE_DESC, "test")
        assertTrue(result.isEmpty())
    }

    @Test
    fun `UT-E02 sort on empty list returns empty`() {
        val result = viewModel.applySortAndFilter(emptyList(), BackupSortMode.NAME_ASC, "")
        assertTrue(result.isEmpty())
    }

    @Test
    fun `UT-E03 empty fields item does not crash`() {
        val items = listOf(BackupItemTestFixtures.emptyFieldsItem)
        val result = viewModel.applySortAndFilter(items, BackupSortMode.DATE_DESC, "x")
        assertTrue(result.isEmpty())
    }

    @Test
    fun `UT-E04 null gameTitle does not throw NPE`() {
        val items = listOf(BackupItemTestFixtures.nullGameTitleItem)
        val result = viewModel.applySortAndFilter(items, BackupSortMode.DATE_DESC, "Game")
        // gameTitle is null, filename is "SaveA" which doesn't contain "Game"
        assertTrue(result.isEmpty())
    }

    @Test
    fun `UT-E05 special regex characters in query are safe`() {
        val items = BackupItemTestFixtures.fiveItems
        val result = viewModel.applySortAndFilter(items, BackupSortMode.DATE_DESC, ".*+?[](){}^$")
        // contains() does not interpret regex, so no matches expected
        assertTrue(result.isEmpty())
    }

    // =============================================
    // 3.3 Boundary cases (UT-B01 ~ UT-B05)
    // =============================================

    @Test
    fun `UT-B01 single character query`() {
        val items = BackupItemTestFixtures.fiveItems
        val result = viewModel.applySortAndFilter(items, BackupSortMode.DATE_DESC, "A")
        // "Action_Save" filename and "Panzer Dragoon" gameTitle contain "a" (case-insensitive)
        assertTrue(result.isNotEmpty())
    }

    @Test
    fun `UT-B02 very long query does not crash`() {
        val items = BackupItemTestFixtures.fiveItems
        val longQuery = "A".repeat(256)
        val result = viewModel.applySortAndFilter(items, BackupSortMode.DATE_DESC, longQuery)
        assertTrue(result.isEmpty())
    }

    @Test
    fun `UT-B03 large list sorts correctly`() {
        val items = BackupItemTestFixtures.generateLargeList(1000)
        val result = viewModel.applySortAndFilter(items, BackupSortMode.NAME_ASC, "")
        assertEquals(1000, result.size)
        // Verify sorted order
        for (i in 0 until result.size - 1) {
            assertTrue(
                "Items at $i and ${i + 1} are not sorted",
                result[i].filename.lowercase() <= result[i + 1].filename.lowercase(),
            )
        }
    }

    @Test
    fun `UT-B04 stable sort with duplicate names`() {
        val items = BackupItemTestFixtures.duplicateNameItems
        val result = viewModel.applySortAndFilter(items, BackupSortMode.NAME_ASC, "")
        assertEquals(3, result.size)
        // All have the same name "Alpha", so original relative order should be preserved (stable sort)
        assertEquals("0", result[0].id)
        assertEquals("1", result[1].id)
        assertEquals("2", result[2].id)
    }

    @Test
    fun `UT-B05 inconsistent date format does not crash`() {
        val items = listOf(
            BackupItemTestFixtures.createItem(id = "0", filename = "A", saveDate = "2024/01/01"),
            BackupItemTestFixtures.createItem(id = "1", filename = "B", saveDate = "2024-01-02"),
        )
        // Should not crash, best-effort sort by string comparison
        val result = viewModel.applySortAndFilter(items, BackupSortMode.DATE_ASC, "")
        assertEquals(2, result.size)
    }
}
