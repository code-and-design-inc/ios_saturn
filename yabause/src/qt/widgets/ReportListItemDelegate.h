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

#ifndef REPORT_LIST_ITEM_DELEGATE_H
#define REPORT_LIST_ITEM_DELEGATE_H

#include <QStyledItemDelegate>

/**
 * @class ReportListItemDelegate
 * @brief Custom delegate for rendering report list items
 *
 * Responsibilities:
 * - Render each report item with custom layout
 * - Display timestamp, rating stars, description preview
 * - Show reproducible indicator badge for reports with save states
 * - Handle selection highlighting and hover states
 *
 * Thread Safety: Must be used only on Qt main thread (UI operations).
 */
class ReportListItemDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit ReportListItemDelegate(QObject* parent = nullptr);
    virtual ~ReportListItemDelegate() = default;

    /**
     * @brief Paint the item
     * @param painter QPainter for drawing
     * @param option Style options for the item
     * @param index Model index of the item
     */
    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    /**
     * @brief Get size hint for the item
     * @param option Style options
     * @param index Model index
     * @return Recommended size for the item
     */
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    /**
   * @brief Set image cache manager
   * @param cacheManager Pointer to image cache manager
   */
    void setImageCacheManager(class ImageCacheManager* cacheManager);

private:
    /**
     * @brief Draw rating stars
     * @param painter QPainter for drawing
     * @param rect Area to draw stars in
     * @param rating Rating value (1-5)
     */
    void drawRatingStars(QPainter* painter, const QRect& rect, int rating) const;

    /**
     * @brief Draw reproducible badge
     * @param painter QPainter for drawing
     * @param rect Area to draw badge in
     */
    void drawReproducibleBadge(QPainter* painter, const QRect& rect) const;

private:
    class ImageCacheManager* imageCacheManager_;  ///< Image cache manager for screenshots

    static constexpr int ITEM_HEIGHT = 120;      ///< Increased height for screenshot
    static constexpr int PADDING = 8;            ///< Padding around item content
    static constexpr int STAR_SIZE = 12;         ///< Size of rating star icons
    static constexpr int BADGE_WIDTH = 100;      ///< Width of reproducible badge
    static constexpr int SCREENSHOT_WIDTH = 100; ///< Screenshot width in pixels
};

#endif // REPORT_LIST_ITEM_DELEGATE_H
