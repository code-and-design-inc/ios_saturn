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

#include "UIReportList.h"
#include "UIYabause.h"
#include "../QtYabause.h"
#include "../filesearchwidget.h"
#include "../services/FirestoreServiceImpl.h"
#include "../services/StorageServiceImpl.h"
#include "../services/ReproductionManagerImpl.h"
#include "../services/ImageCacheManager.h"
#include "../models/ReportListModel.h"
#include "../models/ReportData.h"
#include "../models/AttachmentMetadata.h"
#include "../widgets/ReportListItemDelegate.h"
#include "../widgets/ToastNotification.h"
#include <QMessageBox>
#include <QProgressDialog>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QCloseEvent>
#include <QDebug>
#include <QScrollBar>

UIReportList::UIReportList(firebase::App* app, QWidget* parent)
    : QDialog(parent)
    , firestoreService_(nullptr)
    , storageService_(nullptr)
    , reproductionManager_(nullptr)
    , reportListModel_(nullptr)
    , imageCacheManager_(nullptr)
    , downloadProgressDialog_(nullptr)
    , app_(app)
    , downloadInProgress_(false)
{
    // Setup UI from .ui file
    setupUi(this);

    // Create Firestore service
    firestoreService_ = new FirestoreServiceImpl(app, this);

    // Create Storage service
    qDebug() << "Creating StorageServiceImpl with app:" << (void*)app;
    storageService_ = new StorageServiceImpl(app, this);

    if (!storageService_) {
        qCritical() << "Failed to create StorageService!";
    } else {
        qDebug() << "StorageService created successfully:" << (void*)storageService_;
    }

    // Create Reproduction manager
    qDebug() << "Creating ReproductionManagerImpl";
    reproductionManager_ = new ReproductionManagerImpl(storageService_, this);

    if (!reproductionManager_) {
        qCritical() << "Failed to create ReproductionManager!";
    } else {
        qDebug() << "ReproductionManager created successfully:" << (void*)reproductionManager_;
    }

    // Create image cache manager
    imageCacheManager_ = new ImageCacheManager(this);

    // Create report list model
    reportListModel_ = new ReportListModel(this);

    // Set model on list view
    reportListView->setModel(reportListModel_);

    // Set custom delegate for report items
    ReportListItemDelegate* delegate = new ReportListItemDelegate(this);
    delegate->setImageCacheManager(imageCacheManager_);
    reportListView->setItemDelegate(delegate);

    // Connect Firestore service signals
    connect(firestoreService_, &FirestoreService::reportsLoaded,
            this, &UIReportList::onReportsLoaded);
    connect(firestoreService_, &FirestoreService::loadFailed,
            this, &UIReportList::onLoadFailed);
    connect(firestoreService_, &FirestoreService::loadingStageChanged,
            this, &UIReportList::onLoadingStageChanged);

    // Connect Storage service signals
    connect(storageService_, &StorageService::batchProgress,
            this, &UIReportList::onBatchProgress);
    connect(storageService_, &StorageService::batchComplete,
            this, &UIReportList::onBatchComplete);
    connect(storageService_, &StorageService::batchFailed,
            this, &UIReportList::onBatchFailed);

    // Connect ReproductionManager signals
    connect(reproductionManager_, &ReproductionManager::sessionReady,
            this, &UIReportList::onSessionReady);
    connect(reproductionManager_, &ReproductionManager::sessionFailed,
            this, &UIReportList::onSessionFailed);
    connect(reproductionManager_, &ReproductionManager::downloadProgress,
            this, &UIReportList::onReproductionProgress);
    connect(reproductionManager_, &ReproductionManager::requestEmulationLaunch,
            this, &UIReportList::onEmulationLaunchRequested);

    // Connect double-click on report list
    connect(reportListView, &QListView::doubleClicked,
            this, &UIReportList::on_reportListView_doubleClicked);

    // Connect image cache manager signals
    connect(imageCacheManager_, &ImageCacheManager::imageLoaded,
            this, &UIReportList::onImageLoaded);

    // Connect scroll events to load visible images
    connect(reportListView->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &UIReportList::onListScrolled);

    // Initially hide progress bar and empty state
    progressBar->setVisible(false);
    emptyStateLabel->setVisible(false);
}

UIReportList::~UIReportList()
{
    // Qt parent-child relationship handles cleanup
}

void UIReportList::setGameInfo(const QString& productNumber, const QString& gameTitle)
{
    productNumber_ = productNumber;
    gameTitle_ = gameTitle;

    // Update window title
    QString windowTitle = QString("Bug Reports - %1").arg(gameTitle);
    setWindowTitle(windowTitle);

    // Update title label
    titleLabel->setText(QString("Bug Reports for %1").arg(gameTitle));
}

