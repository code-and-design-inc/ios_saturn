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

#include "ReproductionManagerImpl.h"
#include "../models/AttachmentMetadata.h"
#include "../QtYabause.h"
#include "../YabauseThread.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QDateTime>

// ZIP support using minizip (part of zlib)
extern "C" {
#include <zlib.h>
}

extern "C" {
#include "../../yabause.h"
#include "../../memory.h"
}

ReproductionManagerImpl::ReproductionManagerImpl(StorageService* storageService,
                                                 QObject* parent)
    : ReproductionManager(parent)
    , storageService_(storageService)
    , preferenceManager_(new PreferenceManager(this))
    , activeSessions_()
    , batchToSession_()
{
    if (!storageService_) {
        qCritical() << "ReproductionManagerImpl requires a valid StorageService!";
    }

    // Connect to storage service signals
    connect(storageService_, &StorageService::batchProgress,
            this, &ReproductionManagerImpl::onBatchProgress);
    connect(storageService_, &StorageService::batchComplete,
            this, &ReproductionManagerImpl::onBatchComplete);
    connect(storageService_, &StorageService::batchFailed,
            this, &ReproductionManagerImpl::onBatchFailed);

    qDebug() << "ReproductionManagerImpl initialized";
}

ReproductionManagerImpl::~ReproductionManagerImpl()
{
    // Clean up any active sessions
    for (auto it = activeSessions_.begin(); it != activeSessions_.end(); ++it) {
        ReproductionSession& session = it.value();
        if (session.status == ReproductionSession::Running ||
            session.status == ReproductionSession::Downloading) {
            qWarning() << "Active session" << session.sessionId << "not properly cleaned up";
        }
    }
}

QString ReproductionManagerImpl::prepareReproduction(const ReportData& report,
                                                     const GameInfo& gameInfo)
{
    qDebug() << "Preparing reproduction for report:" << report.documentId;

    // Validate report is reproducible
    if (!report.isReproducible()) {
        qWarning() << "Report is not reproducible - missing required attachments";
        return QString();
    }

    // Generate unique session ID
    QString sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // Create reproduction session
    ReproductionSession session;
    session.sessionId = sessionId;
    session.reportDocumentId = report.documentId;
    session.gameInfo = gameInfo;  // Store game information
    session.status = ReproductionSession::Downloading;
    session.startTime = QDateTime::currentMSecsSinceEpoch();
    session.endTime = 0;

    // Store session
    activeSessions_[sessionId] = session;

    qDebug() << "Created reproduction session:" << sessionId;
    qDebug() << "  Game:" << gameInfo.gameTitle << "(" << gameInfo.productNumber << ")";

    // Prepare attachment list for download
    QList<AttachmentMetadata> attachments;

    // Add screenshot if available
    if (!report.screenshotUrl.isEmpty()) {
        AttachmentMetadata screenshot;
        screenshot.type = AttachmentMetadata::Type::Screenshot;
        screenshot.storageUrl = report.screenshotUrl;
        screenshot.fileName = QString("screenshot_%1.png").arg(report.documentId);
        attachments.append(screenshot);
    }

    // Add save state (required)
    if (!report.saveStateUrl.isEmpty()) {
        AttachmentMetadata saveState;
        saveState.type = AttachmentMetadata::Type::SaveState;
        saveState.storageUrl = report.saveStateUrl;
        saveState.fileName = QString("savestate_%1.yss").arg(report.documentId);
        attachments.append(saveState);
    }

    // Add memory dump (required)
    if (!report.memoryDumpUrl.isEmpty()) {
        AttachmentMetadata memoryDump;
        memoryDump.type = AttachmentMetadata::Type::MemoryDump;
        memoryDump.storageUrl = report.memoryDumpUrl;
        memoryDump.fileName = QString("memory_%1.ram").arg(report.documentId);
        attachments.append(memoryDump);
    }

    if (attachments.isEmpty()) {
        qWarning() << "No attachments to download";
        session.status = ReproductionSession::Failed;
        session.errorMessage = "No attachments found";
        activeSessions_[sessionId] = session;
        emit sessionFailed(sessionId, session.errorMessage);
        return sessionId;
    }

    // Start batch download
    QString batchId = storageService_->downloadBatch(attachments);

    if (batchId.isEmpty()) {
        qCritical() << "Failed to start batch download";
        session.status = ReproductionSession::Failed;
        session.errorMessage = "Failed to start download";
        activeSessions_[sessionId] = session;
        emit sessionFailed(sessionId, session.errorMessage);
        return sessionId;
    }

    // Map batch ID to session ID
    batchToSession_[batchId] = sessionId;

    qDebug() << "Started batch download:" << batchId << "for session:" << sessionId;

    // Emit initial state change
    emit sessionStateChanged(sessionId, ReproductionSession::Downloading);

    return sessionId;
}

