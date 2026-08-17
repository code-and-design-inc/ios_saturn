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

#include "PreferenceManager.h"
#include "../QtYabause.h"
#include "../Settings.h"
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

PreferenceManager::PreferenceManager(QObject* parent)
    : QObject(parent)
{
    qDebug() << "PreferenceManager initialized";
}

PreferenceManager::~PreferenceManager()
{
}

QMap<QString, QVariant> PreferenceManager::saveCurrentPreferences(const QString& sessionId)
{
    qDebug() << "Saving current preferences for session:" << sessionId;

    QMap<QString, QVariant> snapshot;
    Settings* settings = QtYabause::settings();

    // Get all relevant preference keys
    QStringList keys = getRelevantPreferenceKeys();

    // Save current values
    for (const QString& key : keys) {
        QVariant value = settings->value(key);
        snapshot[key] = value;
        qDebug() << "  Saved:" << key << "=" << value;
    }

    // Write snapshot to crash recovery file
    QString recoveryFilePath = getCrashRecoveryFilePath(sessionId);
    if (writeSnapshotToFile(recoveryFilePath, snapshot)) {
        qDebug() << "Crash recovery snapshot written to:" << recoveryFilePath;
    } else {
        qWarning() << "Failed to write crash recovery snapshot to:" << recoveryFilePath;
    }

    return snapshot;
}

void PreferenceManager::applyReproductionPreferences(const QMap<QString, QVariant>& preferences)
{
    qDebug() << "Applying reproduction preferences:" << preferences.size() << "settings";

    Settings* settings = QtYabause::settings();

    // Apply each preference
    for (auto it = preferences.begin(); it != preferences.end(); ++it) {
        const QString& key = it.key();
        const QVariant& value = it.value();

        qDebug() << "  Applying:" << key << "=" << value;
        settings->setValue(key, value);
    }

    // Sync to ensure settings are written
    settings->sync();

    qDebug() << "Reproduction preferences applied successfully";
}

void PreferenceManager::restoreOriginalPreferences(const QMap<QString, QVariant>& snapshot)
{
    qDebug() << "Restoring original preferences:" << snapshot.size() << "settings";

    Settings* settings = QtYabause::settings();

    // Restore each preference
    for (auto it = snapshot.begin(); it != snapshot.end(); ++it) {
        const QString& key = it.key();
        const QVariant& value = it.value();

        qDebug() << "  Restoring:" << key << "=" << value;
        settings->setValue(key, value);
    }

    // Sync to ensure settings are written
    settings->sync();

    // Delete crash recovery file
    deleteCrashRecoveryFile();

    qDebug() << "Original preferences restored successfully";
}

bool PreferenceManager::hasCrashRecoverySnapshot() const
{
    QString filePath = getCrashRecoveryFilePath();
    bool exists = QFile::exists(filePath);
    qDebug() << "Crash recovery file exists:" << exists << "at" << filePath;
    return exists;
}

QMap<QString, QVariant> PreferenceManager::performCrashRecovery()
{
    QString filePath = getCrashRecoveryFilePath();

    if (!QFile::exists(filePath)) {
        qDebug() << "No crash recovery file found";
        return QMap<QString, QVariant>();
    }

    qDebug() << "Performing crash recovery from:" << filePath;

    // Read snapshot from file
    QMap<QString, QVariant> snapshot = readSnapshotFromFile(filePath);

    if (snapshot.isEmpty()) {
        qWarning() << "Crash recovery file is empty or corrupt";
        deleteCrashRecoveryFile();
        return QMap<QString, QVariant>();
    }

    // Restore preferences
    Settings* settings = QtYabause::settings();
    for (auto it = snapshot.begin(); it != snapshot.end(); ++it) {
        qDebug() << "  Recovering:" << it.key() << "=" << it.value();
        settings->setValue(it.key(), it.value());
    }
    settings->sync();

    // Delete recovery file after successful restore
    deleteCrashRecoveryFile();

    qDebug() << "Crash recovery completed successfully";

    return snapshot;
}

void PreferenceManager::deleteCrashRecoveryFile()
{
    QString filePath = getCrashRecoveryFilePath();

    if (QFile::exists(filePath)) {
        if (QFile::remove(filePath)) {
            qDebug() << "Crash recovery file deleted:" << filePath;
        } else {
            qWarning() << "Failed to delete crash recovery file:" << filePath;
        }
    }
}

QString PreferenceManager::getCrashRecoveryFilePath(const QString& sessionId) const
{
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString fileName = sessionId.isEmpty() ? "yabasanshiro_repro_snapshot.json"
                                           : QString("yabasanshiro_repro_%1.json").arg(sessionId);
    return tempDir + "/" + fileName;
}

QStringList PreferenceManager::getRelevantPreferenceKeys() const
{
    // List of all settings that affect emulation behavior
    // Based on Android version's REPRODUCTION_PREFERENCE_KEYS
    return QStringList{
        // Video settings
        "Video/VideoCore",
        "Video/Filter",
        "Video/PolygonGeneration",
        "Video/Resolution",
        "Video/AspectRatio",
        "Video/RBGResolution",
        "Video/UseComputeShader",
        "Video/FrameSkip",

        // Sound settings
        "Sound/SoundCore",
        "Sound/SoundEngine",
        "Sound/ScspTimeSyncMode",
        "Sound/ScspSyncPerFrame",

        // Input settings
        "Input/Port/1",
        "Input/Port/2",
        "Input/P1Device",
        "Input/P2Device",

        // General settings
        "General/Bios",
        "General/CdRom",
        "General/Cart",
        "General/Area",
        "General/ExtendInternalMemory",

        // CPU settings
        "Advanced/CPU",
        "Advanced/CPUAffinity",
        "Advanced/SH2Cache",

        // Display settings
        "View/Menubar",
        "View/Toolbar",
        "View/Fullscreen"
    };
}

bool PreferenceManager::writeSnapshotToFile(const QString& filePath, const QMap<QString, QVariant>& snapshot)
{
    // Convert QMap to QJsonObject
    QJsonObject jsonObj;
    for (auto it = snapshot.begin(); it != snapshot.end(); ++it) {
        // Store as string to preserve all types
        jsonObj[it.key()] = it.value().toString();
    }

    // Write to file
    QJsonDocument doc(jsonObj);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open file for writing:" << filePath << "-" << file.errorString();
        return false;
    }

    qint64 bytesWritten = file.write(doc.toJson());
    file.close();

    if (bytesWritten == -1) {
        qWarning() << "Failed to write snapshot to file:" << filePath;
        return false;
    }

    return true;
}

QMap<QString, QVariant> PreferenceManager::readSnapshotFromFile(const QString& filePath)
{
    QMap<QString, QVariant> snapshot;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file for reading:" << filePath << "-" << file.errorString();
        return snapshot;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "Invalid JSON in snapshot file:" << filePath;
        return snapshot;
    }

    // Convert QJsonObject to QMap
    QJsonObject jsonObj = doc.object();
    for (auto it = jsonObj.begin(); it != jsonObj.end(); ++it) {
        snapshot[it.key()] = it.value().toString();
    }

    return snapshot;
}
