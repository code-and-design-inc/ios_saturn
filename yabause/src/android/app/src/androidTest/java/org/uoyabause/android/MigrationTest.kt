package org.uoyabause.android

import androidx.room.Room
import androidx.room.testing.MigrationTestHelper
import androidx.sqlite.db.framework.FrameworkSQLiteOpenHelperFactory
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import java.io.IOException

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

@RunWith(AndroidJUnit4::class)
class MigrationTest {
    private val TEST_DB = "migration-test"

    @get:Rule
    val helper: MigrationTestHelper =
        MigrationTestHelper(
            InstrumentationRegistry.getInstrumentation(),
            GameInfoDatabase::class.java,
            emptyList(),
            FrameworkSQLiteOpenHelperFactory(),
        )

    // ========== IT-DB-001: マイグレーション成功 ==========
    @Test
    @Throws(IOException::class)
    fun migrate1To2_successfullyMigrates() {
        // Create the database with version 1
        helper.createDatabase(TEST_DB, 1).apply {
            // Insert test data (10 GameInfo records)
            for (i in 1..10) {
                execSQL(
                    """
                    INSERT INTO GameInfo (
                        file_path, iso_file_path, game_title, maker_id, product_number,
                        version, release_date, device_infomation, area, input_device,
                        rating
                    ) VALUES (
                        '/path/to/game$i.iso', '/path/to/game$i.iso', 'Test Game $i',
                        'SEGA', 'T-123$i', 'V1.00', '19960101', 'CD-1/1',
                        'JUE', 'JOY', 0
                    )
                    """.trimIndent(),
                )
            }
            close()
        }

        // Run the migration
        val db =
            helper.runMigrationsAndValidate(
                TEST_DB,
                2,
                true,
                GameInfoDatabase.MIGRATION_1_2,
            )

        // Verify migration success: check that all 10 records still exist
        val cursor = db.query("SELECT * FROM GameInfo")
        Assert.assertEquals("All 10 records should exist after migration", 10, cursor.count)

        // Verify that new columns exist and have default values
        if (cursor.moveToFirst()) {
            val raGameIdIndex = cursor.getColumnIndex("ra_game_id")
            val raHashIndex = cursor.getColumnIndex("ra_hash")
            val raUnlockedIndex = cursor.getColumnIndex("ra_unlocked")
            val raTotalIndex = cursor.getColumnIndex("ra_total")
            val raLastUpdateIndex = cursor.getColumnIndex("ra_last_update")
            val raNotSupportedIndex = cursor.getColumnIndex("ra_not_supported")

            Assert.assertTrue("ra_game_id column should exist", raGameIdIndex >= 0)
            Assert.assertTrue("ra_hash column should exist", raHashIndex >= 0)
            Assert.assertTrue("ra_unlocked column should exist", raUnlockedIndex >= 0)
            Assert.assertTrue("ra_total column should exist", raTotalIndex >= 0)
            Assert.assertTrue("ra_last_update column should exist", raLastUpdateIndex >= 0)
            Assert.assertTrue("ra_not_supported column should exist", raNotSupportedIndex >= 0)

            // Check default values
            Assert.assertTrue("ra_game_id should be null", cursor.isNull(raGameIdIndex))
            Assert.assertTrue("ra_hash should be null", cursor.isNull(raHashIndex))
            Assert.assertEquals("ra_unlocked should be 0", 0, cursor.getInt(raUnlockedIndex))
            Assert.assertEquals("ra_total should be 0", 0, cursor.getInt(raTotalIndex))
            Assert.assertTrue("ra_last_update should be null", cursor.isNull(raLastUpdateIndex))
            Assert.assertEquals("ra_not_supported should be 0 (false)", 0, cursor.getInt(raNotSupportedIndex))
        }

        cursor.close()
        db.close()
    }