void ReproductionManagerImpl::launchReproduction(const QString& sessionId)
{
    qDebug() << "Launching reproduction for session:" << sessionId;

    if (!activeSessions_.contains(sessionId)) {
        qWarning() << "Session not found:" << sessionId;
        return;
    }

    ReproductionSession& session = activeSessions_[sessionId];

    // Verify session is ready
    if (session.status != ReproductionSession::Ready) {
        qWarning() << "Session is not ready for launch. Current status:" << session.status;
        emit sessionFailed(sessionId, "Session is not ready");
        return;
    }

    // Verify required files exist
    if (!QFile::exists(session.saveStatePath)) {
        qCritical() << "Save state file not found:" << session.saveStatePath;
        session.status = ReproductionSession::Failed;
        session.errorMessage = "Save state file not found";
        activeSessions_[sessionId] = session;
        emit sessionFailed(sessionId, session.errorMessage);
        return;
    }

    if (!QFile::exists(session.memoryDumpPath)) {
        qCritical() << "Memory dump file not found:" << session.memoryDumpPath;
        session.status = ReproductionSession::Failed;
        session.errorMessage = "Memory dump file not found";
        activeSessions_[sessionId] = session;
        emit sessionFailed(sessionId, session.errorMessage);
        return;
    }

    qDebug() << "Files verified:";
    qDebug() << "  Save state:" << session.saveStatePath;
    qDebug() << "  Memory dump:" << session.memoryDumpPath;

    // Save current preferences
    qDebug() << "Saving current preferences...";
    session.originalPreferences = preferenceManager_->saveCurrentPreferences(sessionId);
    session.snapshotFilePath = preferenceManager_->getCrashRecoveryFilePath(sessionId);

    // TODO: Apply reproduction preferences from report
    // For now, we skip this as the report doesn't store reproduction preferences yet
    // In the future, this would be: preferenceManager_->applyReproductionPreferences(report.reproductionPreferences);

    // Update session status to Running
    session.status = ReproductionSession::Running;
    activeSessions_[sessionId] = session;

    qDebug() << "Requesting emulation launch for session:" << sessionId;

    // Emit signal to UI layer to handle actual emulation launch
    // The UI layer will:
    // 1. Set backup path via YabauseThread_setBackupPath()
    // 2. Launch the game via handleFileSelected() equivalent
    // 3. Wait a few seconds for emulation to start
    // 4. Load save state via YabLoadState()
    emit requestEmulationLaunch(sessionId,
                               session.gameInfo,
                               session.saveStatePath,
                               session.memoryDumpPath);

    emit sessionStateChanged(sessionId, ReproductionSession::Running);
}

void ReproductionManagerImpl::finishReproduction(const QString& sessionId)
{
    qDebug() << "Finishing reproduction for session:" << sessionId;

    if (sessionId.isEmpty()) {
        qDebug() << "Empty session ID, nothing to finish";
        return;
    }

    if (!activeSessions_.contains(sessionId)) {
        qDebug() << "Session not found:" << sessionId;
        return;
    }

    ReproductionSession& session = activeSessions_[sessionId];

    // Only restore if session was running
    if (session.status != ReproductionSession::Running) {
        qDebug() << "Session was not running, skipping preference restore";
        return;
    }

    // Restore original preferences
    qDebug() << "Restoring original preferences...";
    preferenceManager_->restoreOriginalPreferences(session.originalPreferences);

    // Update session status
    session.status = ReproductionSession::Completed;
    session.endTime = QDateTime::currentMSecsSinceEpoch();
    activeSessions_[sessionId] = session;

    qDebug() << "Reproduction finished successfully for session:" << sessionId;
    qDebug() << "  Duration:" << session.getDurationMs() << "ms";

    emit sessionStateChanged(sessionId, ReproductionSession::Completed);
    emit preferencesRestored(sessionId);

    // Clean up session after a delay
    // (Keep it around for a bit in case UI needs to query it)
}