void UIReportList::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    // Load reports when window is first shown
    if (!productNumber_.isEmpty()) {
        loadReports();
    }
}

void UIReportList::closeEvent(QCloseEvent* event)
{
    // Finish reproduction session if one is active
    if (!currentSessionId_.isEmpty()) {
        qDebug() << "Dialog closing, finishing reproduction session:" << currentSessionId_;
        reproductionManager_->finishReproduction(currentSessionId_);
        currentSessionId_.clear();
    }

    // Accept the close event
    QDialog::closeEvent(event);
}

void UIReportList::loadReports()
{
    if (productNumber_.isEmpty()) {
        qWarning() << "Cannot load reports: product number not set";
        return;
    }

    // Clear existing reports
    reportListModel_->clear();

    // Show loading state
    setLoading(true);
    showEmptyState(false);

    // Request reports from Firestore
    firestoreService_->loadReportsForGame(productNumber_);
}

void UIReportList::onReportsLoaded(const QList<ReportData>& reports)
{
    // Hide loading indicator
    setLoading(false);

    // Update model with new reports
    reportListModel_->setReports(reports);

    // Show empty state if no reports
    showEmptyState(reports.isEmpty());

    qDebug() << "Loaded" << reports.size() << "reports for" << productNumber_;

    // Load images for visible items
    if (!reports.isEmpty()) {
        loadVisibleImages();
    }
}

void UIReportList::onLoadFailed(const QString& error, int errorCode)
{
    // Hide loading indicator
    setLoading(false);

    // Show error message
    QMessageBox::critical(this,
                          "Error Loading Reports",
                          QString("Failed to load bug reports:\n%1\n\nError code: %2")
                              .arg(error)
                              .arg(errorCode));

    // Show empty state
    showEmptyState(true);

    qWarning() << "Failed to load reports:" << error << "(code:" << errorCode << ")";
}

void UIReportList::on_refreshButton_clicked()
{
    // Reload reports
    loadReports();
}

void UIReportList::onLoadingStageChanged(const QString& stage)
{
    // Could update a status label here if needed
    qDebug() << "Loading stage:" << stage;
}

void UIReportList::showEmptyState(bool isEmpty)
{
    // Show/hide list view and empty state label
    reportListView->setVisible(!isEmpty);
    emptyStateLabel->setVisible(isEmpty);
}

void UIReportList::setLoading(bool loading)
{
    // Show/hide progress bar
    progressBar->setVisible(loading);

    // Set indeterminate progress (animated)
    if (loading) {
        progressBar->setRange(0, 0); // Indeterminate mode
    } else {
        progressBar->setRange(0, 100);
        progressBar->setValue(0);
    }

    // Disable refresh button during loading
    refreshButton->setEnabled(!loading);
}

