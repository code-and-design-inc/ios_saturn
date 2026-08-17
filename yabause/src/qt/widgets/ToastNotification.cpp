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

#include "ToastNotification.h"
#include <QVBoxLayout>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

void ToastNotification::show(QWidget* parent, const QString& message, int durationMs)
{
    // Create toast notification (will auto-delete after hiding)
    ToastNotification* toast = new ToastNotification(parent, message, durationMs);
    toast->showNotification();
}

ToastNotification::ToastNotification(QWidget* parent, const QString& message, int durationMs)
    : QWidget(parent)
    , label_(new QLabel(message, this))
    , timer_(new QTimer(this))
    , duration_(durationMs)
{
    // Set widget flags for frameless, non-modal overlay
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_DeleteOnClose);

    // Setup label styling
    label_->setStyleSheet(
        "QLabel {"
        "    background-color: rgba(50, 50, 50, 220);"
        "    color: white;"
        "    padding: 12px 24px;"
        "    border-radius: 8px;"
        "    font-size: 14px;"
        "}"
    );
    label_->setAlignment(Qt::AlignCenter);
    label_->setWordWrap(true);
    label_->setMaximumWidth(400);

    // Layout
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(label_);
    layout->setContentsMargins(0, 0, 0, 0);

    // Adjust size to content
    adjustSize();

    // Position at bottom-center of parent
    if (parent) {
        int x = (parent->width() - width()) / 2;
        int y = parent->height() - height() - 50;  // 50px from bottom
        move(parent->mapToGlobal(QPoint(x, y)));
    }

    // Setup timer for auto-dismiss
    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, &ToastNotification::onTimeout);
}

void ToastNotification::showNotification()
{
    // Show the widget (explicitly call QWidget::show to avoid name collision)
    QWidget::show();
    raise();

    // Start auto-dismiss timer
    timer_->start(duration_);
}

void ToastNotification::hideNotification()
{
    // Close and delete
    close();
}

void ToastNotification::onTimeout()
{
    hideNotification();
}
