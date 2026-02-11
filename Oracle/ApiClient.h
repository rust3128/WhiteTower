#ifndef APICLIENT_H
#define APICLIENT_H


#include "StationStruct.h"

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QFile>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;
class User;

// Структура для зберігання детальної інформації про помилку API
struct ApiError {
    int httpStatusCode = 0;         // Напр., 404, 500
    QString requestUrl;             // URL, на який йшов запит
    QString errorString;            // Текстовий опис помилки
    QByteArray responseBody;        // Тіло відповіді від сервера (може містити JSON з деталями)
};
Q_DECLARE_METATYPE(ApiError)

ApiError parseReply(QNetworkReply* reply);

class ApiClient : public QObject
{
    Q_OBJECT
public:
    static ApiClient& instance();
    // --- НОВИЙ СЕТТЕР ДЛЯ БОТА ---
    void setBotApiKey(const QString& key);
    // Методи для виклику API
    void login(const QString& username);
    void fetchAllUsers();
    void fetchUserById(int userId);
    void fetchAllRoles();
    void updateUser(int userId, const QJsonObject& userData);
    void fetchAllClients();
    void createClient(const QJsonObject& clientData);
    void fetchClientById(int clientId);
    void fetchAllIpGenMethods();
    void testDbConnection(const QJsonObject& config);
    void updateClient(int clientId, const QJsonObject& clientData);
    void fetchSettings(const QString& appName);
    void updateSettings(const QString& appName, const QVariantMap& settings);
    void syncClientObjects(int clientId);
    void fetchSyncStatus(int clientId);
    void fetchObjects(const QVariantMap& filters = {});
    void fetchRegionsList();
    // метод для встановлення URL:
    void setServerUrl(const QString& url);
    // метод для реєстрації:
    void registerBotUser(const QJsonObject& userData);
    void fetchBotRequests();
    void rejectBotRequest(int requestId);
    void approveBotRequest(int requestId, const QString& login);
    void linkBotRequest(int requestId, int userId);
    void checkBotUserStatus(const QJsonObject& message);

    // --- НОВИЙ МЕТОД ДЛЯ ISENGARD ---
    void fetchBotClients(qint64 telegramId);

    void fetchBotRequestsForAdmin(qint64 telegramId);
    void approveBotRequestForAdmin(qint64 adminTelegramId, int requestId, const QString& login);
    void rejectBotRequestForAdmin(qint64 adminTelegramId, int requestId, const QString& login);
    void fetchBotActiveUsers(qint64 adminTelegramId);

    void fetchStationsForClient(qint64 telegramId, int clientId);
    void fetchStationDetails(qint64 telegramId, int clientId, const QString& terminalNo);
    void fetchExportTasks();


    // --- Методи для управління завданнями експорту (EXPORT_TASKS) ---
    void fetchAllExportTasks();
    void fetchExportTaskById(int taskId);
    void saveExportTask(const QJsonObject& taskData); // Використовується як для створення (ID=-1), так і для оновлення

    // Запит даних для моніторингу
    void fetchDashboardData();

    // Команда на запуск синхронізації
    void syncClient(int clientId);

    // Запит на отримання даних РРО (POS)
    // telegramId = 0 за замовчуванням (для Gandalf)
    void fetchStationPosData(int clientId, int terminalId, qint64 telegramId = 0);

    // Запит на отримання резервуарів
    void fetchStationTanks(int clientId, int terminalId, qint64 telegramId = 0);

    void fetchDispenserConfig(int clientId, int terminalId, qint64 telegramId = 0);

    /**
     * @brief Запит Redmine задач для користувача Telegram.
     * @param telegramId ID користувача Telegram.
     */
    void fetchRedmineTasks(qint64 telegramId);

    /**
     * @brief Запит Redmine задач для користувача Gandalf.
     * @param userId ID користувача Gandalf.
     */
    void fetchRedmineTasks(int userId);

