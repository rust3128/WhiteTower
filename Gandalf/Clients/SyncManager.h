#ifndef SYNCMANAGER_H
#define SYNCMANAGER_H

#include <QObject>
#include <QQueue>
#include <QSet>

class SyncManager : public QObject
{
    Q_OBJECT
public:
    static SyncManager& instance();
    void queueClient(int clientId);
    bool isSyncing() const;
    int activeClient() const { return m_activeClient; }
    bool isClientInQueue(int clientId) const { return m_queue.contains(clientId); }
    // --- НОВІ МЕТОДИ ДЛЯ ВІДСОТКІВ ---
    int getBatchTotal() const { return m_batchTotal; }
    int getBatchCompleted() const { return m_batchCompleted; }
    bool isTracked(int clientId) const { return m_trackedClients.contains(clientId); }
    void markCompleted(int clientId);
signals:
    void clientSyncStarted(int clientId);
    void syncRequestSent(int clientId, bool success, QString message);
    void queueProgress(int remainingCount);
    void allFinished();

private:
    explicit SyncManager(QObject *parent = nullptr);

    // Слот для відповіді від ApiClient
    void onApiSyncFinished(int clientId, bool success, QString message);

    void processNext();

    QQueue<int> m_queue;
    bool m_isBusy;
    int m_activeClient;
    QSet<int> m_trackedClients; // Ті, хто зараз крутиться на сервері
    int m_batchTotal;           // Всього завдань у поточному запуску
    int m_batchCompleted;       // Скільки вже повернули SUCCESS/ERROR
};

#endif // SYNCMANAGER_H
