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

#include "ImageCacheManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <QNetworkRequest>
#include <QDebug>

ImageCacheManager::ImageCacheManager(QObject* parent)
    : QObject(parent)
    , networkManager_(new QNetworkAccessManager(this))
{
    // Setup cache directory
    cacheDirectory_ = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                      + "/screenshot_cache";

    QDir dir;
    if (!dir.exists(cacheDirectory_)) {
        dir.mkpath(cacheDirectory_);
        qDebug() << "Created image cache directory:" << cacheDirectory_;
    }
}

ImageCacheManager::~ImageCacheManager()
{
    // Cancel any active downloads
    for (QNetworkReply* reply : activeDownloads_.keys()) {
        reply->abort();
        reply->deleteLater();
    }
}

QPixmap ImageCacheManager::getImage(const QString& url, int targetWidth)
{
    if (url.isEmpty()) {
        return QPixmap();
    }

    // Check memory cache first
    if (memoryCache_.contains(url)) {
        return memoryCache_[url];
    }

    // Try to load from disk cache
    QPixmap pixmap = loadFromDiskCache(url, targetWidth);
    if (!pixmap.isNull()) {
        // Cache in memory for faster access
        memoryCache_[url] = pixmap;
        return pixmap;
    }

    // Not in cache - start download if not already downloading
    bool alreadyDownloading = false;
    for (const QString& downloadUrl : activeDownloads_.values()) {
        if (downloadUrl == url) {
            alreadyDownloading = true;
            break;
        }
    }

    if (!alreadyDownloading) {
        qDebug() << "Starting download for image:" << url;

        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
        request.setRawHeader("User-Agent", "YabaSanshiro/1.0");

        QNetworkReply* reply = networkManager_->get(request);
        activeDownloads_[reply] = url;

        connect(reply, &QNetworkReply::finished, this, &ImageCacheManager::onDownloadFinished);
    }

    // Return null pixmap - will emit imageLoaded() when download completes
    return QPixmap();
}

bool ImageCacheManager::isCached(const QString& url) const
{
    if (memoryCache_.contains(url)) {
        return true;
    }

    QString cacheFile = getCacheFilePath(url);
    return QFile::exists(cacheFile);
}

void ImageCacheManager::clearCache()
{
    // Clear memory cache
    memoryCache_.clear();

    // Clear disk cache
    QDir cacheDir(cacheDirectory_);
    QStringList files = cacheDir.entryList(QDir::Files);
    for (const QString& file : files) {
        cacheDir.remove(file);
    }

    qDebug() << "Image cache cleared";
}

QString ImageCacheManager::getCacheDirectory() const
{
    return cacheDirectory_;
}

void ImageCacheManager::onDownloadFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    QString url = activeDownloads_.value(reply);
    activeDownloads_.remove(reply);

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Image download failed:" << url << "-" << reply->errorString();
        emit imageLoadFailed(url, reply->errorString());
        reply->deleteLater();
        return;
    }

    // Read image data
    QByteArray imageData = reply->readAll();
    reply->deleteLater();

    if (imageData.isEmpty()) {
        qWarning() << "Downloaded image is empty:" << url;
        emit imageLoadFailed(url, "Empty image data");
        return;
    }

    // Load pixmap from data
    QPixmap pixmap;
    if (!pixmap.loadFromData(imageData)) {
        qWarning() << "Failed to load pixmap from data:" << url;
        emit imageLoadFailed(url, "Invalid image format");
        return;
    }

    qDebug() << "Image downloaded successfully:" << url
             << "Size:" << pixmap.size();

    // Save to disk cache (original size)
    saveToDiskCache(url, pixmap);

    // Scale to target width for display
    QPixmap scaledPixmap = scaleToWidth(pixmap, 100); // Default 100px width

    // Cache in memory
    memoryCache_[url] = scaledPixmap;

    // Emit success signal
    emit imageLoaded(url, scaledPixmap);
}

QString ImageCacheManager::getCacheFilePath(const QString& url) const
{
    QString cacheKey = getCacheKey(url);
    return cacheDirectory_ + "/" + cacheKey + ".png";
}

QString ImageCacheManager::getCacheKey(const QString& url) const
{
    // Use MD5 hash of URL as cache key
    QByteArray hash = QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Md5);
    return QString(hash.toHex());
}

QPixmap ImageCacheManager::loadFromDiskCache(const QString& url, int targetWidth)
{
    QString cacheFile = getCacheFilePath(url);

    if (!QFile::exists(cacheFile)) {
        return QPixmap();
    }

    QPixmap pixmap(cacheFile);
    if (pixmap.isNull()) {
        qWarning() << "Failed to load cached image:" << cacheFile;
        // Remove corrupted cache file
        QFile::remove(cacheFile);
        return QPixmap();
    }

    qDebug() << "Loaded image from disk cache:" << url;

    // Scale to target width
    return scaleToWidth(pixmap, targetWidth);
}

void ImageCacheManager::saveToDiskCache(const QString& url, const QPixmap& pixmap)
{
    QString cacheFile = getCacheFilePath(url);

    if (!pixmap.save(cacheFile, "PNG")) {
        qWarning() << "Failed to save image to disk cache:" << cacheFile;
        return;
    }

    qDebug() << "Saved image to disk cache:" << cacheFile;
}

QPixmap ImageCacheManager::scaleToWidth(const QPixmap& pixmap, int targetWidth) const
{
    if (pixmap.isNull()) {
        return QPixmap();
    }

    // If already at target width or smaller, return as-is
    if (pixmap.width() <= targetWidth) {
        return pixmap;
    }

    // Scale maintaining aspect ratio
    return pixmap.scaledToWidth(targetWidth, Qt::SmoothTransformation);
}