    // ========== IT-DB-002: GameInfoのCRUD操作 ==========
    @Test
    fun migrate1To2_crudOperationsWork() {
        // Create and migrate database
        helper.createDatabase(TEST_DB, 1).apply {
            close()
        }

        val db =
            helper.runMigrationsAndValidate(
                TEST_DB,
                2,
                true,
                GameInfoDatabase.MIGRATION_1_2,
            )

        // Test INSERT with RA fields
        db.execSQL(
            """
            INSERT INTO GameInfo (
                file_path, game_title, product_number, ra_game_id, ra_hash,
                ra_unlocked, ra_total, ra_not_supported
            ) VALUES (
                '/test/game.iso', 'Test Game', 'T-9999', 12345, 'a1b2c3d4e5f6789012345678901234ab',
                10, 50, 0
            )
            """.trimIndent(),
        )

        // Test SELECT
        val cursor = db.query("SELECT * FROM GameInfo WHERE product_number = 'T-9999'")
        Assert.assertEquals("Record should be inserted", 1, cursor.count)

        if (cursor.moveToFirst()) {
            val raGameIdIndex = cursor.getColumnIndex("ra_game_id")
            val raHashIndex = cursor.getColumnIndex("ra_hash")
            val raUnlockedIndex = cursor.getColumnIndex("ra_unlocked")
            val raTotalIndex = cursor.getColumnIndex("ra_total")

            Assert.assertEquals("ra_game_id should be 12345", 12345, cursor.getInt(raGameIdIndex))
            Assert.assertEquals("ra_hash should match", "a1b2c3d4e5f6789012345678901234ab", cursor.getString(raHashIndex))
            Assert.assertEquals("ra_unlocked should be 10", 10, cursor.getInt(raUnlockedIndex))
            Assert.assertEquals("ra_total should be 50", 50, cursor.getInt(raTotalIndex))
        }

        cursor.close()

        // Test UPDATE
        db.execSQL(
            """
            UPDATE GameInfo SET ra_unlocked = 15, ra_total = 50 WHERE product_number = 'T-9999'
            """.trimIndent(),
        )

        val cursorAfterUpdate = db.query("SELECT ra_unlocked, ra_total FROM GameInfo WHERE product_number = 'T-9999'")
        if (cursorAfterUpdate.moveToFirst()) {
            Assert.assertEquals("ra_unlocked should be updated to 15", 15, cursorAfterUpdate.getInt(0))
            Assert.assertEquals("ra_total should remain 50", 50, cursorAfterUpdate.getInt(1))
        }
        cursorAfterUpdate.close()

        // Test DELETE
        db.execSQL("DELETE FROM GameInfo WHERE product_number = 'T-9999'")
        val cursorAfterDelete = db.query("SELECT * FROM GameInfo WHERE product_number = 'T-9999'")
        Assert.assertEquals("Record should be deleted", 0, cursorAfterDelete.count)
        cursorAfterDelete.close()

        db.close()
    }

    // ========== RT-DB-001: 旧バージョンDBからの移行 ==========
    @Test
    fun migrate1To2_preservesExistingData() {
        // Create database with version 1 and insert data with various fields
        helper.createDatabase(TEST_DB, 1).apply {
            execSQL(
                """
                INSERT INTO GameInfo (
                    file_path, iso_file_path, game_title, maker_id, product_number,
                    version, release_date, device_infomation, area, input_device,
                    rating, image_url
                ) VALUES (
                    '/games/virtua_fighter.iso', '/games/virtua_fighter.iso', 'Virtua Fighter',
                    'SEGA ENTERPRISES', 'GS-9001', 'V1.000', '19941122', 'CD-1/1',
                    'JUE', 'JOY', 5, 'https://example.com/vf.png'
                )
                """.trimIndent(),
            )
            close()
        }

        // Run migration
        val db =
            helper.runMigrationsAndValidate(
                TEST_DB,
                2,
                true,
                GameInfoDatabase.MIGRATION_1_2,
            )

        // Verify all existing data is preserved
        val cursor = db.query("SELECT * FROM GameInfo WHERE product_number = 'GS-9001'")
        Assert.assertEquals("Record should exist after migration", 1, cursor.count)

        if (cursor.moveToFirst()) {
            Assert.assertEquals("game_title should be preserved", "Virtua Fighter", cursor.getString(cursor.getColumnIndex("game_title")))
            Assert.assertEquals("maker_id should be preserved", "SEGA ENTERPRISES", cursor.getString(cursor.getColumnIndex("maker_id")))
            Assert.assertEquals("rating should be preserved", 5, cursor.getInt(cursor.getColumnIndex("rating")))
            Assert.assertEquals("image_url should be preserved", "https://example.com/vf.png", cursor.getString(cursor.getColumnIndex("image_url")))

            // Verify new columns have default values
            Assert.assertTrue("ra_game_id should be null", cursor.isNull(cursor.getColumnIndex("ra_game_id")))
            Assert.assertEquals("ra_unlocked should be 0", 0, cursor.getInt(cursor.getColumnIndex("ra_unlocked")))
        }

        cursor.close()
        db.close()
    }

    // ========== RT-DB-002: 新規インストール ==========
    @Test
    fun newInstallation_createsSchemaWithVersion2() {
        // Create a new database directly with version 2 (simulating fresh install)
        val db =
            Room
                .databaseBuilder(
                    InstrumentationRegistry.getInstrumentation().targetContext,
                    GameInfoDatabase::class.java,
                    "new-install-test",
                ).build()

        // Verify version
        val cursor = db.openHelper.writableDatabase.query("PRAGMA user_version")
        if (cursor.moveToFirst()) {
            Assert.assertEquals("Database version should be 2", 2, cursor.getInt(0))
        }
        cursor.close()

        // Verify that all RA columns exist
        val gameInfoDao = db.gameInfoDao()
        val testGame =
            GameInfo(
                file_path = "/test.iso",
                game_title = "Test",
                product_number = "T-TEST",
                raGameId = 999,
                raHash = "0123456789abcdef0123456789abcdef",
                raUnlocked = 5,
                raTotal = 10,
                raNotSupported = false,
            )

        // Should not throw exception
        gameInfoDao.insertAll(testGame)

        val retrieved = gameInfoDao.findByProductId("T-TEST", "")
        Assert.assertNotNull("Game should be inserted", retrieved)
        Assert.assertEquals("raGameId should match", 999, retrieved.raGameId)
        Assert.assertEquals("raUnlocked should match", 5, retrieved.raUnlocked)

        db.close()
    }
}