void ReproductionManagerImpl::cancelReproduction(const QString& sessionId)
{
    qDebug() << "Cancelling reproduction for session:" << sessionId;

    if (!activeSessions_.contains(sessionId)) {
        qWarning() << "Session not found:" << sessionId;
        return;
    }

    ReproductionSession& session = activeSessions_[sessionId];

    // If downloading, try to cancel the download
    if (session.status == ReproductionSession::Downloading) {
        // Find batch ID for this session
        for (auto it = batchToSession_.begin(); it != batchToSession_.end(); ++it) {
            if (it.value() == sessionId) {
                QString batchId = it.key();
                qDebug() << "Cancelling download batch:" << batchId;
                // Note: We don't have a cancelBatch method yet, so downloads will complete
                // but we'll mark the session as cancelled
                batchToSession_.remove(batchId);
                break;
            }
        }
    }

    // Clean up downloaded files
    if (!session.screenshotPath.isEmpty() && QFile::exists(session.screenshotPath)) {
        QFile::remove(session.screenshotPath);
    }
    if (!session.saveStatePath.isEmpty() && QFile::exists(session.saveStatePath)) {
        QFile::remove(session.saveStatePath);
    }
    if (!session.memoryDumpPath.isEmpty() && QFile::exists(session.memoryDumpPath)) {
        QFile::remove(session.memoryDumpPath);
    }

    // Delete crash recovery file if it exists
    if (!session.snapshotFilePath.isEmpty() && QFile::exists(session.snapshotFilePath)) {
        QFile::remove(session.snapshotFilePath);
    }

    // Remove session
    activeSessions_.remove(sessionId);

    qDebug() << "Reproduction cancelled for session:" << sessionId;

    emit sessionCancelled(sessionId);
}

void ReproductionManagerImpl::performCrashRecovery()
{
    qDebug() << "Checking for crash recovery...";

    if (!preferenceManager_->hasCrashRecoverySnapshot()) {
        qDebug() << "No crash recovery needed";
        return;
    }

    qDebug() << "Performing crash recovery...";

    QMap<QString, QVariant> restoredPrefs = preferenceManager_->performCrashRecovery();

    if (!restoredPrefs.isEmpty()) {
        qDebug() << "Crash recovery completed. Restored" << restoredPrefs.size() << "preferences";
        emit crashRecoveryPerformed(restoredPrefs);
    } else {
        qWarning() << "Crash recovery found snapshot but couldn't restore preferences";
    }
}

ReproductionSession ReproductionManagerImpl::getSession(const QString& sessionId) const
{
    if (activeSessions_.contains(sessionId)) {
        return activeSessions_[sessionId];
    }

    // Return empty session if not found
    ReproductionSession emptySession;
    emptySession.sessionId = sessionId;
    emptySession.status = ReproductionSession::Failed;
    emptySession.errorMessage = "Session not found";
    return emptySession;
}

void ReproductionManagerImpl::onBatchProgress(const QString& batchId, int completedFiles, int totalFiles,
                                             qint64 totalBytesTransferred, qint64 totalBytes)
{
    if (!batchToSession_.contains(batchId)) {
        return;
    }

    QString sessionId = batchToSession_[batchId];
    int percentComplete = (totalBytes > 0) ? (int)((totalBytesTransferred * 100) / totalBytes) : 0;

    qDebug() << "Session" << sessionId << "download progress:" << percentComplete << "%"
             << "(" << completedFiles << "/" << totalFiles << "files)";

    emit downloadProgress(sessionId, percentComplete);
}

