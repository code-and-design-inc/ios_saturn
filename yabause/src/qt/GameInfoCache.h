#ifndef GAMEINFOCACHE_H
#define GAMEINFOCACHE_H

#include <QString>
#include <QHash>
#include <QSet>
#include "QGameInfo.h"

// Persistent JSON-backed cache of scanned game metadata.
// Key is the absolute file path; an entry is considered valid only when the
// stored modification time and size match the current file on disk. This lets
// the startup scan skip the expensive disc-header read for unchanged files.
class GameInfoCache {
public:
    GameInfoCache();

    // Load the cache file from disk into memory. Safe to call on a missing file.
    void load();

    // Return a heap-allocated QGameInfo restored from cache when an entry for
    // absPath exists and its mtime/size match the arguments. Caller takes
    // ownership. Returns nullptr on miss or mismatch.
    QGameInfo* lookup(const QString& absPath, qint64 mtime, qint64 size) const;

    // Insert/update the entry for absPath.
    void put(const QString& absPath, qint64 mtime, qint64 size, const QGameInfo& info);

    // Drop entries whose paths were not present in seenPaths (deleted files).
    void pruneMissing(const QSet<QString>& seenPaths);

    // Write the in-memory cache back to disk.
    void save() const;

private:
    struct Entry {
        qint64 mtime;
        qint64 size;
        QGameInfo info;
    };

    static QString cacheFilePath();

    QHash<QString, Entry> m_entries;
};

#endif // GAMEINFOCACHE_H
