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

#include "StorageServiceImpl.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QCryptographicHash>
#include <QTimer>

StorageServiceImpl::StorageServiceImpl(firebase::App* app, QObject* parent)
    : StorageService(parent)
    , networkManager_(new QNetworkAccessManager(this))
    , activeDownloads_()
    , activeBatches_()
    , downloadToBatch_()
    , replyToDownloadId_()
{
    Q_UNUSED(app);  // Not needed for HTTPS downloads

    // Setup cache directory
    cacheDirectory_ = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                      + "/attachment_cache";

    QDir dir;
    if (!dir.exists(cacheDirectory_)) {
        dir.mkpath(cacheDirectory_);
        qDebug() << "Created attachment cache directory:" << cacheDirectory_;
    }

    qDebug() << "StorageServiceImpl initialized with QNetworkAccessManager";
    qDebug() << "Cache directory:" << cacheDirectory_;
}

StorageServiceImpl::~StorageServiceImpl()
{
    // Cancel all active downloads
    for (auto it = activeDownloads_.begin(); it != activeDownloads_.end(); ++it) {
        DownloadInfo* info = it.value();
        if (info->reply) {
            info->reply->abort();
            info->reply->deleteLater();
        }
        if (info->file) {
            info->file->close();
            delete info->file;
        }
        delete info;
    }
    activeDownloads_.clear();

    // Clean up batch info
    for (auto it = activeBatches_.begin(); it != activeBatches_.end(); ++it) {
        delete it.value();
    }
    activeBatches_.clear();
}

QString StorageServiceImpl::downloadFile(const QString& storageUrl,
                                         const QString& localPath,
                                         AttachmentMetadata::Type type)
{
    // Generate unique download ID
    QString downloadId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    qDebug() << "Creating download request:";
    qDebug() << "  ID:" << downloadId;
    qDebug() << "  URL:" << storageUrl;
    qDebug() << "  Local path:" << localPath;

    // Check cache first
    if (isCached(storageUrl)) {
        qDebug() << "File found in cache, copying from cache";
        if (copyFromCache(storageUrl, localPath)) {
            qDebug() << "Successfully copied from cache to:" << localPath;

            // Create temporary download info for batch tracking
            DownloadInfo* info = new DownloadInfo();
            info->downloadId = downloadId;
            info->storageUrl = storageUrl;
            info->localPath = localPath;
            info->type = type;
            info->reply = nullptr;
            info->file = nullptr;
            info->retryCount = 0;
            info->totalBytes = 0;
            info->receivedBytes = 0;

            // Store in active downloads temporarily
            activeDownloads_[downloadId] = info;

            // Emit completion signal asynchronously
            // This ensures downloadToBatch_ is populated before signal is processed
            QTimer::singleShot(100, this, [this, info]() {
                onDownloadComplete(info);
            });

            return downloadId;
        } else {
            qWarning() << "Failed to copy from cache, will download from network";
        }
    }

    // Create download info
    DownloadInfo* info = new DownloadInfo();
    info->downloadId = downloadId;
    info->storageUrl = storageUrl;
    info->localPath = localPath;
    info->type = type;
    info->reply = nullptr;
    info->file = nullptr;
    info->retryCount = 0;
    info->totalBytes = 0;
    info->receivedBytes = 0;

    // Store in active downloads
    activeDownloads_[downloadId] = info;

    // Start download
    executeDownload(info);

    return downloadId;
}

QString StorageServiceImpl::downloadBatch(const QList<AttachmentMetadata>& attachments)
{
    if (attachments.isEmpty()) {
        qWarning() << "downloadBatch called with empty attachments list";
        return QString();
    }

    qDebug() << "Starting batch download with" << attachments.size() << "files (SEQUENTIAL MODE)";

    // Generate unique batch ID
    QString batchId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // Create batch info
    BatchInfo* batch = new BatchInfo();
    batch->batchId = batchId;
    batch->pendingAttachments = attachments;  // Queue all attachments
    batch->tempDirectory = getTempDirectory();
    batch->totalFiles = attachments.size();
    batch->completedFiles = 0;
    batch->totalBytes = 0;
    batch->transferredBytes = 0;
    batch->hasFailed = false;

    activeBatches_[batchId] = batch;

    // Start first download only (sequential)
    startNextBatchDownload(batchId);

    return batchId;
}

