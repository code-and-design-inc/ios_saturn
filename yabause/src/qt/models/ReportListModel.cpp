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

#include "ReportListModel.h"
#include <QDebug>

ReportListModel::ReportListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int ReportListModel::rowCount(const QModelIndex& parent) const
{
    // List models don't have hierarchical structure, so parent should always be invalid
    if (parent.isValid()) {
        return 0;
    }

    return reports_.size();
}

QVariant ReportListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= reports_.size()) {
        return QVariant();
    }

    const ReportData& report = reports_.at(index.row());

    switch (role) {
        case Qt::DisplayRole:
            // Default display: description preview
            return report.description.left(100); // First 100 characters

        case TimestampRole:
            return report.timestamp;

        case RatingRole:
            return report.rating;

        case DescriptionRole:
            return report.description;

        case IsReproducibleRole:
            return report.isReproducible();

        case FormattedTimestampRole:
            return report.getFormattedTimestamp();

        case DocumentIdRole:
            return report.documentId;

        case ScreenshotUrlRole:
            return report.screenshotUrl;

        case SaveStateUrlRole:
            return report.saveStateUrl;

        case MemoryDumpUrlRole:
            return report.memoryDumpUrl;

        default:
            return QVariant();
    }
}

QHash<int, QByteArray> ReportListModel::roleNames() const
{
    // Define role names for QML access (optional, but good practice)
    QHash<int, QByteArray> roles;
    roles[TimestampRole] = "timestamp";
    roles[RatingRole] = "rating";
    roles[DescriptionRole] = "description";
    roles[IsReproducibleRole] = "isReproducible";
    roles[FormattedTimestampRole] = "formattedTimestamp";
    roles[DocumentIdRole] = "documentId";
    roles[ScreenshotUrlRole] = "screenshotUrl";
    roles[SaveStateUrlRole] = "saveStateUrl";
    roles[MemoryDumpUrlRole] = "memoryDumpUrl";
    return roles;
}

void ReportListModel::setReports(const QList<ReportData>& reports)
{
    // Signal that we're about to reset the model
    beginResetModel();

    // Replace data
    reports_ = reports;

    // Signal that model reset is complete
    endResetModel();

    qDebug() << "ReportListModel updated with" << reports_.size() << "reports";
}

ReportData ReportListModel::getReport(int row) const
{
    if (row < 0 || row >= reports_.size()) {
        qWarning() << "Invalid row index:" << row;
        return ReportData(); // Return default-constructed report
    }

    return reports_.at(row);
}

void ReportListModel::clear()
{
    if (reports_.isEmpty()) {
        return; // Already empty, no need to signal
    }

    beginResetModel();
    reports_.clear();
    endResetModel();

    qDebug() << "ReportListModel cleared";
}
