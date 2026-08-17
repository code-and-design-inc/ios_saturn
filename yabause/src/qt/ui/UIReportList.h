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

#ifndef UI_REPORT_LIST_H
#define UI_REPORT_LIST_H

#include "ui_UIReportList.h"
#include <QDialog>
#include <QString>
#include <QList>
#include "../models/ReportData.h"
#include "../services/ReproductionManager.h"

// Forward declarations
class FirestoreService;
class StorageService;
class ReportListModel;
class QProgressDialog;

namespace firebase {
    class App;
}

/**
 * @class UIReportList
 * @brief Dialog window for displaying game bug reports
 *
 * Responsibilities:
 * - Display list of bug reports for a specific game
 * - Handle report loading from Firestore
 * - Show loading states and empty states
 * - Allow refresh to reload reports
 * - Navigate to report reproduction on selection
 *
 * Thread Safety: UI operations must run on Qt main thread.
 * Firebase operations run asynchronously and signal completion.
 */
class UIReportList : public QDialog, private Ui::UIReportList {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param app Firebase App instance (must be initialized)
     * @param parent Parent widget
     */
    explicit UIReportList(firebase::App* app, QWidget* parent = nullptr);
    virtual ~UIReportList();

    /**
     * @brief Set game information for report loading
     * @param productNumber Game product number (e.g., "T-1234G")
     * @param gameTitle Human-readable game title
     */
    void setGameInfo(const QString& productNumber, const QString& gameTitle);

protected:
    /**
     * @brief Override to load reports when window is shown
     * @param event Show event
     */
    void showEvent(QShowEvent* event) override;

    /**
     * @brief Override to finish reproduction when dialog is closed
     * @param event Close event
     */
    void closeEvent(QCloseEvent* event) override;

private slots:
    /**
     * @brief Handle reports loaded successfully
     * @param reports List of loaded reports
     */
    void onReportsLoaded(const QList<ReportData>& reports);

    /**
     * @brief Handle report loading failure
     * @param error Error message
     * @param errorCode Firebase error code
     */
    void onLoadFailed(const QString& error, int errorCode);

    /**
     * @brief Handle refresh button click
     */
    void on_refreshButton_clicked();

    /**
     * @brief Handle loading stage changes
     * @param stage Current loading stage description
     */
    void onLoadingStageChanged(const QString& stage);

    /**
     * @brief Handle report list item double-click
     * @param index Clicked item index
     */
    void on_reportListView_doubleClicked(const QModelIndex& index);

    /**
     * @brief Handle download progress updates
     * @param downloadId Download operation ID
     * @param bytesTransferred Bytes downloaded so far
     * @param totalBytes Total file size
     */
    void onDownloadProgress(const QString& downloadId, qint64 bytesTransferred, qint64 totalBytes);

    /**
     * @brief Handle batch download progress
     * @param batchId Batch ID
     * @param completedFiles Number of completed files
     * @param totalFiles Total files in batch
     * @param totalBytesTransferred Total bytes transferred
     * @param totalBytes Total size of all files
     */
    void onBatchProgress(const QString& batchId, int completedFiles, int totalFiles,
                        qint64 totalBytesTransferred, qint64 totalBytes);

    /**
     * @brief Handle batch download completion
     * @param batchId Batch ID
     * @param filePaths Paths to downloaded files
     */
    void onBatchComplete(const QString& batchId, const QStringList& filePaths);

    /**
     * @brief Handle batch download failure
     * @param batchId Batch ID
     * @param error Error message
     */
    void onBatchFailed(const QString& batchId, const QString& error);

    /**
     * @brief Handle reproduction session ready
     * @param sessionId Session ID
     * @param downloadedFiles List of downloaded file paths
     */
    void onSessionReady(const QString& sessionId, const QStringList& downloadedFiles);

    /**
     * @brief Handle reproduction session failure
     * @param sessionId Session ID
     * @param error Error message
     */
    void onSessionFailed(const QString& sessionId, const QString& error);

    /**
     * @brief Handle reproduction download progress
     * @param sessionId Session ID
     * @param percentComplete Progress percentage (0-100)
     */
    void onReproductionProgress(const QString& sessionId, int percentComplete);

    /**
     * @brief Handle emulation launch request from reproduction manager
     * @param sessionId Session ID
     * @param gameInfo Game information
     * @param saveStatePath Path to save state file
     * @param memoryDumpPath Path to memory dump file
     */
    void onEmulationLaunchRequested(const QString& sessionId,
                                   const GameInfo& gameInfo,
                                   const QString& saveStatePath,
                                   const QString& memoryDumpPath);

    /**
     * @brief Handle image loaded from cache
     * @param url Image URL
     * @param pixmap Loaded pixmap
     */
    void onImageLoaded(const QString& url, const QPixmap& pixmap);

    /**
     * @brief Handle list scroll to load visible images
     */
    void onListScrolled();

private:
    /**
     * @brief Load reports from Firestore
     *
     * Shows loading indicator and calls FirestoreService to fetch reports.
     */
    void loadReports();

    /**
     * @brief Show or hide empty state label based on report count
     * @param isEmpty True if no reports available
     */
    void showEmptyState(bool isEmpty);

    /**
     * @brief Show or hide loading progress bar
     * @param loading True to show progress bar
     */
    void setLoading(bool loading);

    /**
     * @brief Format file list for display
     * @param filePaths List of file paths
     * @return Formatted string with file names
     */
    QString getFileList(const QStringList& filePaths) const;

    /**
     * @brief Format file size in human-readable format
     * @param bytes File size in bytes
     * @return Formatted size string (e.g., "1.5 MB")
     */
    QString formatFileSize(qint64 bytes) const;

    /**
     * @brief Load images for visible items only
     */
    void loadVisibleImages();

private:
    FirestoreService* firestoreService_;  ///< Service for loading reports from Firestore
    StorageService* storageService_;      ///< Service for downloading attachments
    ReproductionManager* reproductionManager_; ///< Manager for bug reproduction workflow
    ReportListModel* reportListModel_;    ///< Model for display reports in list view
    class ImageCacheManager* imageCacheManager_; ///< Manager for screenshot caching
    QProgressDialog* downloadProgressDialog_; ///< Progress dialog for downloads
    QString productNumber_;                ///< Current game product number
    QString gameTitle_;                    ///< Current game title
    QString currentBatchId_;               ///< Current download batch ID
    QString currentSessionId_;             ///< Current reproduction session ID
    firebase::App* app_;                   ///< Firebase app instance
    bool downloadInProgress_;              ///< Flag to prevent duplicate downloads
};

#endif // UI_REPORT_LIST_H