void StorageServiceImpl::startNextBatchDownload(const QString& batchId)
{
    qDebug() << "startNextBatchDownload called for batch:" << batchId;
    qDebug() << "this =" << (void*)this;
    qDebug() << "downloadToBatch_ size =" << downloadToBatch_.size();

    if (!activeBatches_.contains(batchId)) {
        qWarning() << "Batch not found:" << batchId;
        return;
    }

    BatchInfo* batch = activeBatches_[batchId];

    if (batch->hasFailed) {
        qDebug() << "Batch already failed, not starting next download";
        return;
    }

    if (batch->pendingAttachments.isEmpty()) {
        qDebug() << "No more pending attachments in batch";
        checkBatchCompletion(batchId);
        return;
    }

    // Get next attachment from queue
    AttachmentMetadata attachment = batch->pendingAttachments.takeFirst();

    qDebug() << "Starting download" << (batch->totalFiles - batch->pendingAttachments.size())
             << "of" << batch->totalFiles << ":" << attachment.fileName;

    QString localPath = batch->tempDirectory + "/" + attachment.fileName;
    QString downloadId = downloadFile(attachment.storageUrl, localPath, attachment.type);

    if (!downloadId.isEmpty()) {
        qDebug() << "Download started, adding to tracking maps";
        qDebug() << "downloadId:" << downloadId << "-> batchId:" << batchId;

        batch->downloadIds.append(downloadId);

        qDebug() << "About to insert into downloadToBatch_, current size:" << downloadToBatch_.size();
        downloadToBatch_[downloadId] = batchId;
        qDebug() << "Successfully inserted, new size:" << downloadToBatch_.size();
    } else {
        qWarning() << "Failed to start download for" << attachment.fileName;
        batch->hasFailed = true;
        batch->errorMessage = QString("Failed to start download: %1").arg(attachment.fileName);
        emit batchFailed(batchId, batch->errorMessage);
    }
}

void StorageServiceImpl::executeDownload(DownloadInfo* info)
{
    qDebug() << "=== Starting HTTPS download ===";
    qDebug() << "Download ID:" << info->downloadId;
    qDebug() << "Storage URL:" << info->storageUrl;
    qDebug() << "Local Path:" << info->localPath;

    // Create file for writing
    info->file = new QFile(info->localPath);
    if (!info->file->open(QIODevice::WriteOnly)) {
        QString error = QString("Cannot open file for writing: %1").arg(info->file->errorString());
        qCritical() << error;
        delete info->file;
        info->file = nullptr;
        onDownloadFailed(info, error);
        return;
    }

    // Create network request
    QNetworkRequest request(QUrl(info->storageUrl));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    // Start GET request
    info->reply = networkManager_->get(request);
    replyToDownloadId_[info->reply] = info->downloadId;

    // Connect progress signal
    connect(info->reply, &QNetworkReply::downloadProgress, this,
        [this, info](qint64 received, qint64 total) {
            info->receivedBytes = received;
            info->totalBytes = total;

            // Emit individual file progress
            emit downloadProgress(info->downloadId, received, total);

            // Update batch progress if part of batch
            if (downloadToBatch_.contains(info->downloadId)) {
                QString batchId = downloadToBatch_[info->downloadId];
                if (activeBatches_.contains(batchId)) {
                    BatchInfo* batch = activeBatches_[batchId];

                    // Calculate aggregate progress for all downloads in batch
                    qint64 totalTransferred = 0;
                    qint64 totalSize = 0;
                    for (const QString& downloadId : batch->downloadIds) {
                        if (activeDownloads_.contains(downloadId)) {
                            DownloadInfo* d = activeDownloads_[downloadId];
                            totalTransferred += d->receivedBytes;
                            totalSize += d->totalBytes;
                        }
                    }

                    batch->transferredBytes = totalTransferred;
                    batch->totalBytes = totalSize;

                    emit batchProgress(batchId, batch->completedFiles, batch->totalFiles,
                                      totalTransferred, totalSize);
                }
            }
        });

    // Connect readyRead signal to write data incrementally
    connect(info->reply, &QNetworkReply::readyRead, this,
        [this, info]() {
            if (info->file) {
                info->file->write(info->reply->readAll());
            }
        });

    // Connect finished signal
    connect(info->reply, &QNetworkReply::finished, this,
        [this, info]() {
            // Write any remaining data
            if (info->file && info->reply) {
                info->file->write(info->reply->readAll());
                info->file->close();
            }

            // Check for errors
            if (info->reply->error() == QNetworkReply::NoError) {
                qDebug() << "Download finished successfully:" << info->downloadId;
                onDownloadComplete(info);
            } else {
                QString error = info->reply->errorString();
                qWarning() << "Download failed:" << info->downloadId << "-" << error;
                onDownloadFailed(info, error);
            }
        });

    qDebug() << "HTTPS download started successfully:" << info->downloadId;
}

