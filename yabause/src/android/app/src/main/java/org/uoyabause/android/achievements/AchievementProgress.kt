package org.uoyabause.android.achievements

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

/**
 * RetroAchievements achievement progress data for a specific game
 *
 * @property gameId RetroAchievements Game ID
 * @property totalAchievements Total number of achievements available
 * @property unlockedAchievements Number of achievements unlocked (normal + hardcore)
 * @property unlockedHardcore Number of achievements unlocked in hardcore mode only
 * @property completionPercent Completion percentage string (e.g., "25.5")
 * @property completionPercentHardcore Hardcore completion percentage string (e.g., "12.7")
 * @property imageIcon URL to the game's icon/badge image
 */
data class AchievementProgress(
    val gameId: Int,
    val totalAchievements: Int,
    val unlockedAchievements: Int,
    val unlockedHardcore: Int = 0,
    val completionPercent: String = "0.0",
    val completionPercentHardcore: String = "0.0",
    val imageIcon: String = "",
)
