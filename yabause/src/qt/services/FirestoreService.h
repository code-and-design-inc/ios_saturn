/**
 * @file FirestoreService.h
 * @brief Contract for Firestore database operations
 *
 * This interface defines the contract for querying and retrieving bug reports
 * from Firebase Firestore. Implementations must handle Firebase async operations
 * and translate them to Qt signals.
 */

#ifndef FIRESTORE_SERVICE_H
#define FIRESTORE_SERVICE_H

#include <QObject>
#include <QString>
#include <QList>
#include "../models/ReportData.h"

/**
 * @class FirestoreService
 * @brief Service for querying bug reports from Firestore
 *
 * Responsibilities:
 * - Query game document by product number
 * - Retrieve ratings subcollection for a game
 * - Filter visible reports only
 * - Order reports by timestamp (descending)
 * - Handle Firebase authentication errors
 * - Translate Firestore documents to ReportData objects
 *
 * Thread Safety: All methods must be callable from Qt main thread.
 * Operations execute asynchronously and emit signals on completion.
 */
class FirestoreService : public QObject {
    Q_OBJECT

public:
    explicit FirestoreService(QObject* parent = nullptr);
    virtual ~FirestoreService() = default;

    /**
     * @brief Load all visible reports for a specific game
     * @param productNumber Game product number (e.g., "T-1234G")
     *
     * Executes asynchronous Firestore query:
     * 1. Find game document where product_number == productNumber
     * 2. Query ratings subcollection where isVisible == true
     * 3. Order by timestamp descending
     *
     * Emits: reportsLoaded() on success
     * Emits: loadFailed() on error
     */
    virtual void loadReportsForGame(const QString& productNumber) = 0;

    /**
     * @brief Refresh current report list
     *
     * Re-executes the last query to get updated data from Firestore.
     * Useful for pull-to-refresh functionality.
     *
     * Emits: reportsLoaded() on success
     * Emits: loadFailed() on error
     */
    virtual void refresh() = 0;

    /**
     * @brief Cancel any in-progress query
     *
     * Cancels the current Firebase query if one is running.
     * No signals are emitted if query is successfully cancelled.
     */
    virtual void cancel() = 0;

signals:
    /**
     * @brief Emitted when reports are successfully loaded
     * @param reports List of report data objects
     */
    void reportsLoaded(const QList<ReportData>& reports);

    /**
     * @brief Emitted when report loading fails
     * @param error Human-readable error message
     * @param errorCode Firebase error code (0 = success, non-zero = error)
     */
    void loadFailed(const QString& error, int errorCode);

    /**
     * @brief Emitted during query execution for progress indication
     * @param stage Description of current stage (e.g., "Finding game...", "Loading reports...")
     */
    void loadingStageChanged(const QString& stage);
};

#endif // FIRESTORE_SERVICE_H
