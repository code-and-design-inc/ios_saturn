#ifndef FILESEARCHWORKER_H
#define FILESEARCHWORKER_H

#include <QObject>
#include <QFileInfo>
#include "GameInfoCache.h"

class QGameInfo;

class FileSearchWorker : public QObject
{
    Q_OBJECT

public:
    explicit FileSearchWorker(QObject *parent = nullptr);

public slots:
    void search(const QString &path, bool forceRescan = false);

signals:
    void fileFound(const QFileInfo &fileInfo, const QGameInfo * gameInfo);
    void searchCompleted(int fileCount);
    void searchStarted();

private:
    bool m_shouldStop;
    GameInfoCache m_cache;

public slots:
    void stop();
};

#endif // FILESEARCHWORKER_H