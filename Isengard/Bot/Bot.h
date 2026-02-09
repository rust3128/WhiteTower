#ifndef BOT_H
#define BOT_H

#include "AttachmentManager.h"

#include <QObject>
#include "Oracle/ApiClient.h"
#include <QMap>
#include <QTimer>

class TelegramClient;
class ApiClient;
class QJsonObject;

class Bot : public QObject
{
    Q_OBJECT
public:
    explicit Bot(const QString& botToken, QObject *parent = nullptr);

    void start();

private slots:
    // Слот для обробки оновлень від Telegram
    void onUpdatesReceived(const QJsonArray& updates);

    // Слоти для обробки відповідей від нашого сервера
    void onUserRegistered(const QJsonObject& response, qint64 telegramId);
    void onUserRegistrationFailed(const ApiError& error, qint64 telegramId);

    void onUserStatusReceived(const QJsonObject& status, const QJsonObject& message);
    void onUserStatusCheckFailed(const ApiError& error);

    // --- СЛОТИ ДЛЯ КЛІЄНТІВ ---
    void onBotClientsReceived(const QJsonArray& clients, qint64 telegramId);
    void onBotClientsFailed(const ApiError& error, qint64 telegramId);

    void onAdminRequestsReceived(const QJsonArray& requests, qint64 telegramId);
    void onAdminRequestsFailed(const ApiError& error, qint64 telegramId);

    // --- ДОДАЙТЕ ЦІ ДВА РЯДКИ ---
    void onActiveUsersReceived(const QJsonArray& users, qint64 telegramId);
    void onActiveUsersFailed(const ApiError& error, qint64 telegramId);

    void onStationsReceived(const QJsonArray& stations, qint64 telegramId, int clientId);
    void onStationsFailed(const ApiError& error, qint64 telegramId, int clientId);
    void onStationDetailsReceived(const QJsonObject& station, qint64 telegramId, int clientId);
    void onStationDetailsFailed(const ApiError& error, qint64 telegramId, int clientId);

    void onStationPosDataReceived(const QJsonArray& data, int clientId, int terminalId, qint64 telegramId);
    void onStationPosDataFailed(const ApiError& error, qint64 telegramId);

    // Слоти для резервуарів
    void onStationTanksReceived(const QJsonArray& data, int clientId, int terminalId, qint64 telegramId);
    void onStationTanksFailed(const ApiError& error, qint64 telegramId);



    void onDispenserConfigReceived(const QJsonArray& config, int clientId, int terminalId, qint64 telegramId);
    void onDispenserConfigFailed(const ApiError& error, qint64 telegramId);

    void onRedmineTasksFetched(const QJsonArray& tasks, qint64 telegramId, int userId);
    void onRedmineTasksFetchFailed(const ApiError& error, qint64 telegramId, int userId);

    // !!! НОВІ СЛОТИ ДЛЯ JIRA !!!
    void onJiraTasksFetched(const QJsonArray& tasks, qint64 telegramId);
    void onJiraTasksFetchFailed(const ApiError& error, qint64 telegramId);

    void onTaskDetailsFetched(const QJsonObject& taskDetails, qint64 telegramId);
    void onTaskDetailsFetchFailed(const ApiError& error, qint64 telegramId);

    void onAssignTaskSuccess(const QJsonObject& response, qint64 telegramId);
    void onAssignTaskFailed(const ApiError& error, qint64 telegramId);

    void onReportTaskSuccess(const QJsonObject& response, qint64 telegramId);
    void onReportTaskFailed(const ApiError& error, qint64 telegramId);

    void handleSessionTimeout();

private:
    // Тип-вказівник на метод-обробник
    using CommandHandler = void (Bot::*)(const QJsonObject& message);
    using CallbackHandler = void (Bot::*)(const QJsonObject& query, const QStringList& parts);
    // --- Методи налаштування ---
    void setupCommandHandlers();
    void setupCallbackHandlers();
    void setupConnections();

    // --- Методи-маршрутизатори ---
    void processActiveUserCommand(const QJsonObject& message);
    void processActiveAdminCommand(const QJsonObject& message);

    // --- Обробники команд ---
    void handleUserHelp(const QJsonObject& message);
    void handleAdminHelp(const QJsonObject& message);
    void handleMyTasks(const QJsonObject& message);
    void handleAdminRequests(const QJsonObject& message);
    void handleClientsCommand(const QJsonObject& message);

    void handleUnknownCommand(const QJsonObject& message);

    // --- Методи для меню (ЗМІНЕНО ПІДПИС) ---
    void sendUserMenu(const QJsonObject& message);    // <-- ЗМІНЕНО
    void sendAdminMenu(const QJsonObject& message);   // <-- ЗМІНЕНО
    void handleAdminUsers(const QJsonObject& message);

    void handleCallbackQuery(const QJsonObject& callbackQuery);
    void handleStationNumberInput(const QJsonObject& message);

