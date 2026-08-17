/**
 * @file StorageService.h
 * @brief Contract for Firebase Storage file download operations
 *
 * This interface defines the contract for downloading report attachments
 * from Firebase Storage with progress tracking.
 */

#ifndef STORAGE_SERVICE_H
#define STORAGE_SERVICE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include "../models/AttachmentMetadata.h"

/**
 * @class StorageService
 * @brief Service for downloading files from Firebase Storage
 *
 * Responsibilities:
 * - Download files from Firebase Storage URLs
 * - Track download progress
 * - Save files to local temporary directory
 * - Validate downloaded file integrity
 * - Handle network errors and retries
 * - Clean up temporary files on completion/failure
 *
 * Thread Safety: All methods must be callable from Qt main thread.
 * Operations execute asynchronously and emit signals for progress/completion.
 */
class StorageService : public QObject {
    Q_OBJECT

public:
    explicit StorageService(QObject* parent = nullptr);
    virtual ~StorageService() = default;

    /**
     * @brief Download a single file from Firebase Storage
     * @param storageUrl Firebase Storage download URL
     * @param localPath Destination path in local filesystem
     * @param type Type of attachment (for validation)
     * @return Download ID for tracking this operation
     *
     * Downloads file asynchronously. Progress is reported via downloadProgress() signal.
     *
     * Emits: downloadProgress() periodically during download
     * Emits: downloadComplete() on success
     * Emits: downloadFailed() on error
     */
    virtual QString downloadFile(const QString& storageUrl,
                                  const QString& localPath,
                                  AttachmentMetadata::Type type) = 0;

    /**
     * @brief Download multiple files concurrently
     * @param attachments List of attachments to download
     * @return Batch ID for tracking this group of downloads
     *
     * Downloads all files in parallel. Progress is aggregated across all downloads.
     *
     * Emits: batchProgress() periodically
     * Emits: batchComplete() when all downloads succeed
     * Emits: batchFailed() if any download fails
     */
    virtual QString downloadBatch(const QList<AttachmentMetadata>& attachments) = 0;

    /**
     * @brief Cancel an in-progress download
     * @param downloadId ID returned from downloadFile()
     *
     * Stops the download and cleans up partial files.
     * No signals are emitted for cancelled downloads.
     */
    virtual void cancelDownload(const QString& downloadId) = 0;

    /**
     * @brief Clean up all temporary downloaded files
     *
     * Deletes all files in the temporary download directory.
     * Should be called when reproduction session ends.
     */
    virtual void cleanupTempFiles() = 0;

signals:
    /**
     * @brief Emitted periodically during file download
     * @param downloadId Download operation ID
     * @param bytesTransferred Number of bytes downloaded so far
     * @param totalBytes Total file size in bytes
     */
    void downloadProgress(const QString& downloadId,
                          qint64 bytesTransferred,
                          qint64 totalBytes);

    /**
     * @brief Emitted when single file download completes successfully
     * @param downloadId Download operation ID
     * @param localPath Path where file was saved
     */
    void downloadComplete(const QString& downloadId, const QString& localPath);

    /**
     * @brief Emitted when single file download fails
     * @param downloadId Download operation ID
     * @param error Human-readable error message
     */
    void downloadFailed(const QString& downloadId, const QString& error);

    /**
     * @brief Emitted periodically during batch download
     * @param batchId Batch operation ID
     * @param completedFiles Number of files downloaded successfully
     * @param totalFiles Total number of files in batch
     * @param totalBytesTransferred Total bytes downloaded across all files
     * @param totalBytes Total size of all files
     */
    void batchProgress(const QString& batchId,
                       int completedFiles,
                       int totalFiles,
                       qint64 totalBytesTransferred,
                       qint64 totalBytes);

    /**
     * @brief Emitted when batch download completes successfully
     * @param batchId Batch operation ID
     * @param filePaths List of local paths where files were saved
     */
    void batchComplete(const QString& batchId, const QStringList& filePaths);

    /**
     * @brief Emitted when batch download fails
     * @param batchId Batch operation ID
     * @param error Human-readable error message describing which file failed
     */
    void batchFailed(const QString& batchId, const QString& error);
};

#endif // STORAGE_SERVICE_H
