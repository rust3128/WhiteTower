#include "SyncManager.h"
#include "Oracle/ApiClient.h"
#include "Oracle/Logger.h"

SyncManager& SyncManager::instance()
{
    static SyncManager self;
    return self;
}

// Ініціалізуємо m_activeClient зі значенням -1
SyncManager::SyncManager(QObject *parent) : QObject(parent), m_isBusy(false), m_activeClient(-1), m_batchTotal(0), m_batchCompleted(0)
{
    connect(&ApiClient::instance(), &ApiClient::clientSyncRequestFinished,
            this, &SyncManager::onApiSyncFinished);
}

void SyncManager::queueClient(int clientId)
{
    if (m_queue.contains(clientId) || m_activeClient == clientId || m_trackedClients.contains(clientId)) return;

    m_queue.enqueue(clientId);

    // --- ПОЧИНАЄМО ТРЕКАТИ ---
    m_trackedClients.insert(clientId);
    m_batchTotal++; // Збільшуємо загальну кількість завдань

    emit queueProgress(m_queue.count());

    if (!m_isBusy) {
        processNext();
    }
}
void SyncManager::processNext()
{
    if (m_queue.isEmpty()) {
        m_isBusy = false;
        m_activeClient = -1; // Очищаємо активного
        emit allFinished();
        return;
    }

    m_activeClient = m_queue.dequeue(); // Запам'ятовуємо, кого обробляємо
    m_isBusy = true;

    emit queueProgress(m_queue.count());
    emit clientSyncStarted(m_activeClient);

    logInfo() << "Sending API sync request for client" << m_activeClient;
    ApiClient::instance().syncClient(m_activeClient);
}

void SyncManager::onApiSyncFinished(int clientId, bool success, QString message)
{
    // Цей метод викликається миттєво, щойно сервер відповів 202 Accepted
    logInfo() << "API accepted sync command for client" << clientId << "Success:" << success;

    // Сповіщаємо UI, що команду доставлено (успішно чи з помилкою мережі)
    emit syncRequestSent(clientId, success, message);

    m_isBusy = false;
    m_activeClient = -1; // Звільняємо активного клієнта

    // Беремо наступного з черги
    processNext();
}

void SyncManager::markCompleted(int clientId)
{
    if (m_trackedClients.contains(clientId)) {
        m_trackedClients.remove(clientId);
        m_batchCompleted++;

        // Якщо всі відслідковувані клієнти завершилися, скидаємо лічильники для наступного разу
        if (m_trackedClients.isEmpty()) {
            m_batchTotal = 0;
            m_batchCompleted = 0;
        }
    }
}