void StorageServiceImpl::onDownloadComplete(DownloadInfo* info)
{
    qDebug() << "Download complete:" << info->downloadId << "->" << info->localPath;

    // Save to cache for future use (only if not already cached)
    if (!isCached(info->storageUrl)) {
        saveToCache(info->storageUrl, info->localPath);
    }

    // Emit completion signal
    emit downloadComplete(info->downloadId, info->localPath);

    QString batchId;
    bool isBatchDownload = false;

    // Update batch if part of one
    if (downloadToBatch_.contains(info->downloadId)) {
        batchId = downloadToBatch_[info->downloadId];
        isBatchDownload = true;

        if (activeBatches_.contains(batchId)) {
            BatchInfo* batch = activeBatches_[batchId];
            batch->completedFiles++;
            batch->completedPaths.append(info->localPath);

            qDebug() << "Batch progress:" << batch->completedFiles << "/" << batch->totalFiles;
        }
    }

    // Clean up download info
    QString downloadId = info->downloadId;
    if (info->reply) {
        replyToDownloadId_.remove(info->reply);
        info->reply->deleteLater();
    }
    if (info->file) {
        delete info->file;
    }
    delete info;
    activeDownloads_.remove(downloadId);

    // Start next download in batch (SEQUENTIAL)
    if (isBatchDownload) {
        startNextBatchDownload(batchId);
    }
}

void StorageServiceImpl::onDownloadFailed(DownloadInfo* info, const QString& error)
{
    qWarning() << "Download failed:" << info->downloadId << "-" << error;

    // Check if should retry
    if (info->retryCount < 3) {
        qDebug() << "Will retry download (attempt" << (info->retryCount + 1) << "of 3)";
        retryDownload(info);
    } else {
        qCritical() << "Download failed after 3 retries:" << info->downloadId;

        // Emit failure signal
        emit downloadFailed(info->downloadId, error);

        // Update batch if part of one - STOP all remaining downloads
        if (downloadToBatch_.contains(info->downloadId)) {
            QString batchId = downloadToBatch_[info->downloadId];
            if (activeBatches_.contains(batchId)) {
                BatchInfo* batch = activeBatches_[batchId];
                batch->hasFailed = true;
                batch->errorMessage = QString("File %1 failed: %2")
                                        .arg(QFileInfo(info->localPath).fileName())
                                        .arg(error);

                // Clear pending downloads
                batch->pendingAttachments.clear();

                emit batchFailed(batchId, batch->errorMessage);
            }
        }

        // Clean up download info
        QString downloadId = info->downloadId;
        if (info->reply) {
            replyToDownloadId_.remove(info->reply);
            info->reply->deleteLater();
        }
        if (info->file) {
            info->file->close();
            delete info->file;
            // Delete partial download file
            QFile::remove(info->localPath);
        }
        delete info;
        activeDownloads_.remove(downloadId);
    }
}

void StorageServiceImpl::retryDownload(DownloadInfo* info)
{
    info->retryCount++;

    // Exponential backoff: 1s, 2s, 4s
    int delayMs = (1 << (info->retryCount - 1)) * 1000;

    qDebug() << "Retrying download" << info->downloadId
             << "attempt" << info->retryCount << "after" << delayMs << "ms";

    // Clean up old reply and file
    if (info->reply) {
        replyToDownloadId_.remove(info->reply);
        info->reply->deleteLater();
        info->reply = nullptr;
    }
    if (info->file) {
        info->file->close();
        delete info->file;
        info->file = nullptr;
    }

    // Delete partial file
    QFile::remove(info->localPath);

    // Reset progress counters
    info->receivedBytes = 0;
    info->totalBytes = 0;

    // Retry after delay
    QTimer::singleShot(delayMs, this, [this, info]() {
        executeDownload(info);
    });
}

