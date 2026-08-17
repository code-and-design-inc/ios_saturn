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

#ifndef STORAGE_SERVICE_IMPL_H
#define STORAGE_SERVICE_IMPL_H

#include "StorageService.h"
#include <QTimer>
#include <QMap>
#include <QUuid>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <memory>
#include <firebase/app.h>

/**
 * @class StorageServiceImpl
 * @brief Implementation of StorageService using Firebase C++ SDK
 *
 * Responsibilities:
 * - Initialize Firebase Storage client
 * - Download files with progress tracking using Controller
 * - Poll download futures with QTimer
 * - Handle concurrent batch downloads
 * - Clean up temporary files
 *
 * Threading: All Firebase operations run on Firebase's internal threads.
 * Results are emitted as Qt signals on the main thread via QTimer polling.
 */
class StorageServiceImpl : public StorageService {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param app Firebase App instance (must be initialized)
     * @param parent Qt parent object
     */
    explicit StorageServiceImpl(firebase::App* app, QObject* parent = nullptr);
    virtual ~StorageServiceImpl();

    // StorageService interface implementation
    QString downloadFile(const QString& storageUrl,
                        const QString& localPath,
                        AttachmentMetadata::Type type) override;

    QString downloadBatch(const QList<AttachmentMetadata>& attachments) override;

    void cancelDownload(const QString& downloadId) override;

    void cleanupTempFiles() override;

private:
    /**
     * @brief Structure tracking a single download operation
     */
    struct DownloadInfo {
        QString downloadId;
        QString storageUrl;
        QString localPath;
        AttachmentMetadata::Type type;
        QNetworkReply* reply;
        QFile* file;
        int retryCount;
        qint64 totalBytes;
        qint64 receivedBytes;
    };

    /**
     * @brief Structure tracking a batch download operation
     */
    struct BatchInfo {
        QString batchId;
        QStringList downloadIds;
        QStringList completedPaths;
        QList<AttachmentMetadata> pendingAttachments;  // Queue for sequential downloads
        QString tempDirectory;
        int totalFiles;
        int completedFiles;
        qint64 totalBytes;
        qint64 transferredBytes;
        bool hasFailed;
        QString errorMessage;
    };

    /**
     * @brief Execute download for a single file using HTTPS
     * @param info Download information structure
     */
    void executeDownload(DownloadInfo* info);

    /**
     * @brief Handle download completion
     * @param info Download information structure
     */
    void onDownloadComplete(DownloadInfo* info);

    /**
     * @brief Handle download failure
     * @param info Download information structure
     * @param error Error message
     */
    void onDownloadFailed(DownloadInfo* info, const QString& error);

    /**
     * @brief Retry a failed download with exponential backoff
     * @param info Download information structure
     */
    void retryDownload(DownloadInfo* info);

    /**
     * @brief Check if a batch is complete
     * @param batchId Batch ID to check
     */
    void checkBatchCompletion(const QString& batchId);

    /**
     * @brief Start next download in batch queue
     * @param batchId Batch ID
     */
    void startNextBatchDownload(const QString& batchId);

    /**
     * @brief Get local temp directory for downloads
     * @return Path to temp directory
     */
    QString getTempDirectory() const;

    /**
     * @brief Get cache key from URL
     * @param url Storage URL
     * @return MD5 hash of URL
     */
    QString getCacheKey(const QString& url) const;

    /**
     * @brief Get cache file path for URL
     * @param url Storage URL
     * @return Local cache file path
     */
    QString getCacheFilePath(const QString& url) const;

    /**
     * @brief Check if file is in cache
     * @param url Storage URL
     * @return True if cached
     */
    bool isCached(const QString& url) const;

    /**
     * @brief Copy file from cache to destination
     * @param url Storage URL
     * @param destPath Destination file path
     * @return True if successful
     */
    bool copyFromCache(const QString& url, const QString& destPath);

    /**
     * @brief Save downloaded file to cache
     * @param url Storage URL
     * @param sourcePath Downloaded file path
     */
    void saveToCache(const QString& url, const QString& sourcePath);

private:
    QNetworkAccessManager* networkManager_;         ///< Network manager for HTTPS downloads
    QMap<QString, DownloadInfo*> activeDownloads_;  ///< Active download operations
    QMap<QString, BatchInfo*> activeBatches_;       ///< Active batch operations
    QMap<QString, QString> downloadToBatch_;        ///< Map download ID to batch ID
    QMap<QNetworkReply*, QString> replyToDownloadId_; ///< Map reply to download ID
    QString cacheDirectory_;                        ///< Cache directory for downloaded files
};

#endif // STORAGE_SERVICE_IMPL_H