    // Обробники для "clients:"
    void handleCallbackClientsMain(const QJsonObject& query, const QStringList& parts);
    // Обробники для "client:"
    void handleCallbackClientSelect(const QJsonObject& query, const QStringList& parts);
    // Обробники для "stations:"
    void handleCallbackStationsList(const QJsonObject& query, const QStringList& parts);
    void handleCallbackStationsEnter(const QJsonObject& query, const QStringList& parts);
    void handleCallbackStationsPage(const QJsonObject& query, const QStringList& parts);
    void handleCallbackStationsClose(const QJsonObject& query, const QStringList& parts);
    // Обробники для "station:"
    void handleCallbackStationStub(const QJsonObject& query, const QStringList& parts);
    void handleCallbackStationMap(const QJsonObject& query, const QStringList& parts);
    // Загальний обробник
    void handleCallbackUnknown(const QJsonObject& query, const QStringList& parts);
    // --- (КІНЕЦЬ НОВОГО БЛОКУ) ---

    void sendPaginatedStations(qint64 telegramId, int clientId, int page, int messageId);

    void handleCallbackStationPos(const QJsonObject& query, const QStringList& parts);
    void handleCallbackStationTanks(const QJsonObject& query, const QStringList& parts);
    void handleCallbackStationDisp(const QJsonObject& query, const QStringList& parts);

    void handleZaglushka(const QJsonObject& message);

    void handleTaskTrackerSelection(const QJsonObject& query, const QStringList& parts);

    void handleReportTrackerSelection(const QJsonObject& callbackQuery, const QStringList& parts);
    void handleCreateReport(const QJsonObject& message);

    void handleRedmineTaskSelectionForReport(const QJsonArray& tasks, qint64 telegramId);

    void handleCallbackReportSelectTask(const QJsonObject& query, const QStringList& parts);
    void handleCallbackReportManualId(const QJsonObject& query, const QStringList& parts);

    void handleReportInput(const QJsonObject& message);

    /**
     * @brief Показує меню звіту (Коментар / Закрити) і фіксує контекст.
     */
    void showReportMenu(qint64 chatId, const QString& taskId, const QString& tracker, bool isEdit = false, qint64 messageId = 0);
    void handleCallbackReportSelectType(const QJsonObject& query, const QStringList& parts);

//    void handleCallbackReportDone(const QJsonObject& query, const QStringList& parts);

    void startSessionTimeout(qint64 telegramId);
    void stopSessionTimeout(qint64 telegramId);
    void resetSession(qint64 telegramId, const QString& reason);

    void handleCallbackReportSearch(const QJsonObject& query, const QStringList& parts);

    void showJiraTaskCard(qint64 chatId, const QJsonObject& issue);

    void handleCallbackReportAction(const QJsonObject& query, const QStringList& parts);

    // Нові обробники для меню закриття/відхилення
    void handleCallbackReportMethod(const QJsonObject& query, const QStringList& parts);
    void handleCallbackReportResult(const QJsonObject& query, const QStringList& parts);
    void handleCallbackReportCancel(const QJsonObject& query, const QStringList& parts);



private:
    enum class UserState {
        None,
        WaitingForStationNumber,
        WaitingForReportInit,          // Очікування вибору трекера (Redmine/Jira)
        WaitingForTaskSelection,       // Очікування вибору задачі зі списку (або ручного введення)
        WaitingForManualTaskId,        // Очікування ручного введення ID задачі
//        WaitingForReportType,          // Очікування вибору дії (Коментар/Закрити)
        WaitingForComment,             // Очікування тексту коментаря
//        WaitingForAttachment,          // Очікування фотозвіту
        ValidatingTask,                // Очікування відповіді API на валідацію ID
        WaitingForAssignment,
        WaitingForJiraTerminalID, // Стан очікування введення номера АЗС для пошуку
        WaitingForTimeSpent,
        WaitingForJiraTaskId,
        WaitingForJiraPhoto
    };
    QMap<qint64, UserState> m_userState; // <telegramId, State>

    QMap<qint64, int> m_userClientContext; // <telegramId, clientId>
    QMap<qint64, QJsonArray> m_userStationCache; // <telegramId, Full_Stations_Array>
    QMap<qint64, QJsonArray> m_userClientCache; // <telegramId, Full_Clients_Array>
    QMap<qint64, QVariantMap> m_reportContext; // <telegramId, { "tracker": "redmine", "taskId": 12345, "reportType": "close" }>

    // Мапи (карти) обробників
    QMap<QString, CallbackHandler> m_clientsHandlers; // "clients:main"
    QMap<QString, CallbackHandler> m_clientHandlers;  // "client:select"
    QMap<QString, CallbackHandler> m_stationsHandlers; // "stations:list", "stations:page", etc.
    QMap<QString, CallbackHandler> m_stationHandlers;  // "station:map", "station:stub"
    QMap<QString, CallbackHandler> m_tasksHandlers;    // МАПА ДЛЯ ОБРОБКИ ЗАПИТІВ ЗАДАЧ (tasks:show)
    QMap<QString, CallbackHandler> m_reportHandlers;
    // QMap<telegramId, QTimer*> - зберігає таймер для кожного активного користувача
    QMap<qint64, QTimer*> m_sessionTimers;
    QMap<qint64, User*> m_users;

    QMap<QString, CommandHandler> m_userCommandHandlers;
    QMap<QString, CommandHandler> m_adminCommandHandlers;
    TelegramClient* m_telegramClient;
    ApiClient& m_apiClient;
    AttachmentManager *m_attachmentManager;
};

#endif // BOT_H
