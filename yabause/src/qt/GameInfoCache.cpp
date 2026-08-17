#include "GameInfoCache.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>

// Schema version. Bump when the stored field set changes so stale entries are
// ignored instead of mis-restored.
static const int kCacheVersion = 1;

GameInfoCache::GameInfoCache() {}

QString GameInfoCache::cacheFilePath() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty()) {
        dir = QDir::homePath();
    }
    QDir().mkpath(dir);
    return QDir(dir).filePath("gamelist_cache.json");
}

void GameInfoCache::load() {
    m_entries.clear();

    QFile file(cacheFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }

    QJsonObject root = doc.object();
    if (root.value("version").toInt() != kCacheVersion) {
        return;
    }

    QJsonObject games = root.value("games").toObject();
    for (auto it = games.begin(); it != games.end(); ++it) {
        QJsonObject obj = it.value().toObject();

        Entry entry;
        entry.mtime = static_cast<qint64>(obj.value("mtime").toDouble());
        entry.size = static_cast<qint64>(obj.value("size").toDouble());

        QGameInfo& info = entry.info;
        info.filePath = it.key();
        info.makerId = obj.value("makerId").toString();
        info.productNumber = obj.value("productNumber").toString();
        info.version = obj.value("version_str").toString();
        info.releaseDate = obj.value("releaseDate").toString();
        info.area = obj.value("area").toString();
        info.inputDevice = obj.value("inputDevice").toString();
        info.deviceInformation = obj.value("deviceInformation").toString();
        info.gameTitle = obj.value("gameTitle").toString();
        info.displayName = obj.value("displayName").toString();
        info.imageUrl = obj.value("imageUrl").toString();

        // Self-heal: caches written by a run that could not resolve the
        // CloudService token (e.g. launched from a directory without
        // settings.ini) carry empty image URLs forever. Recompute from the
        // product number; keep the stored URL if the token is unavailable
        // right now so a bad launch cannot poison a good cache.
        QString recomputed = QGameInfo::coverImageUrl(info.productNumber);
        if (!recomputed.isEmpty())
            info.imageUrl = recomputed;

        m_entries.insert(it.key(), entry);
    }
}

QGameInfo* GameInfoCache::lookup(const QString& absPath, qint64 mtime, qint64 size) const {
    auto it = m_entries.constFind(absPath);
    if (it == m_entries.constEnd()) {
        return nullptr;
    }
    if (it->mtime != mtime || it->size != size) {
        return nullptr;
    }
    return new QGameInfo(it->info);
}

void GameInfoCache::put(const QString& absPath, qint64 mtime, qint64 size, const QGameInfo& info) {
    Entry entry;
    entry.mtime = mtime;
    entry.size = size;
    entry.info = info;
    m_entries.insert(absPath, entry);
}

void GameInfoCache::pruneMissing(const QSet<QString>& seenPaths) {
    for (auto it = m_entries.begin(); it != m_entries.end();) {
        if (!seenPaths.contains(it.key())) {
            it = m_entries.erase(it);
        } else {
            ++it;
        }
    }
}

void GameInfoCache::save() const {
    QJsonObject games;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        const Entry& entry = it.value();
        const QGameInfo& info = entry.info;

        QJsonObject obj;
        obj.insert("mtime", static_cast<double>(entry.mtime));
        obj.insert("size", static_cast<double>(entry.size));
        obj.insert("makerId", info.makerId);
        obj.insert("productNumber", info.productNumber);
        obj.insert("version_str", info.version);
        obj.insert("releaseDate", info.releaseDate);
        obj.insert("area", info.area);
        obj.insert("inputDevice", info.inputDevice);
        obj.insert("deviceInformation", info.deviceInformation);
        obj.insert("gameTitle", info.gameTitle);
        obj.insert("displayName", info.displayName);
        obj.insert("imageUrl", info.imageUrl);

        games.insert(it.key(), obj);
    }

    QJsonObject root;
    root.insert("version", kCacheVersion);
    root.insert("games", games);

    QFile file(cacheFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.close();
}
