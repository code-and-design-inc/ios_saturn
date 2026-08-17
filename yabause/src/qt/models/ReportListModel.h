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

#ifndef REPORT_LIST_MODEL_H
#define REPORT_LIST_MODEL_H

#include <QAbstractListModel>
#include <QList>
#include "ReportData.h"

/**
 * @class ReportListModel
 * @brief Qt model for displaying a list of bug reports
 *
 * Responsibilities:
 * - Store QList<ReportData> for display in QListView
 * - Provide data access via Qt's Model/View architecture
 * - Emit signals when data changes (for automatic view updates)
 * - Support custom roles for rich display (timestamp, rating, reproducible flag)
 *
 * Thread Safety: Must be accessed only from Qt main thread.
 * Views automatically update when model emits dataChanged() signal.
 */
class ReportListModel : public QAbstractListModel {
    Q_OBJECT

public:
    /**
     * @brief Custom roles for data access beyond Qt::DisplayRole
     */
    enum ReportRoles {
        TimestampRole = Qt::UserRole + 1,  ///< qint64 Unix timestamp
        RatingRole,                         ///< int Rating (1-5)
        DescriptionRole,                    ///< QString Description text
        IsReproducibleRole,                 ///< bool Has save state + memory dump
        FormattedTimestampRole,             ///< QString Human-readable timestamp
        DocumentIdRole,                     ///< QString Firestore document ID
        ScreenshotUrlRole,                  ///< QString Screenshot URL
        SaveStateUrlRole,                   ///< QString Save state URL
        MemoryDumpUrlRole                   ///< QString Memory dump URL
    };

    explicit ReportListModel(QObject* parent = nullptr);
    virtual ~ReportListModel() = default;

    // QAbstractListModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    /**
     * @brief Replace model data with new report list
     * @param reports New list of reports
     *
     * Clears existing data and replaces with new reports.
     * Emits appropriate signals for view updates.
     */
    void setReports(const QList<ReportData>& reports);

    /**
     * @brief Get report at specific row
     * @param row Row index (0-based)
     * @return ReportData at row, or default-constructed if invalid
     */
    ReportData getReport(int row) const;

    /**
     * @brief Clear all reports from model
     */
    void clear();

    /**
     * @brief Get total number of reports
     * @return Number of reports in model
     */
    int count() const { return reports_.size(); }

    /**
     * @brief Check if model is empty
     * @return True if no reports in model
     */
    bool isEmpty() const { return reports_.isEmpty(); }

private:
    QList<ReportData> reports_;  ///< Internal storage for report data
};

#endif // REPORT_LIST_MODEL_H