    /**
     * @brief Запит Jira задач для користувача Telegram.
     * @param telegramId ID користувача Telegram.
     */
    void fetchJiraTasks(qint64 telegramId);

    /**
     * @brief Запит деталей будь-якої задачі (для валідації).
     * @param tracker 'redmine' або 'jira'.
     * @param taskId ID або ключ задачі.
     */
    void fetchTaskDetails(const QString& tracker, const QString& taskId, qint64 telegramId);

    /**
     * @brief Призначає задачу на поточного користувача ( Assign-to-Self ).
     * @param tracker 'redmine' або 'jira'.
     * @param taskId ID або ключ задачі.
     */
    void assignTaskToSelf(const QString& tracker, const QString& taskId, qint64 telegramId);

    /**
     * @brief Відправляє фінальний звіт (коментар/закриття) по задачі.
     * @param payload QJsonObject із даними (tracker, taskId, action, comment, attachments).
     * @param telegramId ID користувача для X-Telegram-ID.
     */
    void reportTask(const QJsonObject& payload, qint64 telegramId);

    void fetchJiraTasksByTerminal(qint64 telegramId, int terminalId);

    // path - шлях на диску, taskId - ключ задачі (напр. AZS-46937), chatId - для відповіді
    void uploadAttachmentToJira(const QString &path, const QString &taskId, qint64 chatId);

    void sendTaskComment(const QString& taskId, const QString& tracker, const QString& comment, qint64 telegramId);

    /**
 * @brief Запит на пошук АЗС за номером терміналу
 * @param terminalId Номер терміналу для пошуку
 */
    void searchStation(int terminalId);

signals:
    // Сигнали для логіну
    void loginSuccess(User* user);
    void loginFailed(const ApiError& error);

    // Сигнали для списку користувачів
    void usersFetched(const QJsonArray& users);
    void usersFetchFailed(const ApiError& error);

    // Сигнали для одного користувача
    void userDetailsFetched(const QJsonObject& user);
    void userDetailsFetchFailed(const ApiError& error);

    // Сигнали для ролей
    void rolesFetched(const QJsonArray& roles);
    void rolesFetchFailed(const ApiError& error);

    // Сигнали для оновлення користувача
    void userUpdateSuccess();
    void userUpdateFailed(const ApiError& error);

    // Сигнали для списку клієнтів
    void clientsFetched(const QJsonArray& clients);
    void clientsFetchFailed(const ApiError& error);

    // Сигнали для створення клієнта
    void clientCreateSuccess(const QJsonObject& newClient);
    void clientCreateFailed(const ApiError& error); // <-- ЦЕЙ СИГНАЛ БУЛО ПРОПУЩЕНО

    void clientDetailsFetched(const QJsonObject& client);
    void clientDetailsFetchFailed(const ApiError& error);

    void ipGenMethodsFetched(const QJsonArray& methods);
    void ipGenMethodsFetchFailed(const ApiError& error);

    void connectionTestSuccess(const QString& message);
    void connectionTestFailed(const ApiError& error);

    void clientUpdateSuccess();
    void clientUpdateFailed(const ApiError& error);

    // Додайте в signals секцію
    void settingsFetched(const QVariantMap& settings);
    void settingsFetchFailed(const ApiError& error);

    void settingsUpdateSuccess();
    void settingsUpdateFailed(const ApiError& error);

    void syncRequestAcknowledged(int clientId, bool success, const ApiError& details);

    void syncStatusFetched(int clientId, const QJsonObject& status);
    void syncStatusFetchFailed(int clientId, const ApiError& error);

    void objectsFetched(const QJsonArray& objects);
    void objectsFetchFailed(const ApiError& error);

    void regionsListFetched(const QStringList& regions);
    void regionsListFetchFailed(const ApiError& error);

    void botUserRegistered(const QJsonObject& result, qint64 telegramId);
    void botUserRegistrationFailed(const ApiError& error, qint64 telegramId);