void UIReportList::on_reportListView_doubleClicked(const QModelIndex& index)
{
    qDebug() << "Double-click event triggered on index:" << index.row();

    // Prevent duplicate downloads
    if (downloadInProgress_) {
        qWarning() << "Download already in progress, ignoring duplicate double-click event";
        return;
    }

    if (!index.isValid()) {
        qWarning() << "Invalid index in double-click handler";
        return;
    }

    // Get report from model
    ReportData report = reportListModel_->getReport(index.row());

    qDebug() << "Report data:";
    qDebug() << "  Document ID:" << report.documentId;
    qDebug() << "  Description:" << report.description.left(50);
    qDebug() << "  Screenshot URL:" << report.screenshotUrl;
    qDebug() << "  Save State URL:" << report.saveStateUrl;
    qDebug() << "  Memory Dump URL:" << report.memoryDumpUrl;
    qDebug() << "  isReproducible():" << report.isReproducible();

    // Check if report is reproducible
    if (!report.isReproducible()) {
        QMessageBox::information(this,
                                "Cannot Reproduce",
                                QString("This report cannot be reproduced because it is missing required attachments.\n\n"
                                "Required:\n"
                                "- Save state file %1\n"
                                "- Memory dump file %2")
                                .arg(report.saveStateUrl.isEmpty() ? "(MISSING)" : "(OK)")
                                .arg(report.memoryDumpUrl.isEmpty() ? "(MISSING)" : "(OK)"));
        return;
    }

    // Show confirmation dialog for reproduction
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Reproduce Bug",
        QString("Do you want to reproduce this bug?\n\n"
                "Report: %1\n"
                "Rating: %2/5\n\n"
                "This will:\n"
                "1. Download attachments (screenshot, save state, memory dump)\n"
                "2. Save your current emulator settings\n"
                "3. Load the save state to reproduce the bug\n"
                "4. Restore your settings when you close the report list\n\n"
                "Do you want to continue?")
            .arg(report.description.left(100))
            .arg(report.rating),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    // Prepare game info for reproduction
    GameInfo gameInfo;
    gameInfo.productNumber = productNumber_;
    gameInfo.gameTitle = gameTitle_;
    gameInfo.filePath = "";  // Will be filled in by user if needed

    // Create progress dialog
    downloadProgressDialog_ = new QProgressDialog(
        "Preparing reproduction...",
        "Cancel",
        0,
        100,
        this);
    downloadProgressDialog_->setWindowModality(Qt::WindowModal);
    downloadProgressDialog_->setMinimumDuration(0);
    downloadProgressDialog_->setValue(0);

    // Connect cancel button
    connect(downloadProgressDialog_, &QProgressDialog::canceled, this, [this]() {
        if (!currentSessionId_.isEmpty()) {
            qDebug() << "User cancelled reproduction, cancelling session:" << currentSessionId_;
            reproductionManager_->cancelReproduction(currentSessionId_);
            downloadProgressDialog_->deleteLater();
            downloadProgressDialog_ = nullptr;
            currentSessionId_.clear();
            downloadInProgress_ = false;  // Reset download flag
        }
    });

    // Set download in progress flag
    downloadInProgress_ = true;

    // Check if reproduction manager is valid
    if (!reproductionManager_) {
        qCritical() << "ReproductionManager is null! Cannot start reproduction.";
        downloadInProgress_ = false;
        QMessageBox::critical(this, "Error", "Reproduction manager is not initialized.");
        return;
    }

    qDebug() << "Starting reproduction preparation for report:" << report.documentId;

    // Start reproduction preparation (downloads files and prepares session)
    currentSessionId_ = reproductionManager_->prepareReproduction(report, gameInfo);

    if (currentSessionId_.isEmpty()) {
        qCritical() << "Failed to start reproduction";
        downloadInProgress_ = false;
        QMessageBox::critical(this, "Error", "Failed to start bug reproduction.");
        if (downloadProgressDialog_) {
            downloadProgressDialog_->deleteLater();
            downloadProgressDialog_ = nullptr;
        }
        return;
    }

    qDebug() << "Started reproduction session:" << currentSessionId_;
}

void UIReportList::onDownloadProgress(const QString& downloadId, qint64 bytesTransferred, qint64 totalBytes)
{
    // Individual file progress - not used for batch progress dialog
    Q_UNUSED(downloadId);
    Q_UNUSED(bytesTransferred);
    Q_UNUSED(totalBytes);
}

void UIReportList::onBatchProgress(const QString& batchId, int completedFiles, int totalFiles,
                                  qint64 totalBytesTransferred, qint64 totalBytes)
{
    if (batchId != currentBatchId_) {
        return;
    }

    if (!downloadProgressDialog_) {
        return;
    }

    // Calculate overall percentage
    int percentage = 0;
    if (totalBytes > 0) {
        percentage = static_cast<int>((totalBytesTransferred * 100) / totalBytes);
    }

    // Update progress dialog
    downloadProgressDialog_->setLabelText(
        QString("Downloading attachments... (%1/%2 files)")
            .arg(completedFiles)
            .arg(totalFiles));
    downloadProgressDialog_->setValue(percentage);

    qDebug() << "Batch progress:" << percentage << "% (" << completedFiles << "/" << totalFiles << "files)";
}

void UIReportList::onBatchComplete(const QString& batchId, const QStringList& filePaths)
{
    if (batchId != currentBatchId_) {
        return;
    }

    qDebug() << "Batch download complete:" << batchId << "-" << filePaths.size() << "files";

    // Close progress dialog
    if (downloadProgressDialog_) {
        downloadProgressDialog_->deleteLater();
        downloadProgressDialog_ = nullptr;
    }

    currentBatchId_.clear();
    downloadInProgress_ = false;  // Reset download flag

    // Get download directory
    QString downloadDir;
    if (!filePaths.isEmpty()) {
        downloadDir = QFileInfo(filePaths.first()).absolutePath();
    }

    // Show success message with option to open folder
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Download Complete");
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setText(QString("Successfully downloaded %1 file(s).").arg(filePaths.size()));
    msgBox.setInformativeText(QString("Files are stored in:\n%1\n\n"
                                      "Downloaded files:\n%2")
                                .arg(downloadDir)
                                .arg(getFileList(filePaths)));

    QPushButton* openFolderButton = msgBox.addButton("Open Folder", QMessageBox::ActionRole);
    msgBox.addButton(QMessageBox::Close);

    msgBox.exec();

    // Check which button was clicked
    if (msgBox.clickedButton() == openFolderButton) {
        // Open folder in file explorer
        if (!downloadDir.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(downloadDir));
            qDebug() << "Opening folder:" << downloadDir;
        }
    }

    // TODO: Implement reproduction (Phase 5)
    // For now, user can manually check the downloaded files
}

