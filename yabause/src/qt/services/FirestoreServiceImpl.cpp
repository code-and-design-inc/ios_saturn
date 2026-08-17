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

#include "FirestoreServiceImpl.h"
#include "../models/ReportData.h"
#include <firebase/app.h>
#include <firebase/firestore.h>
#include <firebase/firestore/document_snapshot.h>
#include <firebase/firestore/query_snapshot.h>
#include <QDateTime>
#include <QDebug>

using namespace firebase;
using namespace firebase::firestore;

FirestoreServiceImpl::FirestoreServiceImpl(firebase::App* app, QObject* parent)
    : FirestoreService(parent)
    , firestore_(nullptr)
    , pollTimer_(new QTimer(this))
    , queryInProgress_(false)
{
    // Initialize Firestore client
    firestore_ = Firestore::GetInstance(app);

    // Configure poll timer
    pollTimer_->setInterval(100); // Poll every 100ms
}

FirestoreServiceImpl::~FirestoreServiceImpl()
{
    // Firebase cleanup is handled by Firebase SDK
    if (pollTimer_->isActive()) {
        pollTimer_->stop();
    }
}

void FirestoreServiceImpl::loadReportsForGame(const QString& productNumber)
{
    if (queryInProgress_) {
        qWarning() << "Query already in progress, ignoring loadReportsForGame()";
        return;
    }

    if (productNumber.isEmpty()) {
        emit loadFailed(QString("Product number cannot be empty"), -1);
        return;
    }

    lastProductNumber_ = productNumber;
    queryInProgress_ = true;

    emit loadingStageChanged("Finding game...");

    // Step 1: Find game document by product_number
    executeGameQuery(productNumber);
}

void FirestoreServiceImpl::executeGameQuery(const QString& productNumber)
{
    // Query: SELECT * FROM games WHERE product_number == productNumber LIMIT 1
    Query query = firestore_->Collection("games")
                      .WhereEqualTo("product_number", FieldValue::String(productNumber.toStdString()));

    // Execute query asynchronously
    firebase::Future<QuerySnapshot> future = query.Get();

    // Poll future status with QTimer
    QObject* context = new QObject(this);
    connect(pollTimer_, &QTimer::timeout, context, [this, future, productNumber, context]() mutable {
        FutureStatus status = future.status();

        if (status == kFutureStatusComplete) {
            pollTimer_->stop();
            context->deleteLater();

            if (future.error() == 0) {
                const QuerySnapshot* snapshot = future.result();
                if (snapshot && !snapshot->documents().empty()) {
                    // Get first game document
                    DocumentSnapshot gameDoc = snapshot->documents()[0];
                    QString gameDocPath = QString::fromStdString(gameDoc.reference().path());

                    emit loadingStageChanged("Loading reports...");

                    // Step 2: Query ratings subcollection
                    executeRatingsQuery(gameDocPath);
                } else {
                    queryInProgress_ = false;
                    emit loadFailed(QString("Game not found: %1").arg(productNumber), -1);
                }
            } else {
                queryInProgress_ = false;
                QString errorMsg = QString::fromStdString(future.error_message());
                emit loadFailed(QString("Failed to find game: %1").arg(errorMsg), future.error());
            }
        } else if (status == kFutureStatusInvalid) {
            pollTimer_->stop();
            context->deleteLater();
            queryInProgress_ = false;
            emit loadFailed(QString("Query future became invalid"), -1);
        }
    });

    pollTimer_->start();
}

void FirestoreServiceImpl::executeRatingsQuery(const QString& gameDocPath)
{
    // Query: SELECT * FROM {gameDocPath}/ratings WHERE isVisible == true ORDER BY timestamp DESC
    DocumentReference gameRef = firestore_->Document(gameDocPath.toStdString());
    Query query = gameRef.Collection("ratings")
                      .WhereEqualTo("isVisible", FieldValue::Boolean(true))
                      .OrderBy("timestamp", Query::Direction::kDescending);

    // Execute query asynchronously
    firebase::Future<QuerySnapshot> future = query.Get();

    // Poll future status with QTimer
    QObject* context = new QObject(this);
    connect(pollTimer_, &QTimer::timeout, context, [this, future, context]() mutable {
        FutureStatus status = future.status();

        if (status == kFutureStatusComplete) {
            pollTimer_->stop();
            context->deleteLater();

            if (future.error() == 0) {
                const QuerySnapshot* snapshot = future.result();
                if (snapshot) {
                    QList<ReportData> reports;

                    // Parse each document into ReportData
                    for (const DocumentSnapshot& doc : snapshot->documents()) {
                        try {
                            ReportData report = parseReportFromDocument(doc);
                            reports.append(report);
                        } catch (const std::exception& e) {
                            qWarning() << "Failed to parse report document:" << doc.id().c_str() << "-" << e.what();
                            // Continue processing other documents
                        }
                    }

                    queryInProgress_ = false;
                    emit reportsLoaded(reports);
                } else {
                    queryInProgress_ = false;
                    emit loadFailed(QString("Query returned null result"), -1);
                }
            } else {
                queryInProgress_ = false;
                QString errorMsg = QString::fromStdString(future.error_message());
                emit loadFailed(QString("Failed to load reports: %1").arg(errorMsg), future.error());
            }
        } else if (status == kFutureStatusInvalid) {
            pollTimer_->stop();
            context->deleteLater();
            queryInProgress_ = false;
            emit loadFailed(QString("Query future became invalid"), -1);
        }
    });

    pollTimer_->start();
}

