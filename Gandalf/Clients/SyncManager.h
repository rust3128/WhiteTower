#ifndef SYNCMANAGER_H
#define SYNCMANAGER_H

#include <QObject>
#include <QQueue>

class SyncManager : public QObject
{
    Q_OBJECT
public:
    static SyncManager& instance();
    void queueClient(int clientId);
    bool isSyncing() const;

signals:
    void clientSyncStarted(int clientId);
    void clientSyncFinished(int clientId, bool success, QString message);
    void queueProgress(int remainingCount);
    void allFinished();

private:
    explicit SyncManager(QObject *parent = nullptr);

    // Слот для відповіді від ApiClient
    void onApiSyncFinished(int clientId, bool success, QString message);

    void processNext();

    QQueue<int> m_queue;
    bool m_isBusy;
};

#endif // SYNCMANAGER_H
