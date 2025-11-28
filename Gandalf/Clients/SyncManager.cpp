#include "SyncManager.h"
#include "Oracle/ApiClient.h"
#include "Oracle/Logger.h"

SyncManager& SyncManager::instance()
{
    static SyncManager self;
    return self;
}

SyncManager::SyncManager(QObject *parent) : QObject(parent), m_isBusy(false)
{
    // Підписуємося на сигнали ApiClient
    connect(&ApiClient::instance(), &ApiClient::clientSyncRequestFinished,
            this, &SyncManager::onApiSyncFinished);
}

void SyncManager::queueClient(int clientId)
{
    if (m_queue.contains(clientId)) return;

    m_queue.enqueue(clientId);
    emit queueProgress(m_queue.count());

    if (!m_isBusy) {
        processNext();
    }
}

void SyncManager::processNext()
{
    if (m_queue.isEmpty()) {
        m_isBusy = false;
        emit allFinished();
        return;
    }

    int clientId = m_queue.dequeue();
    m_isBusy = true; // Займаємо чергу

    emit queueProgress(m_queue.count());
    emit clientSyncStarted(clientId);

    logInfo() << "Sending API sync request for client" << clientId;

    // --- ГОЛОВНА ЗМІНА: ВИКЛИК API ЗАМІСТЬ DB ---
    ApiClient::instance().syncClient(clientId);
}

void SyncManager::onApiSyncFinished(int clientId, bool success, QString message)
{
    // Цей метод викличеться, коли прийде відповідь від сервера
    logInfo() << "API sync request finished for client" << clientId << success << message;

    emit clientSyncFinished(clientId, success, message);

    // Беремо наступного (невелика затримка для краси, щоб UI не миготів)
    processNext();
}

bool SyncManager::isSyncing() const
{
    return m_isBusy;
}
