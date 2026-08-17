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

#ifndef FIRESTORE_SERVICE_IMPL_H
#define FIRESTORE_SERVICE_IMPL_H

#include "FirestoreService.h"
#include <QTimer>
#include <memory>
#include <firebase/app.h>
#include <firebase/future.h>
#include <firebase/firestore.h>
#include <firebase/firestore/document_snapshot.h>
#include <firebase/firestore/query_snapshot.h>

/**
 * @class FirestoreServiceImpl
 * @brief Implementation of FirestoreService using Firebase C++ SDK
 *
 * Responsibilities:
 * - Initialize Firebase Firestore client
 * - Execute async queries and convert Future<T> to Qt signals
 * - Parse Firestore documents into ReportData objects
 * - Handle query cancellation and error reporting
 *
 * Threading: All Firebase operations run on Firebase's internal threads.
 * Results are emitted as Qt signals on the main thread via QTimer polling.
 */
class FirestoreServiceImpl : public FirestoreService {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param app Firebase App instance (must be initialized)
     * @param parent Qt parent object
     */
    explicit FirestoreServiceImpl(firebase::App* app, QObject* parent = nullptr);
    virtual ~FirestoreServiceImpl();

    // FirestoreService interface implementation
    void loadReportsForGame(const QString& productNumber) override;
    void refresh() override;
    void cancel() override;

private:
    /**
     * @brief Parse Firestore document into ReportData
     * @param doc Firestore document snapshot
     * @return Parsed ReportData object
     */
    ReportData parseReportFromDocument(const firebase::firestore::DocumentSnapshot& doc);

    /**
     * @brief Execute query for game document by product_number
     * @param productNumber Game product number
     */
    void executeGameQuery(const QString& productNumber);

    /**
     * @brief Execute query for ratings subcollection
     * @param gameDocPath Path to game document
     */
    void executeRatingsQuery(const QString& gameDocPath);

private:
    firebase::firestore::Firestore* firestore_;  ///< Firebase Firestore client
    QString lastProductNumber_;                   ///< Last queried product number (for refresh)
    QTimer* pollTimer_;                           ///< Timer for polling Firebase futures
    bool queryInProgress_;                        ///< Flag to prevent concurrent queries
};

#endif // FIRESTORE_SERVICE_IMPL_H