    void botRequestsFetched(const QJsonArray& requests);
    void botRequestsFetchFailed(const ApiError& error);

    void botRequestRejected(int requestId);
    void botRequestRejectFailed(const ApiError& error);

    void botRequestApproved(int requestId);
    void botRequestApproveFailed(const ApiError& error);

    void botRequestLinked(int requestId);
    void botRequestLinkFailed(const ApiError& error);

    void botUserStatusReceived(const QJsonObject& status, const QJsonObject& message);
    void botUserStatusCheckFailed(const ApiError& error);

    // --- НОВІ СИГНАЛИ ДЛЯ ISENGARD ---
    void botClientsFetched(const QJsonArray& clients, qint64 telegramId);
    void botClientsFetchFailed(const ApiError& error, qint64 telegramId);

    void botAdminRequestsFetched(const QJsonArray& requests, qint64 telegramId);
    void botAdminRequestsFetchFailed(const ApiError& error, qint64 telegramId);

    void botAdminRequestApproved(int requestId, qint64 adminTelegramId);
    void botAdminRequestApproveFailed(const ApiError& error, qint64 adminTelegramId);
    void botAdminRequestRejected(int requestId, qint64 adminTelegramId);
    void botAdminRequestRejectFailed(const ApiError& error, qint64 adminTelegramId);

    void botActiveUsersFetched(const QJsonArray& users, qint64 telegramId);
    void botActiveUsersFetchFailed(const ApiError& error, qint64 telegramId);

    void stationsFetched(const QJsonArray& stations, qint64 telegramId, int clientId);
    void stationsFetchFailed(const ApiError& error, qint64 telegramId, int clientId);
    void stationDetailsFetched(const QJsonObject& station, qint64 telegramId, int clientId);
    void stationDetailsFetchFailed(const ApiError& error, qint64 telegramId, int clientId);

    // --- Сигнали для Export Tasks ---
    void exportTasksFetched(const QJsonArray& tasks);
    void exportTasksFetchFailed(const ApiError& error);

    void exportTaskFetched(const QJsonObject& task);
    void exportTaskFetchFailed(const ApiError& error);

    // Сигнал для успішного збереження (створення/оновлення)
    void exportTaskSaved(int taskId);
    void exportTaskSaveFailed(const ApiError& error);

    // Сигнали для дашборду
    void dashboardDataFetched(const QJsonArray& data);
    void dashboardDataFetchFailed(const ApiError& error);

    // Сигнали результату запуску
    void clientSyncRequestFinished(int clientId, bool success, QString message);

    //  Сигнали для РРО
    void stationPosDataReceived(const QJsonArray& data, int clientId, int terminalId, qint64 telegramId);
    void stationPosDataFailed(const ApiError& error, qint64 telegramId);

    // Сигнали для резервуарів
    void stationTanksReceived(const QJsonArray& data, int clientId, int terminalId, qint64 telegramId);
    void stationTanksFailed(const ApiError& error, qint64 telegramId);

    void dispenserConfigReceived(const QJsonArray& config, int clientId, int terminalId, qint64 telegramId);
    void dispenserConfigFailed(const ApiError& error, qint64 telegramId);

    /**
     * @brief Сигнал успішного отримання Redmine задач.
     * @param tasks Масив JSON із завданнями.
     * @param telegramId ID користувача Telegram (якщо запит від бота).
     * @param userId ID користувача Gandalf (якщо запит від Gandalf).
     */
    void redmineTasksFetched(const QJsonArray& tasks, qint64 telegramId = 0, int userId = 0);
    void redmineTasksFetchFailed(const ApiError& error, qint64 telegramId = 0, int userId = 0);

    /**
     * @brief Сигнал успішного отримання Jira задач.
     */
    void jiraTasksFetched(const QJsonArray& tasks, qint64 telegramId = 0);
    void jiraTasksFetchFailed(const ApiError& error, qint64 telegramId = 0);


