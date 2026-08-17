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

#ifndef TOAST_NOTIFICATION_H
#define TOAST_NOTIFICATION_H

#include <QWidget>
#include <QLabel>
#include <QTimer>

/**
 * @class ToastNotification
 * @brief Android Toast-like notification widget
 *
 * Features:
 * - Non-modal notification that appears briefly
 * - Auto-dismiss after configurable duration
 * - Positioned at bottom center of parent widget
 * - Semi-transparent background
 * - Fade-in/fade-out animation (optional)
 */
class ToastNotification : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Show a toast notification
     * @param parent Parent widget (notification will be centered on this)
     * @param message Text to display
     * @param durationMs Display duration in milliseconds (default: 3000ms)
     */
    static void show(QWidget* parent, const QString& message, int durationMs = 3000);

private:
    explicit ToastNotification(QWidget* parent, const QString& message, int durationMs);
    virtual ~ToastNotification() = default;

    void showNotification();
    void hideNotification();

private slots:
    void onTimeout();

private:
    QLabel* label_;
    QTimer* timer_;
    int duration_;
};

#endif // TOAST_NOTIFICATION_H
