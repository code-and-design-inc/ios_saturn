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

#include "ReportListItemDelegate.h"
#include "../models/ReportListModel.h"
#include "../services/ImageCacheManager.h"
#include <QPainter>
#include <QApplication>
#include <QStyle>

ReportListItemDelegate::ReportListItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
    , imageCacheManager_(nullptr)
{
}

void ReportListItemDelegate::setImageCacheManager(ImageCacheManager* cacheManager)
{
    imageCacheManager_ = cacheManager;
}

void ReportListItemDelegate::paint(QPainter* painter,
                                    const QStyleOptionViewItem& option,
                                    const QModelIndex& index) const
{
    if (!index.isValid()) {
        return;
    }

    painter->save();

    // Draw background and selection highlight
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // Fill background
    if (opt.state & QStyle::State_Selected) {
        painter->fillRect(opt.rect, opt.palette.highlight());
    } else if (opt.state & QStyle::State_MouseOver) {
        painter->fillRect(opt.rect, opt.palette.alternateBase());
    } else {
        painter->fillRect(opt.rect, opt.palette.base());
    }

    // Get data from model
    QString timestamp = index.data(ReportListModel::FormattedTimestampRole).toString();
    int rating = index.data(ReportListModel::RatingRole).toInt();
    QString description = index.data(ReportListModel::DescriptionRole).toString();
    bool isReproducible = index.data(ReportListModel::IsReproducibleRole).toBool();
    QString screenshotUrl = index.data(ReportListModel::ScreenshotUrlRole).toString();

    // Set text color based on selection
    QColor textColor = (opt.state & QStyle::State_Selected)
                       ? opt.palette.highlightedText().color()
                       : opt.palette.text().color();

    // Calculate layout areas (relative to opt.rect)
    int contentLeft = opt.rect.left() + PADDING;
    int contentTop = opt.rect.top() + PADDING;

    // Screenshot area (left side, 100px wide)
    QRect screenshotRect(contentLeft, contentTop, SCREENSHOT_WIDTH, ITEM_HEIGHT - 2 * PADDING);

    // Content area (right side, after screenshot)
    int textLeft = contentLeft + SCREENSHOT_WIDTH + PADDING;
    int textWidth = opt.rect.width() - (SCREENSHOT_WIDTH + 3 * PADDING);

    // Draw screenshot if available
    if (!screenshotUrl.isEmpty() && imageCacheManager_) {
        QPixmap screenshot = imageCacheManager_->getImage(screenshotUrl, SCREENSHOT_WIDTH);
        if (!screenshot.isNull()) {
            // Center screenshot vertically in its area
            int imgHeight = screenshot.height();
            int yOffset = (screenshotRect.height() - imgHeight) / 2;
            QRect imgRect(screenshotRect.x(), screenshotRect.y() + yOffset,
                         screenshot.width(), screenshot.height());
            painter->drawPixmap(imgRect, screenshot);
        } else {
            // Draw placeholder
            painter->setPen(QPen(opt.palette.mid().color(), 1));
            painter->setBrush(opt.palette.alternateBase());
            painter->drawRect(screenshotRect);
            painter->setPen(textColor);
            painter->setFont(QFont(opt.font.family(), 8));
            painter->drawText(screenshotRect, Qt::AlignCenter, "Loading...");
        }
    }

    // Draw timestamp (top-right area)
    QRect timestampRect(textLeft, contentTop, textWidth / 2, 20);
    painter->setPen(textColor);
    painter->setFont(QFont(opt.font.family(), 9));
    painter->drawText(timestampRect, Qt::AlignLeft | Qt::AlignVCenter, timestamp);

    // Draw rating stars (top-right corner)
    QRect ratingRect(textLeft + textWidth / 2, contentTop, textWidth / 2, 20);
    drawRatingStars(painter, ratingRect, rating);

    // Draw "指摘内容" label
    QRect labelRect(textLeft, contentTop + 25, textWidth, 20);
    painter->setPen(textColor);
    painter->setFont(QFont(opt.font.family(), 9, QFont::Bold));
    painter->drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, "指摘内容:");

    // Draw description (full text, word wrap)
    QRect descRect(textLeft, contentTop + 45, textWidth, ITEM_HEIGHT - contentTop - 55);
    painter->setPen(textColor);
    painter->setFont(QFont(opt.font.family(), 10));
    painter->drawText(descRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, description);

    // Draw reproducible badge (bottom-right)
    if (isReproducible) {
        QRect badgeRect(opt.rect.right() - BADGE_WIDTH - PADDING,
                       opt.rect.bottom() - 25,
                       BADGE_WIDTH,
                       20);
        drawReproducibleBadge(painter, badgeRect);
    }

    // Draw border
    painter->setPen(QPen(opt.palette.mid().color(), 1));
    painter->drawLine(opt.rect.bottomLeft(), opt.rect.bottomRight());

    painter->restore();
}

QSize ReportListItemDelegate::sizeHint(const QStyleOptionViewItem& option,
                                       const QModelIndex& index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);

    // Fixed height for all items
    return QSize(0, ITEM_HEIGHT);
}

void ReportListItemDelegate::drawRatingStars(QPainter* painter,
                                             const QRect& rect,
                                             int rating) const
{
    // Draw stars from right to left
    int x = rect.right();
    const int starSpacing = 2;

    painter->save();

    for (int i = 0; i < 5; i++) {
        QRect starRect(x - STAR_SIZE, rect.top(), STAR_SIZE, STAR_SIZE);

        // Filled star if within rating, empty star otherwise
        if (i < rating) {
            painter->setBrush(QColor(255, 215, 0)); // Gold color
            painter->setPen(Qt::NoPen);
        } else {
            painter->setBrush(Qt::transparent);
            painter->setPen(QColor(128, 128, 128)); // Gray outline
        }

        // Draw simple star polygon
        QPolygonF star;
        QPointF center(starRect.center());
        double outerRadius = STAR_SIZE / 2.0;
        double innerRadius = STAR_SIZE / 4.0;

        for (int j = 0; j < 10; j++) {
            double angle = (j * 36 - 90) * M_PI / 180.0;
            double radius = (j % 2 == 0) ? outerRadius : innerRadius;
            star << QPointF(center.x() + radius * cos(angle),
                           center.y() + radius * sin(angle));
        }

        painter->drawPolygon(star);

        x -= (STAR_SIZE + starSpacing);
    }

    painter->restore();
}

void ReportListItemDelegate::drawReproducibleBadge(QPainter* painter,
                                                   const QRect& rect) const
{
    painter->save();

    // Draw badge background
    painter->setBrush(QColor(76, 175, 80)); // Green color
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(rect, 3, 3);

    // Draw badge text
    painter->setPen(Qt::white);
    painter->setFont(QFont(painter->font().family(), 8, QFont::Bold));
    painter->drawText(rect, Qt::AlignCenter, "REPRODUCIBLE");

    painter->restore();
}