void UIReportList::onBatchFailed(const QString& batchId, const QString& error)
{
    if (batchId != currentBatchId_) {
        return;
    }

    qWarning() << "Batch download failed:" << batchId << "-" << error;

    // Close progress dialog
    if (downloadProgressDialog_) {
        downloadProgressDialog_->deleteLater();
        downloadProgressDialog_ = nullptr;
    }

    currentBatchId_.clear();
    downloadInProgress_ = false;  // Reset download flag

    // Show error message with retry option
    QMessageBox::StandardButton reply = QMessageBox::critical(
        this,
        "Download Failed",
        QString("Failed to download attachments:\n%1\n\nDo you want to retry?").arg(error),
        QMessageBox::Retry | QMessageBox::Cancel);

    if (reply == QMessageBox::Retry) {
        // Get the current report and retry download
        QModelIndex currentIndex = reportListView->currentIndex();
        if (currentIndex.isValid()) {
            on_reportListView_doubleClicked(currentIndex);
        }
    }
}

QString UIReportList::getFileList(const QStringList& filePaths) const
{
    QStringList fileNames;
    for (const QString& filePath : filePaths) {
        QFileInfo fileInfo(filePath);
        fileNames.append(QString("- %1 (%2)")
                        .arg(fileInfo.fileName())
                        .arg(formatFileSize(fileInfo.size())));
    }
    return fileNames.join("\n");
}

QString UIReportList::formatFileSize(qint64 bytes) const
{
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;

    if (bytes >= GB) {
        return QString::number(bytes / (double)GB, 'f', 2) + " GB";
    } else if (bytes >= MB) {
        return QString::number(bytes / (double)MB, 'f', 2) + " MB";
    } else if (bytes >= KB) {
        return QString::number(bytes / (double)KB, 'f', 2) + " KB";
    } else {
        return QString::number(bytes) + " bytes";
    }
}

void UIReportList::onSessionReady(const QString& sessionId, const QStringList& downloadedFiles)
{
    if (sessionId != currentSessionId_) {
        return;
    }

    qDebug() << "Reproduction session ready:" << sessionId << "-" << downloadedFiles.size() << "files";

    // Close progress dialog
    if (downloadProgressDialog_) {
        downloadProgressDialog_->deleteLater();
        downloadProgressDialog_ = nullptr;
    }

    downloadInProgress_ = false;

    // Show ready dialog with option to launch
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Ready to Reproduce");
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setText("Files downloaded successfully. Ready to reproduce bug.");
    msgBox.setInformativeText(QString("Downloaded files:\n%1\n\n"
                                     "Click 'Launch' to load the save state and reproduce the bug.\n"
                                     "Your current settings will be preserved.")
                                .arg(getFileList(downloadedFiles)));

    QPushButton* launchButton = msgBox.addButton("Launch Reproduction", QMessageBox::ActionRole);
    msgBox.addButton(QMessageBox::Cancel);

    msgBox.exec();

    // Check which button was clicked
    if (msgBox.clickedButton() == launchButton) {
        qDebug() << "User chose to launch reproduction";
        reproductionManager_->launchReproduction(sessionId);

        // Show toast notification instead of modal dialog
        ToastNotification::show(this, "Reproduction Started", 3000);

        // Note: The emulator is now running with the save state loaded
        // When the user closes this dialog, finishReproduction() will be called
        // in the hideEvent() or closeEvent()
    } else {
        qDebug() << "User cancelled reproduction launch";
        reproductionManager_->cancelReproduction(sessionId);
        currentSessionId_.clear();
    }
}