void ReproductionManagerImpl::onBatchComplete(const QString& batchId, const QStringList& filePaths)
{
    qDebug() << "Batch download complete:" << batchId << "-" << filePaths.size() << "files";

    if (!batchToSession_.contains(batchId)) {
        qWarning() << "Batch ID not mapped to any session:" << batchId;
        return;
    }

    QString sessionId = batchToSession_[batchId];

    if (!activeSessions_.contains(sessionId)) {
        qWarning() << "Session not found for batch:" << sessionId;
        batchToSession_.remove(batchId);
        return;
    }

    ReproductionSession& session = activeSessions_[sessionId];

    // Process downloaded files (unzip if necessary)
    QStringList processedFiles;
    for (const QString& filePath : filePaths) {
        QFileInfo fileInfo(filePath);

        // Check if file is a ZIP archive
        if (fileInfo.suffix().toLower() == "yss" || fileInfo.suffix().toLower() == "ram") {
            qDebug() << "Extracting ZIP file:" << filePath;
            QStringList extractedFiles = extractZipFile(filePath, fileInfo.absolutePath());
            if (extractedFiles.isEmpty()) {
                qCritical() << "Failed to extract ZIP file:" << filePath;
                // Continue with original file as fallback
                processedFiles.append(filePath);
            } else {
                processedFiles.append(extractedFiles);
                qDebug() << "Extracted" << extractedFiles.size() << "files from ZIP";
            }
        } else {
            processedFiles.append(filePath);
        }
    }

    // Map processed files to session paths
    for (const QString& filePath : processedFiles) {
        QFileInfo fileInfo(filePath);
        QString fileName = fileInfo.fileName().toLower();

        if (fileName.startsWith("screenshot")) {
            session.screenshotPath = filePath;
            qDebug() << "  Screenshot:" << filePath;
        } else if (fileInfo.suffix().toLower()== "yss") {
            session.saveStatePath = filePath;
            qDebug() << "  Save state:" << filePath;
        } else if (fileInfo.suffix().toLower() == "ram") {
            session.memoryDumpPath = filePath;
            qDebug() << "  Memory dump:" << filePath;
        }
    }

    // Verify required files were downloaded
    if (session.saveStatePath.isEmpty() || session.memoryDumpPath.isEmpty()) {
        qCritical() << "Missing required files after download";
        session.status = ReproductionSession::Failed;
        session.errorMessage = "Missing required files (save state or memory dump)";
        activeSessions_[sessionId] = session;
        batchToSession_.remove(batchId);
        emit sessionFailed(sessionId, session.errorMessage);
        return;
    }

    // Update session status
    session.status = ReproductionSession::Ready;
    activeSessions_[sessionId] = session;

    // Remove batch mapping
    batchToSession_.remove(batchId);

    qDebug() << "Session ready:" << sessionId;

    emit sessionStateChanged(sessionId, ReproductionSession::Ready);
    emit sessionReady(sessionId, filePaths);
}

void ReproductionManagerImpl::onBatchFailed(const QString& batchId, const QString& error)
{
    qWarning() << "Batch download failed:" << batchId << "-" << error;

    if (!batchToSession_.contains(batchId)) {
        qWarning() << "Batch ID not mapped to any session:" << batchId;
        return;
    }

    QString sessionId = batchToSession_[batchId];

    if (!activeSessions_.contains(sessionId)) {
        qWarning() << "Session not found for batch:" << sessionId;
        batchToSession_.remove(batchId);
        return;
    }

    ReproductionSession& session = activeSessions_[sessionId];
    session.status = ReproductionSession::Failed;
    session.errorMessage = QString("Download failed: %1").arg(error);
    activeSessions_[sessionId] = session;

    batchToSession_.remove(batchId);

    emit sessionStateChanged(sessionId, ReproductionSession::Failed);
    emit sessionFailed(sessionId, session.errorMessage);
}

bool ReproductionManagerImpl::loadSaveState(const QString& filePath)
{
    qDebug() << "Loading save state from:" << filePath;

    // Use YabLoadState from memory.h
    int result = YabLoadState(filePath.toLocal8Bit().constData());

    if (result != 0) {
        qCritical() << "YabLoadState failed with code:" << result;
        return false;
    }

    qDebug() << "Save state loaded successfully";
    return true;
}

