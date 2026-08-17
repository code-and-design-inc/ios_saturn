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

#ifndef ATTACHMENT_METADATA_H
#define ATTACHMENT_METADATA_H

#include <QString>

/**
 * @brief Represents metadata for a downloadable file associated with a report
 *
 * Derived from Firebase Storage metadata and local filesystem after download
 */
struct AttachmentMetadata {
    /**
     * @brief Type of attachment
     */
    enum Type {
        Screenshot,
        SaveState,
        MemoryDump
    };

    // Identity
    QString storageUrl;         ///< Firebase Storage download URL
    Type type;                  ///< Attachment type
    QString fileName;           ///< Original filename

    // Storage info
    qint64 fileSizeBytes;       ///< File size in bytes
    QString contentType;        ///< MIME type (image/png, application/octet-stream)

    // Download state
    QString localPath;          ///< Path where file is downloaded (empty if not downloaded)
    qint64 downloadedBytes;     ///< Progress tracking
    bool isDownloaded;          ///< Download completion flag

    /**
     * @brief Get human-readable file size
     * @return File size as string (e.g., "1.5 MB", "512 KB", "128 B")
     */
    QString getHumanReadableSize() const;

    /**
     * @brief Get download progress percentage
     * @return Progress as integer 0-100
     */
    int getDownloadProgress() const;
};

#endif // ATTACHMENT_METADATA_H