void UIReportList::onSessionFailed(const QString& sessionId, const QString& error)
{
    if (sessionId != currentSessionId_) {
        return;
    }

    qWarning() << "Reproduction session failed:" << sessionId << "-" << error;

    // Close progress dialog
    if (downloadProgressDialog_) {
        downloadProgressDialog_->deleteLater();
        downloadProgressDialog_ = nullptr;
    }

    currentSessionId_.clear();
    downloadInProgress_ = false;

    // Show error message with retry option
    QMessageBox::StandardButton reply = QMessageBox::critical(
        this,
        "Reproduction Failed",
        QString("Failed to prepare bug reproduction:\n%1\n\nDo you want to retry?").arg(error),
        QMessageBox::Retry | QMessageBox::Cancel);

    if (reply == QMessageBox::Retry) {
        // Get the current report and retry
        QModelIndex currentIndex = reportListView->currentIndex();
        if (currentIndex.isValid()) {
            on_reportListView_doubleClicked(currentIndex);
        }
    }
}

void UIReportList::onReproductionProgress(const QString& sessionId, int percentComplete)
{
    if (sessionId != currentSessionId_) {
        return;
    }

    if (!downloadProgressDialog_) {
        return;
    }

    downloadProgressDialog_->setValue(percentComplete);
    qDebug() << "Reproduction progress:" << percentComplete << "%";
}

void UIReportList::onEmulationLaunchRequested(const QString& sessionId,
                                             const GameInfo& gameInfo,
                                             const QString& saveStatePath,
                                             const QString& memoryDumpPath)
{
    qDebug() << "Emulation launch requested for session:" << sessionId;
    qDebug() << "  Game:" << gameInfo.gameTitle << "(" << gameInfo.productNumber << ")";
    qDebug() << "  Save state:" << saveStatePath;
    qDebug() << "  Memory dump:" << memoryDumpPath;

    // Get main window
    UIYabause* mainWindow = qobject_cast<UIYabause*>(QtYabause::mainWindow());
    if (!mainWindow) {
        qCritical() << "Failed to get main window";
        reproductionManager_->finishReproduction(sessionId);
        QMessageBox::critical(this, "Error", "Failed to get main window");
        return;
    }

    // Get FileSearchWidget from main window
    // Assuming FileSearchWidget is accessible through the main window
    // You may need to adjust this based on your actual window hierarchy
    FileSearchWidget* fileSearch = mainWindow->findChild<FileSearchWidget*>();
    if (!fileSearch) {
        qCritical() << "Failed to get FileSearchWidget";
        reproductionManager_->finishReproduction(sessionId);
        QMessageBox::critical(this, "Error",
                            "Failed to get game list widget.\n"
                            "Please make sure your game library is loaded.");
        return;
    }

    // Close this dialog before launching reproduction
    qDebug() << "Closing report list dialog";
    accept();  // Close the dialog

    // Delegate to FileSearchWidget to launch game and load state
    qDebug() << "Delegating to FileSearchWidget";
    bool success = fileSearch->launchGameForReproduction(gameInfo.productNumber,
                                                         saveStatePath,
                                                         memoryDumpPath);

    if (!success) {
        qCritical() << "Failed to launch game for reproduction";
        // Note: Dialog is already closed, error messages shown by FileSearchWidget
    }
}

void UIReportList::onImageLoaded(const QString& url, const QPixmap& pixmap)
{
    Q_UNUSED(url);
    Q_UNUSED(pixmap);

    // Trigger a repaint of the list view to show the newly loaded image
    reportListView->viewport()->update();
}

void UIReportList::loadVisibleImages()
{
    if (!reportListModel_ || !imageCacheManager_) {
        return;
    }

    // Get visible rect of the list view
    QRect visibleRect = reportListView->viewport()->rect();

    // Get the first and last visible items
    QModelIndex topIndex = reportListView->indexAt(visibleRect.topLeft());
    QModelIndex bottomIndex = reportListView->indexAt(visibleRect.bottomLeft());

    if (!topIndex.isValid()) {
        return; // No items visible
    }

    int firstRow = topIndex.row();
    int lastRow = bottomIndex.isValid() ? bottomIndex.row() : reportListModel_->rowCount() - 1;

    qDebug() << "Loading images for visible items:" << firstRow << "to" << lastRow;

    // Load images for visible items
    for (int row = firstRow; row <= lastRow; ++row) {
        QModelIndex index = reportListModel_->index(row, 0);
        QString screenshotUrl = index.data(ReportListModel::ScreenshotUrlRole).toString();

        if (!screenshotUrl.isEmpty() && !imageCacheManager_->isCached(screenshotUrl)) {
            // Request image - will trigger onImageLoaded() when ready
            imageCacheManager_->getImage(screenshotUrl, 100);
        }
    }
}

void UIReportList::onListScrolled()
{
    // Load images for newly visible items
    loadVisibleImages();
}
