#include "filesearchworker.h"
#include <QDirIterator>
#include <QDateTime>
#include <QSet>
#include "QGameInfo.h"

FileSearchWorker::FileSearchWorker(QObject *parent)
    : QObject(parent)
    , m_shouldStop(false)
{
    m_cache.load();
}

void FileSearchWorker::search(const QString &path, bool forceRescan)
{
    m_shouldStop = false;
    emit searchStarted();

    QDirIterator it(path, QDirIterator::Subdirectories);
    int fileCount = 0;
    QSet<QString> seenPaths;

    while (it.hasNext() && !m_shouldStop) {
        QString filePath = it.next();
        QFileInfo fileInfo(filePath);

        if (fileInfo.isFile()) {
          try {
            QString extension = fileInfo.suffix().toLower();
            if (extension == "chd" || extension == "cue" || extension == "mds" || extension == "ccd") {
              QString absPath = fileInfo.absoluteFilePath();
              qint64 mtime = fileInfo.lastModified().toMSecsSinceEpoch();
              qint64 size = fileInfo.size();
              seenPaths.insert(absPath);

              QGameInfo* info = nullptr;

              // Try the cache first; only read the disc header on a miss.
              if (!forceRescan) {
                info = m_cache.lookup(absPath, mtime, size);
              }

              if (info == nullptr) {
                if (extension == "chd") {
                  info = QGameInfo::fromChdFile(fileInfo.filePath());
                }
                else if (extension == "cue") {
                  info = QGameInfo::fromCueFile(fileInfo.filePath());
                }
                else if (extension == "mds") {
                  info = QGameInfo::fromMdsFile(fileInfo.filePath());
                }
                else if (extension == "ccd") {
                  info = QGameInfo::fromCcdFile(fileInfo.filePath());
                }
                if (info == nullptr) {
                  continue;
                }
                m_cache.put(absPath, mtime, size, *info);
              }

              emit fileFound(fileInfo, info);
              fileCount++;
            }
          }
          catch (const GameInfoError& e) {
            qDebug() << e.errorMessage;
          }
        }
    }

    // Keep the cache in sync with the directory state, then persist it.
    if (!m_shouldStop) {
      m_cache.pruneMissing(seenPaths);
    }
    m_cache.save();

    emit searchCompleted(fileCount);
}

void FileSearchWorker::stop()
{
    m_shouldStop = true;
}
