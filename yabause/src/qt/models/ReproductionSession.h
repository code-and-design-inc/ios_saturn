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

#ifndef REPRODUCTION_SESSION_H
#define REPRODUCTION_SESSION_H

#include <QString>
#include <QMap>
#include <QVariant>
#include <QDateTime>
#include "GameInfo.h"

/**
 * @brief Represents the temporary state during bug reproduction workflow
 *
 * Managed in-memory during reproduction, not persisted to Firestore
 */
struct ReproductionSession {
    /**
     * @brief Session status enum
     */
    enum Status {
        Downloading,            ///< Files being downloaded
        Ready,                  ///< Files downloaded, ready to launch
        Running,                ///< Emulator running with reproduction
        Completed,              ///< Returned from emulator, prefs restored
        Failed                  ///< Error occurred
    };

    // Session identity
    QString sessionId;          ///< UUID for this reproduction session
    QString reportDocumentId;   ///< Reference to original report

    // Game information
    GameInfo gameInfo;          ///< Game information for launching emulation

    // Downloaded files
    QString screenshotPath;     ///< Local path to downloaded screenshot (optional)
    QString saveStatePath;      ///< Local path to downloaded save state (required)
    QString memoryDumpPath;     ///< Local path to downloaded memory dump (required)

    // Preserved state
    QMap<QString, QVariant> originalPreferences;  ///< User's settings before reproduction
    QString snapshotFilePath;   ///< Temporary file storing preference snapshot (crash recovery)

    // Session state
    Status status;              ///< Current session status
    QString errorMessage;       ///< Error details if status == Failed

    // Timestamps
    qint64 startTime;           ///< Session start timestamp
    qint64 endTime;             ///< Session end timestamp

    /**
     * @brief Check if session is valid for reproduction
     * @return true if required files exist and status is not Failed
     */
    bool isValid() const;

    /**
     * @brief Get session duration in milliseconds
     * @return Duration from start to end (or current time if still running)
     */
    qint64 getDurationMs() const;
};

#endif // REPRODUCTION_SESSION_H
