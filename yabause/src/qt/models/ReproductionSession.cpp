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

#include "ReproductionSession.h"

bool ReproductionSession::isValid() const {
    // Session is valid if required files exist and status is not Failed
    return !saveStatePath.isEmpty() &&
           !memoryDumpPath.isEmpty() &&
           status != Failed;
}

qint64 ReproductionSession::getDurationMs() const {
    // Return duration from start to end (or current time if still running)
    if (endTime == 0) {
        // Session still running - calculate duration to now
        return QDateTime::currentMSecsSinceEpoch() - startTime;
    } else {
        // Session completed - return total duration
        return endTime - startTime;
    }
}
