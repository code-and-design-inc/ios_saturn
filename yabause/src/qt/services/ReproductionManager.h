/**
 * @file ReproductionManager.h
 * @brief Contract for bug reproduction workflow orchestration
 *
 * This interface defines the contract for managing the complete bug reproduction
 * workflow including file downloads, preference management, and emulator launching.
 */

#ifndef REPRODUCTION_MANAGER_H
#define REPRODUCTION_MANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QVariant>
#include "../models/ReportData.h"
#include "../models/ReproductionSession.h"
#include "../models/GameInfo.h"

/**
 * @class ReproductionManager
 * @brief High-level orchestrator for bug reproduction workflow
 *
 * Responsibilities:
 * - Coordinate attachment downloads via StorageService
 * - Manage reproduction session lifecycle
 * - Save/restore emulator preferences
 * - Launch emulator with saved state
 * - Handle crash recovery (restore prefs if app crashes)
 * - Clean up temporary files after reproduction
 *
 * Thread Safety: All methods must be callable from Qt main thread.
 * Operations execute asynchronously and emit signals for state changes.
 */
class ReproductionManager : public QObject {
    Q_OBJECT

public:
    explicit ReproductionManager(QObject* parent = nullptr);
    virtual ~ReproductionManager() = default;

    /**
     * @brief Initialize reproduction session for a report
     * @param report Report data containing attachment URLs
     * @param gameInfo Game information (title, file path, etc.)
     * @return Session ID for tracking this reproduction
     *
     * Creates a new ReproductionSession and begins downloading required files.
     * Does NOT launch emulator - use launchReproduction() after preparation completes.
     *
     * Emits: sessionStateChanged() with status=Downloading
     * Emits: downloadProgress() periodically
     * Emits: sessionReady() when files are downloaded
     * Emits: sessionFailed() if download fails
     */
    virtual QString prepareReproduction(const ReportData& report,
                                        const GameInfo& gameInfo) = 0;

    /**
     * @brief Launch emulator with reproduction session
     * @param sessionId Session ID from prepareReproduction()
     *
     * Prerequisites: Session must be in Ready state (files downloaded)
     *
     * Workflow:
     * 1. Save current preferences to snapshot file
     * 2. Apply reproduction preferences from report
     * 3. Launch YabauseThread with save state and memory dump
     *
     * Emits: sessionStateChanged() with status=Running
     * Emits: sessionFailed() if launch fails
     */
    virtual void launchReproduction(const QString& sessionId) = 0;

    /**
     * @brief Restore preferences after returning from emulator
     * @param sessionId Session ID
     *
     * Called automatically when UIReportList regains focus (onResume).
     * Restores original preferences and marks session as Completed.
     *
     * Emits: sessionStateChanged() with status=Completed
     * Emits: preferencesRestored()
     */
    virtual void finishReproduction(const QString& sessionId) = 0;

    /**
     * @brief Cancel reproduction session
     * @param sessionId Session ID
     *
     * Cancels downloads, cleans up files, and discards session.
     * If emulator is running, does NOT forcibly close it.
     *
     * Emits: sessionCancelled()
     */
    virtual void cancelReproduction(const QString& sessionId) = 0;

    /**
     * @brief Check for orphaned preference snapshots from crashes
     *
     * Called on application startup. If snapshot file exists, restores
     * preferences and deletes snapshot file.
     *
     * Emits: crashRecoveryPerformed() if snapshot was found and restored
     */
    virtual void performCrashRecovery() = 0;

    /**
     * @brief Get current session state
     * @param sessionId Session ID
     * @return Current reproduction session object
     */
    virtual ReproductionSession getSession(const QString& sessionId) const = 0;

signals:
    /**
     * @brief Emitted when session state changes
     * @param sessionId Session ID
     * @param status New session status
     */
    void sessionStateChanged(const QString& sessionId,
                             ReproductionSession::Status status);

    /**
     * @brief Emitted when session is ready to launch
     * @param sessionId Session ID
     * @param downloadedFiles List of downloaded file paths
     */
    void sessionReady(const QString& sessionId, const QStringList& downloadedFiles);

    /**
     * @brief Emitted when session fails
     * @param sessionId Session ID
     * @param error Human-readable error message
     */
    void sessionFailed(const QString& sessionId, const QString& error);

    /**
     * @brief Emitted when session is cancelled
     * @param sessionId Session ID
     */
    void sessionCancelled(const QString& sessionId);

    /**
     * @brief Emitted periodically during file downloads
     * @param sessionId Session ID
     * @param percentComplete Overall progress (0-100)
     */
    void downloadProgress(const QString& sessionId, int percentComplete);

    /**
     * @brief Emitted after preferences are successfully restored
     * @param sessionId Session ID
     */
    void preferencesRestored(const QString& sessionId);

    /**
     * @brief Emitted when crash recovery is performed
     * @param restoredPreferences Map of restored preference keys/values
     */
    void crashRecoveryPerformed(const QMap<QString, QVariant>& restoredPreferences);

    /**
     * @brief Emitted when reproduction needs to launch emulation
     * @param sessionId Session ID
     * @param gameInfo Game information (product number, title, file path)
     * @param saveStatePath Path to save state file to load
     * @param memoryDumpPath Path to memory dump file to load
     */
    void requestEmulationLaunch(const QString& sessionId,
                                const GameInfo& gameInfo,
                                const QString& saveStatePath,
                                const QString& memoryDumpPath);
};

#endif // REPRODUCTION_MANAGER_H
