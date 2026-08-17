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

#ifndef IMAGE_CACHE_MANAGER_H
#define IMAGE_CACHE_MANAGER_H

#include <QObject>
#include <QPixmap>
#include <QString>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>

/**
 * @class ImageCacheManager
 * @brief Manages downloading and caching of images from HTTPS URLs
 *
 * Features:
 * - Downloads images from HTTPS URLs
 * - Disk cache for persistent storage
 * - Memory cache for fast access
 * - Lazy loading (only loads visible items)
 * - Automatic scaling to target width while maintaining aspect ratio
 */
class ImageCacheManager : public QObject {
    Q_OBJECT

public:
    explicit ImageCacheManager(QObject* parent = nullptr);
    virtual ~ImageCacheManager();

    /**
     * @brief Load image from URL (cached or download)
     * @param url HTTPS URL of the image
     * @param targetWidth Target width in pixels (maintains aspect ratio)
     * @return Cached pixmap if available, null pixmap if loading
     *
     * If image is not in cache, starts download and emits imageLoaded() when ready.
     */
    QPixmap getImage(const QString& url, int targetWidth = 100);

    /**
     * @brief Check if image is in cache (memory or disk)
     * @param url Image URL
     * @return True if cached
     */
    bool isCached(const QString& url) const;

    /**
     * @brief Clear all caches
     */
    void clearCache();

    /**
     * @brief Get cache directory path
     * @return Path to disk cache directory
     */
    QString getCacheDirectory() const;

signals:
    /**
     * @brief Emitted when image download completes
     * @param url Image URL
     * @param pixmap Loaded and scaled pixmap
     */
    void imageLoaded(const QString& url, const QPixmap& pixmap);

    /**
     * @brief Emitted when image download fails
     * @param url Image URL
     * @param error Error message
     */
    void imageLoadFailed(const QString& url, const QString& error);

private slots:
    void onDownloadFinished();

private:
    /**
     * @brief Get cache file path for URL
     * @param url Image URL
     * @return Local cache file path
     */
    QString getCacheFilePath(const QString& url) const;

    /**
     * @brief Generate cache key from URL
     * @param url Image URL
     * @return Cache key (MD5 hash of URL)
     */
    QString getCacheKey(const QString& url) const;

    /**
     * @brief Load image from disk cache
     * @param url Image URL
     * @param targetWidth Target width for scaling
     * @return Pixmap if found, null pixmap otherwise
     */
    QPixmap loadFromDiskCache(const QString& url, int targetWidth);

    /**
     * @brief Save image to disk cache
     * @param url Image URL
     * @param pixmap Image to save
     */
    void saveToDiskCache(const QString& url, const QPixmap& pixmap);

    /**
     * @brief Scale pixmap to target width maintaining aspect ratio
     * @param pixmap Original pixmap
     * @param targetWidth Target width
     * @return Scaled pixmap
     */
    QPixmap scaleToWidth(const QPixmap& pixmap, int targetWidth) const;

private:
    QNetworkAccessManager* networkManager_;     ///< Network manager for downloads
    QHash<QString, QPixmap> memoryCache_;       ///< Memory cache (URL -> Pixmap)
    QHash<QNetworkReply*, QString> activeDownloads_; ///< Active downloads (Reply -> URL)
    QString cacheDirectory_;                    ///< Disk cache directory path
};

#endif // IMAGE_CACHE_MANAGER_H
