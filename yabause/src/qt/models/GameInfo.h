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

#ifndef GAME_INFO_H
#define GAME_INFO_H

#include <QString>

/**
 * @brief Information about a game for reproduction
 */
struct GameInfo {
    QString productNumber;    // Game product number (e.g., "T-1234G")
    QString gameTitle;        // Human-readable game title
    QString filePath;         // Path to game ISO/CUE file
};

#endif // GAME_INFO_H
