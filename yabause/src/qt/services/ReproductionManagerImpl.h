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

#ifndef REPRODUCTION_MANAGER_IMPL_H
#define REPRODUCTION_MANAGER_IMPL_H

#include "ReproductionManager.h"
#include "StorageService.h"
#include "PreferenceManager.h"
#include <QMap>
#include <QUuid>

// Forward declaration
class YabauseThread;

/**
 * @class ReproductionManagerImpl
 * @brief Concrete implementation of ReproductionManager
 *
 * Orchestrates the complete bug reproduction workflow:
 * 1. Download attachments via StorageService
 * 2. Save current preferences via PreferenceManager
 * 3. Apply reproduction preferences
 * 4. Launch emulator with save state and memory dump
 * 5. Restore preferences on return
 *
 * Dependencies:
 * - StorageService: For downloading report attachments
 * - PreferenceManager: For preference snapshot/restore
 * - YabauseThread: For emulator control
 */
class ReproductionManagerImpl : public ReproductionManager {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param storageService Storage service for file downloads (must not be null)
     * @param parent Qt parent object
     */
    explicit ReproductionManagerImpl(StorageService* storageService,
                                     QObject* parent = nullptr);
    virtual ~ReproductionManagerImpl();

    // ReproductionManager interface implementation
    QString prepareReproduction(const ReportData& report,
                               const GameInfo& gameInfo) override;

    void launchReproduction(const QString& sessionId) override;

    void finishReproduction(const QString& sessionId) override;

    void cancelReproduction(const QString& sessionId) override;

    void performCrashRecovery() override;

    ReproductionSession getSession(const QString& sessionId) const override;

private slots:
    /**
     * @brief Handle batch download progress
     * @param batchId Batch download ID
     * @param completedFiles Number of completed files
     * @param totalFiles Total files in batch
     * @param totalBytesTransferred Total bytes transferred
     * @param totalBytes Total size of all files
     */
    void onBatchProgress(const QString& batchId, int completedFiles, int totalFiles,
                        qint64 totalBytesTransferred, qint64 totalBytes);

    /**
     * @brief Handle batch download completion
     * @param batchId Batch download ID
     * @param filePaths Paths to downloaded files
     */
    void onBatchComplete(const QString& batchId, const QStringList& filePaths);

    /**
     * @brief Handle batch download failure
     * @param batchId Batch download ID
     * @param error Error message
     */
    void onBatchFailed(const QString& batchId, const QString& error);

private:
    /**
     * @brief Load save state file into emulator
     * @param filePath Path to save state file
     * @return true if load succeeded
     */
    bool loadSaveState(const QString& filePath);

    /**
     * @brief Load memory dump file into emulator
     * @param filePath Path to memory dump file
     * @return true if load succeeded
     */
    bool loadMemoryDump(const QString& filePath);

    /**
     * @brief Get temporary directory for reproduction files
     * @return Path to temp directory
     */
    QString getTempDirectory() const;

    /**
     * @brief Extract ZIP file to target directory
     * @param zipFilePath Path to ZIP file
     * @param targetDir Target directory for extraction
     * @return List of extracted file paths, empty if extraction failed
     */
    QStringList extractZipFile(const QString& zipFilePath, const QString& targetDir);

private:
    StorageService* storageService_;            ///< Storage service for downloads
    PreferenceManager* preferenceManager_;      ///< Preference manager for snapshots
    QMap<QString, ReproductionSession> activeSessions_;  ///< Active reproduction sessions
    QMap<QString, QString> batchToSession_;     ///< Map batch ID to session ID
};

#endif // REPRODUCTION_MANAGER_IMPL_H
