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

#ifndef REPORT_DATA_H
#define REPORT_DATA_H

#include <QString>
#include <QMap>
#include <QDateTime>

/**
 * @brief Represents a single user-submitted bug report for a game
 *
 * Maps to Firestore document in games/{gameId}/ratings/{ratingId} collection
 */
struct ReportData {
    // Core identification
    QString documentId;         ///< Firestore document ID
    QString userId;             ///< User who submitted the report
    QString gameCode;           ///< Product number (e.g., "T-1234G")

    // Report content
    qint64 timestamp;           ///< Unix timestamp (milliseconds)
    int rating;                 ///< User rating (1-5 stars)
    QString description;        ///< Bug description text
    bool isVisible;             ///< Admin moderation flag

    // Attachment references (Firebase Storage URLs)
    QString screenshotUrl;      ///< Screenshot image URL (optional)
    QString saveStateUrl;       ///< Save state file URL (optional)
    QString memoryDumpUrl;      ///< Memory dump file URL (optional)

    // Reproduction metadata
    QMap<QString, QString> reproductionPreferences;  ///< Emulator settings at time of report

    /**
     * @brief Check if this report can be reproduced
     * @return true if report has both save state and memory dump
     */
    bool isReproducible() const;

    /**
     * @brief Get formatted timestamp string
     * @return Timestamp in "yyyy-MM-dd hh:mm:ss" format
     */
    QString getFormattedTimestamp() const;
};

#endif // REPORT_DATA_H