ReportData FirestoreServiceImpl::parseReportFromDocument(const DocumentSnapshot& doc)
{
    ReportData report;

    // Core identification
    report.documentId = QString::fromStdString(doc.id());

    // Get all data from document
    MapFieldValue data = doc.GetData();

    // Debug: Log all field names
    qDebug() << "Parsing document:" << report.documentId;
    qDebug() << "Available fields:";
    for (const auto& pair : data) {
        QString fieldName = QString::fromStdString(pair.first);
        qDebug() << "  -" << fieldName;
    }

    // Extract fields from Firestore document
    auto userIdIt = data.find("userId");
    if (userIdIt != data.end() && userIdIt->second.is_string()) {
        report.userId = QString::fromStdString(userIdIt->second.string_value());
    }

    auto gameCodeIt = data.find("gameCode");
    if (gameCodeIt != data.end() && gameCodeIt->second.is_string()) {
        report.gameCode = QString::fromStdString(gameCodeIt->second.string_value());
    }

    // Report content
    auto timestampIt = data.find("timestamp");
    if (timestampIt != data.end()) {
        if (timestampIt->second.is_timestamp()) {
            // Firebase Timestamp type - convert to milliseconds since epoch
            firebase::Timestamp ts = timestampIt->second.timestamp_value();
            report.timestamp = ts.seconds() * 1000LL; // Convert seconds to milliseconds
        } else if (timestampIt->second.is_integer()) {
            // Already stored as integer (milliseconds)
            report.timestamp = timestampIt->second.integer_value();
        }
    }

    auto ratingIt = data.find("rating");
    if (ratingIt != data.end() && ratingIt->second.is_integer()) {
        report.rating = static_cast<int>(ratingIt->second.integer_value());
    }

    auto commentIt = data.find("comment");
    if (commentIt != data.end() && commentIt->second.is_string()) {
        report.description = QString::fromStdString(commentIt->second.string_value());
    }

    auto isVisibleIt = data.find("isVisible");
    if (isVisibleIt != data.end() && isVisibleIt->second.is_boolean()) {
        report.isVisible = isVisibleIt->second.boolean_value();
    }

    // Attachment URLs (use snake_case field names from Android app)
    auto screenshotUrlIt = data.find("screenshot_url");
    if (screenshotUrlIt != data.end() && screenshotUrlIt->second.is_string()) {
        report.screenshotUrl = QString::fromStdString(screenshotUrlIt->second.string_value());
    }

    auto saveStateUrlIt = data.find("savestate_url");
    if (saveStateUrlIt != data.end() && saveStateUrlIt->second.is_string()) {
        report.saveStateUrl = QString::fromStdString(saveStateUrlIt->second.string_value());
    }

    auto memoryDumpUrlIt = data.find("memory_url");
    if (memoryDumpUrlIt != data.end() && memoryDumpUrlIt->second.is_string()) {
        report.memoryDumpUrl = QString::fromStdString(memoryDumpUrlIt->second.string_value());
    }

    // Reproduction preferences (map)
    auto prefsIt = data.find("reproductionPreferences");
    if (prefsIt != data.end() && prefsIt->second.is_map()) {
        MapFieldValue prefsMap = prefsIt->second.map_value();
        for (const auto& pair : prefsMap) {
            QString key = QString::fromStdString(pair.first);
            QString value;
            if (pair.second.is_string()) {
                value = QString::fromStdString(pair.second.string_value());
            }
            report.reproductionPreferences[key] = value;
        }
    }

    return report;
}

void FirestoreServiceImpl::refresh()
{
    if (lastProductNumber_.isEmpty()) {
        qWarning() << "Cannot refresh: No previous query executed";
        return;
    }

    // Re-execute last query
    loadReportsForGame(lastProductNumber_);
}

void FirestoreServiceImpl::cancel()
{
    if (queryInProgress_) {
        // Stop polling timer
        if (pollTimer_->isActive()) {
            pollTimer_->stop();
        }

        queryInProgress_ = false;
        qDebug() << "Query cancelled";
    }
}