void StorageServiceImpl::checkBatchCompletion(const QString& batchId)
{
    if (!activeBatches_.contains(batchId)) {
        return;
    }

    BatchInfo* batch = activeBatches_[batchId];

    if (batch->hasFailed) {
        // Already failed, do nothing
        return;
    }

    if (batch->completedFiles == batch->totalFiles) {
        // All downloads complete
        qDebug() << "Batch complete:" << batchId << "- all" << batch->totalFiles << "files downloaded";

        emit batchComplete(batchId, batch->completedPaths);

        // Clean up batch
        for (const QString& downloadId : batch->downloadIds) {
            downloadToBatch_.remove(downloadId);
        }
        delete batch;
        activeBatches_.remove(batchId);
    }
}

void StorageServiceImpl::cancelDownload(const QString& downloadId)
{
    if (!activeDownloads_.contains(downloadId)) {
        qWarning() << "Cannot cancel download: ID not found -" << downloadId;
        return;
    }

    DownloadInfo* info = activeDownloads_[downloadId];

    // Abort network request
    if (info->reply) {
        info->reply->abort();
        replyToDownloadId_.remove(info->reply);
        info->reply->deleteLater();
    }

    // Close and delete file
    if (info->file) {
        info->file->close();
        delete info->file;
    }

    // Delete partial file
    QFile::remove(info->localPath);

    qDebug() << "Cancelled download:" << downloadId;

    // Clean up
    delete info;
    activeDownloads_.remove(downloadId);
}

void StorageServiceImpl::cleanupTempFiles()
{
    QString tempDir = getTempDirectory();
    QDir dir(tempDir);

    if (!dir.exists()) {
        return;
    }

    // Remove all files in temp directory
    QStringList files = dir.entryList(QDir::Files);
    for (const QString& filename : files) {
        QString filePath = tempDir + "/" + filename;
        if (QFile::remove(filePath)) {
            qDebug() << "Removed temp file:" << filePath;
        } else {
            qWarning() << "Failed to remove temp file:" << filePath;
        }
    }
}

QString StorageServiceImpl::getTempDirectory() const
{
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                      + "/yabasanshiro_reports";

    QDir dir;
    if (!dir.exists(tempDir)) {
        dir.mkpath(tempDir);
    }

    return tempDir;
}

QString StorageServiceImpl::getCacheKey(const QString& url) const
{
    // Use MD5 hash of URL as cache key
    QByteArray hash = QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Md5);
    return QString(hash.toHex());
}

QString StorageServiceImpl::getCacheFilePath(const QString& url) const
{
    QString cacheKey = getCacheKey(url);

    // Extract file extension from URL if possible
    QString extension;
    QUrl qurl(url);
    QString path = qurl.path();
    int lastDot = path.lastIndexOf('.');
    if (lastDot != -1) {
        extension = path.mid(lastDot);  // includes the dot
    }

    return cacheDirectory_ + "/" + cacheKey + extension;
}

bool StorageServiceImpl::isCached(const QString& url) const
{
    QString cacheFile = getCacheFilePath(url);
    return QFile::exists(cacheFile);
}

bool StorageServiceImpl::copyFromCache(const QString& url, const QString& destPath)
{
    QString cacheFile = getCacheFilePath(url);

    if (!QFile::exists(cacheFile)) {
        return false;
    }

    // Remove destination file if it exists
    if (QFile::exists(destPath)) {
        QFile::remove(destPath);
    }

    // Copy from cache to destination
    bool success = QFile::copy(cacheFile, destPath);

    if (success) {
        qDebug() << "Copied from cache:" << cacheFile << "->" << destPath;
    } else {
        qWarning() << "Failed to copy from cache:" << cacheFile << "->" << destPath;
    }

    return success;
}

void StorageServiceImpl::saveToCache(const QString& url, const QString& sourcePath)
{
    QString cacheFile = getCacheFilePath(url);

    // Check if already cached
    if (QFile::exists(cacheFile)) {
        qDebug() << "File already in cache:" << cacheFile;
        return;
    }

    // Copy to cache
    bool success = QFile::copy(sourcePath, cacheFile);

    if (success) {
        qDebug() << "Saved to cache:" << sourcePath << "->" << cacheFile;
    } else {
        qWarning() << "Failed to save to cache:" << sourcePath << "->" << cacheFile;
    }
}