bool ReproductionManagerImpl::loadMemoryDump(const QString& filePath)
{
    qDebug() << "Loading memory dump from:" << filePath;

    // Verify file exists
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        qCritical() << "Memory dump file does not exist:" << filePath;
        return false;
    }

    if (fileInfo.size() == 0) {
        qCritical() << "Memory dump file is empty";
        return false;
    }

    qDebug() << "Setting backup path to:" << filePath << "(" << fileInfo.size() << "bytes)";

    // Use YabauseThread_setBackupPath to load the memory dump
    // This function sets the backup RAM file path that the emulator will use
    extern void YabauseThread_setBackupPath(const char* buf);
    YabauseThread_setBackupPath(filePath.toLocal8Bit().constData());

    qDebug() << "Memory dump path set successfully";
    return true;
}

QString ReproductionManagerImpl::getTempDirectory() const
{
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                      + "/yabasanshiro_reproduction";

    QDir dir;
    if (!dir.exists(tempDir)) {
        dir.mkpath(tempDir);
    }

    return tempDir;
}

QStringList ReproductionManagerImpl::extractZipFile(const QString& zipFilePath, const QString& targetDir)
{
    QStringList extractedFiles;

    // Ensure target directory exists
    QDir dir;
    if (!dir.mkpath(targetDir)) {
        qCritical() << "Failed to create target directory:" << targetDir;
        return extractedFiles;
    }

    qDebug() << "Extracting ZIP file:" << zipFilePath << "to" << targetDir;

    // Open the ZIP file
    QFile zipFile(zipFilePath);
    if (!zipFile.open(QIODevice::ReadOnly)) {
        qCritical() << "Failed to open ZIP file:" << zipFile.errorString();
        return extractedFiles;
    }

    QByteArray zipData = zipFile.readAll();
    zipFile.close();

    if (zipData.isEmpty()) {
        qCritical() << "ZIP file is empty";
        return extractedFiles;
    }

    const char* data = zipData.constData();
    qint64 dataSize = zipData.size();

    // Find End of Central Directory Record (EOCD)
    // Search from end of file for signature 0x06054b50
    qint64 eocdOffset = -1;
    for (qint64 i = dataSize - 22; i >= 0 && i >= dataSize - 65557; i--) {
        quint32 sig = *reinterpret_cast<const quint32*>(data + i);
        if (sig == 0x06054b50) {
            eocdOffset = i;
            break;
        }
    }

    if (eocdOffset == -1) {
        qCritical() << "Could not find End of Central Directory Record";
        return extractedFiles;
    }

    // Parse EOCD
    quint16 totalEntries = *reinterpret_cast<const quint16*>(data + eocdOffset + 10);
    // quint32 centralDirSize = *reinterpret_cast<const quint32*>(data + eocdOffset + 12);
    quint32 centralDirOffset = *reinterpret_cast<const quint32*>(data + eocdOffset + 16);

    qDebug() << "Found" << totalEntries << "entries in central directory at offset" << centralDirOffset;

    // Parse Central Directory
    qint64 cdOffset = centralDirOffset;
    for (int i = 0; i < totalEntries; i++) {
        if (cdOffset + 46 > dataSize) break;

        quint32 cdSig = *reinterpret_cast<const quint32*>(data + cdOffset);
        if (cdSig != 0x02014b50) {  // Central directory file header signature
            qWarning() << "Invalid central directory signature at" << cdOffset;
            break;
        }

        // Parse central directory entry
        quint16 compressionMethod = *reinterpret_cast<const quint16*>(data + cdOffset + 10);
        quint32 compressedSize = *reinterpret_cast<const quint32*>(data + cdOffset + 20);
        quint32 uncompressedSize = *reinterpret_cast<const quint32*>(data + cdOffset + 24);
        quint16 fileNameLength = *reinterpret_cast<const quint16*>(data + cdOffset + 28);
        quint16 extraFieldLength = *reinterpret_cast<const quint16*>(data + cdOffset + 30);
        quint16 fileCommentLength = *reinterpret_cast<const quint16*>(data + cdOffset + 32);
        quint32 localHeaderOffset = *reinterpret_cast<const quint32*>(data + cdOffset + 42);

        if (cdOffset + 46 + fileNameLength + extraFieldLength + fileCommentLength > dataSize) break;

        // Read filename
        QByteArray fileNameBytes(data + cdOffset + 46, fileNameLength);
        QString fileName = QString::fromUtf8(fileNameBytes);

        qDebug() << "Entry" << i << ":" << fileName
                 << "compressed:" << compressedSize
                 << "uncompressed:" << uncompressedSize
                 << "method:" << compressionMethod;

        // Move to next central directory entry
        cdOffset += 46 + fileNameLength + extraFieldLength + fileCommentLength;

        // Security check: prevent path traversal
        if (fileName.contains("..") || fileName.startsWith("/")) {
            qWarning() << "Skipping entry with suspicious path:" << fileName;
            continue;
        }

        // Skip directories
        if (fileName.endsWith('/')) {
            continue;
        }

        // Now read the actual file data from local header
        if (localHeaderOffset + 30 > dataSize) {
            qCritical() << "Invalid local header offset for" << fileName;
            continue;
        }

        quint32 localSig = *reinterpret_cast<const quint32*>(data + localHeaderOffset);
        if (localSig != 0x04034b50) {
            qCritical() << "Invalid local header signature for" << fileName;
            continue;
        }

        quint16 localFileNameLength = *reinterpret_cast<const quint16*>(data + localHeaderOffset + 26);
        quint16 localExtraFieldLength = *reinterpret_cast<const quint16*>(data + localHeaderOffset + 28);

        qint64 fileDataOffset = localHeaderOffset + 30 + localFileNameLength + localExtraFieldLength;

        if (fileDataOffset + compressedSize > dataSize) {
            qCritical() << "File data exceeds ZIP file size for" << fileName;
            continue;
        }

        const char* compressedData = data + fileDataOffset;
        QByteArray uncompressedData;

        if (compressionMethod == 0) {
            // Stored (no compression)
            uncompressedData = QByteArray(compressedData, compressedSize);
        } else if (compressionMethod == 8) {
            // DEFLATE compression
            uncompressedData.resize(uncompressedSize);

            z_stream stream;
            memset(&stream, 0, sizeof(stream));
            stream.zalloc = Z_NULL;
            stream.zfree = Z_NULL;
            stream.opaque = Z_NULL;
            stream.avail_in = compressedSize;
            stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressedData));
            stream.avail_out = uncompressedSize;
            stream.next_out = reinterpret_cast<Bytef*>(uncompressedData.data());

            // Initialize with -15 to use raw deflate (no zlib header)
            int ret = inflateInit2(&stream, -15);
            if (ret != Z_OK) {
                qCritical() << "inflateInit2 failed for" << fileName << ":" << ret;
                continue;
            }

            ret = inflate(&stream, Z_FINISH);
            inflateEnd(&stream);

            if (ret != Z_STREAM_END) {
                qCritical() << "inflate failed for" << fileName << ":" << ret
                           << "total_out:" << stream.total_out << "expected:" << uncompressedSize;
                continue;
            }
        } else {
            qWarning() << "Unsupported compression method:" << compressionMethod;
            continue;
        }

        // Write extracted file
        QString outputPath = QDir(targetDir).filePath(fileName);
        QFileInfo outputFileInfo(outputPath);

        // Create parent directories
        QDir().mkpath(outputFileInfo.absolutePath());

        QFile outputFile(outputPath);
        if (!outputFile.open(QIODevice::WriteOnly)) {
            qCritical() << "Failed to create output file:" << outputPath << outputFile.errorString();
            continue;
        }

        qint64 written = outputFile.write(uncompressedData);
        outputFile.close();

        if (written == uncompressedData.size()) {
            extractedFiles.append(outputPath);
            qDebug() << "  Extracted:" << fileName << "(" << written << "bytes)";
        } else {
            qCritical() << "Failed to write complete file:" << outputPath;
        }
    }

    qDebug() << "Extraction complete. Extracted" << extractedFiles.size() << "files";
    return extractedFiles;
}