    void taskDetailsFetched(const QJsonObject& taskDetails, qint64 telegramId);
    void taskDetailsFetchFailed(const ApiError& error, qint64 telegramId);

    void assignTaskSuccess(const QJsonObject& response, qint64 telegramId);
    void assignTaskFailed(const ApiError& error, qint64 telegramId);

    void reportTaskSuccess(const QJsonObject& response, qint64 telegramId);
    void reportTaskFailed(const ApiError& error, qint64 telegramId);

    void jiraAttachmentSuccess(qint64 telegramId, const QString &taskId);
    void jiraAttachmentFailed(const ApiError &error, qint64 telegramId);

    void taskCommentSuccess(qint64 telegramId, const QString& taskId, const QString& tracker);
    void taskCommentFailed(const ApiError& error, qint64 telegramId);

    /**
 * @brief Сигнал, що повертає список знайдених станцій
 * @param stations Список об'єктів StationStruct
 */
    void stationSearchFinished(const QList<StationStruct>& stations);


private slots:
    void onLoginReplyFinished();
    void onUsersReplyFinished();
    void onUserDetailsReplyFinished();
    void onRolesReplyFinished();
    void onUserUpdateReplyFinished();
    void onClientsReplyFinished();
    void onCreateClientReplyFinished();
    void onClientDetailsReplyFinished();
    void onIpGenMethodsReplyFinished();
    void onConnectionTestReplyFinished();
    void onClientUpdateReplyFinished();
    void onSettingsReplyFinished();
    void onSettingsUpdateReplyFinished();
    void onSyncReplyFinished();
    void onSyncStatusReplyFinished();
    void onObjectsReplyFinished();
    void onRegionsListReplyFinished();
    void onBotRegisterReplyFinished();
    void onBotRequestsReplyFinished();
    void onBotRequestRejectReplyFinished();
    void onBotRequestApproveReplyFinished();
    void onBotRequestLinkReplyFinished();
    void onBotUserStatusReplyFinished();
    // --- НОВИЙ СЛОТ ДЛЯ ISENGARD ---
    void onBotClientsReplyFinished();
    void onBotAdminRequestsReplyFinished();

    void onBotAdminApproveReplyFinished();
    void onBotAdminRejectReplyFinished();

    void onBotActiveUsersReplyFinished();

    void onStationsReplyFinished();
    void onStationDetailsReplyFinished();

    void onExportTasksReplyFinished();

    // --- Слоти для обробки відповідей Export Tasks ---
    void onAllExportTasksReplyFinished();
    void onExportTaskDetailsReplyFinished();
    void onExportTaskSaveReplyFinished();

    void onDashboardDataReplyFinished();

    void onStationPosDataReplyFinished();

    void onStationTanksReplyFinished();

//    void onDispenserConfigReplyFinished();
    void onStantionDispenserReplyFinished();

    void onRedmineTasksReplyFinished();

    // СЛОТ ДЛЯ JIRA
    void onJiraTasksReplyFinished();

    void onTaskDetailsReplyFinished();
    void onAssignTaskReplyFinished();

    void onReportTaskReplyFinished();
private:
    ApiClient(QObject* parent = nullptr);
    ~ApiClient() = default;
    ApiClient(const ApiClient&) = delete;
    ApiClient& operator=(const ApiClient&) = delete;

    // --- НОВІ ПРИВАТНІ МЕТОДИ ---
    QNetworkRequest createBotRequest(const QUrl &url, qint64 telegramId);
    QNetworkRequest createBotRequest(const QUrl &url);

    QNetworkRequest createAuthenticatedRequest(const QUrl &url);


    QNetworkAccessManager* m_networkManager;
    QString m_serverUrl;
    QString m_authToken;
    QString m_botApiKey;

};

#endif // APICLIENT_H
