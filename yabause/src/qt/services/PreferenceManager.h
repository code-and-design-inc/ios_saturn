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

#ifndef PREFERENCE_MANAGER_H
#define PREFERENCE_MANAGER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QVariant>

/**
 * @class PreferenceManager
 * @brief Manages saving and restoring emulator preferences for bug reproduction
 *
 * Responsibilities:
 * - Create snapshot of current emulator preferences
 * - Apply reproduction-specific preferences from bug reports
 * - Restore original preferences after reproduction
 * - Handle crash recovery by persisting snapshots to disk
 *
 * Thread Safety: All methods must be called from Qt main thread
 */
class PreferenceManager : public QObject {
    Q_OBJECT

public:
    explicit PreferenceManager(QObject* parent = nullptr);
    virtual ~PreferenceManager();

    /**
     * @brief Save current preferences to memory and crash recovery file
     * @param sessionId Unique session identifier for this reproduction
     * @return Map of saved preference key-value pairs
     *
     * Creates a snapshot of all relevant emulator settings and writes them
     * to a temporary crash recovery file. If the application crashes during
     * reproduction, this file can be used to restore preferences on next startup.
     */
    QMap<QString, QVariant> saveCurrentPreferences(const QString& sessionId);

    /**
     * @brief Apply reproduction preferences from a bug report
     * @param preferences Map of preference key-value pairs from report
     *
     * Applies the exact settings that were active when the bug was reported.
     * This ensures the emulator runs in the same configuration as when the
     * bug occurred.
     */
    void applyReproductionPreferences(const QMap<QString, QVariant>& preferences);

    /**
     * @brief Restore original preferences from snapshot
     * @param snapshot Preference snapshot returned by saveCurrentPreferences()
     *
     * Restores all settings to their state before reproduction began.
     * Also deletes the crash recovery file if it exists.
     */
    void restoreOriginalPreferences(const QMap<QString, QVariant>& snapshot);

    /**
     * @brief Check if crash recovery file exists
     * @return true if a crash recovery snapshot file is found
     */
    bool hasCrashRecoverySnapshot() const;

    /**
     * @brief Load and restore preferences from crash recovery file
     * @return Map of restored preferences (empty if no recovery file found)
     *
     * Called on application startup. If a crash recovery file exists,
     * loads and applies the preferences, then deletes the recovery file.
     */
    QMap<QString, QVariant> performCrashRecovery();

    /**
     * @brief Delete crash recovery file
     *
     * Removes the temporary snapshot file. Called after successful
     * preference restoration.
     */
    void deleteCrashRecoveryFile();

    /**
     * @brief Get path to crash recovery snapshot file
     * @param sessionId Session identifier (empty for default recovery file)
     * @return Absolute path to recovery file
     */
    QString getCrashRecoveryFilePath(const QString& sessionId = QString()) const;

private:

    /**
     * @brief Get list of preference keys to snapshot
     * @return List of Settings keys that affect emulation behavior
     */
    QStringList getRelevantPreferenceKeys() const;

    /**
     * @brief Write preference snapshot to JSON file
     * @param filePath Path to write snapshot file
     * @param snapshot Preference map to save
     * @return true if write succeeded
     */
    bool writeSnapshotToFile(const QString& filePath, const QMap<QString, QVariant>& snapshot);

    /**
     * @brief Read preference snapshot from JSON file
     * @param filePath Path to snapshot file
     * @return Preference map (empty if read failed)
     */
    QMap<QString, QVariant> readSnapshotFromFile(const QString& filePath);
};

#endif // PREFERENCE_MANAGER_H
