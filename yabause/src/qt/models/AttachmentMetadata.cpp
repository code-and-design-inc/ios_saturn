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

#include "AttachmentMetadata.h"

QString AttachmentMetadata::getHumanReadableSize() const {
    // Convert bytes to human-readable format
    if (fileSizeBytes < 1024) {
        return QString::number(fileSizeBytes) + " B";
    } else if (fileSizeBytes < 1024 * 1024) {
        double sizeKB = fileSizeBytes / 1024.0;
        return QString::number(sizeKB, 'f', 2) + " KB";
    } else {
        double sizeMB = fileSizeBytes / (1024.0 * 1024.0);
        return QString::number(sizeMB, 'f', 2) + " MB";
    }
}

int AttachmentMetadata::getDownloadProgress() const {
    // Return download progress as percentage (0-100)
    if (fileSizeBytes == 0) {
        return 0;
    }
    return static_cast<int>((downloadedBytes * 100) / fileSizeBytes);
}
