#ifndef WEBSERVER_H
#define WEBSERVER_H
#include "Oracle/User.h"
#include <QObject>
#include <QHttpServerResponse> // Додаємо, оскільки метод повертає цей тип

class QHttpServer;
class QHttpServerRequest;

class WebServer : public QObject
{
    Q_OBJECT
public:
    explicit WebServer(quint16 port, const QString& botApiKey, QObject *parent = nullptr);
    bool start();

private:
    // Метод для налаштування всіх маршрутів
    void setupRoutes();
    void logRequest(const QHttpServerRequest &request); // Допоміжний метод для логування
    User* authenticateRequest(const QHttpServerRequest &request); // Перевіряє токен із запиту і повертає об'єкт User, якщо токен валідний
    // Замінюємо старий logResponse на нові методи-помічники
    QHttpServerResponse createTextResponse(const QByteArray &body,
                                           QHttpServerResponse::StatusCode statusCode);
    QHttpServerResponse createJsonResponse(const QJsonObject &body,
                                           QHttpServerResponse::StatusCode statusCode);
    QHttpServerResponse createJsonResponse(const QJsonArray &body, QHttpServerResponse::StatusCode statusCode);
    // Маршрут "/"
    QHttpServerResponse handleRootRequest(const QHttpServerRequest &request);
    // маршрут /status
    QHttpServerResponse handleStatusRequest(const QHttpServerRequest &request);
    // маршрут /api/login
    QHttpServerResponse handleLoginRequest(const QHttpServerRequest &request);
    // Маршрут /api/users
    QHttpServerResponse handleGetUsersRequest(const QHttpServerRequest &request);
    // Маршрут /api/users/<arg>
    QHttpServerResponse handleGetUserByIdRequest(const QString &userId, const QHttpServerRequest &request);
    // Маршрут /api/roles
    QHttpServerResponse handleGetRolesRequest(const QHttpServerRequest &request);
    // Маршрут /api/users/<arg>
    QHttpServerResponse handleUpdateUserRequest(const QString &userId, const QHttpServerRequest &request);
    // Маршрут /api/clients
    QHttpServerResponse handleGetClientsRequest(const QHttpServerRequest &request);
    // Маршрут /api/clients", QHttpServerRequest::Method::Post,
    QHttpServerResponse handleCreateClientRequest(const QHttpServerRequest &request);
    // маршрут GET /api/clients/<arg>
    QHttpServerResponse handleGetClientByIdRequest(const QString &clientId, const QHttpServerRequest &request);
    // Маршрут GET /api/ip-gen-methods
    QHttpServerResponse handleGetIpGenMethodsRequest(const QHttpServerRequest &request);
    // POST /api/connections/test
    QHttpServerResponse handleTestConnectionRequest(const QHttpServerRequest &request);
    // /api/clients/<arg>
    QHttpServerResponse handleUpdateClientRequest(const QString &clientId, const QHttpServerRequest &request);
    // Get /api/settings/<arg>
    QHttpServerResponse handleGetSettingsRequest(const QString& appName, const QHttpServerRequest& request);
    // Put /api/settings/<arg>
    QHttpServerResponse handleUpdateSettingsRequest(const QString& appName, const QHttpServerRequest& request);
    // POST /api/clients/<arg>/sync
    QHttpServerResponse handleSyncClientObjectsRequest(const QString& clientId, const QHttpServerRequest& request);
    // Get /api/clients/<arg>/sync-status
    QHttpServerResponse handleGetSyncStatusRequest(const QString &clientId, const QHttpServerRequest &request);
    // Get /api/objects
    QHttpServerResponse handleGetObjectsRequest(const QHttpServerRequest &request);
    // GET /api/regions-list
    QHttpServerResponse handleGetRegionsListRequest(const QHttpServerRequest &request);
    // POST /api/bot/register
    QHttpServerResponse handleBotRegisterRequest(const QHttpServerRequest& request);
    // Get /api/bot/requests
    QHttpServerResponse handleGetBotRequests(const QHttpServerRequest& request);
    // Post /api/bot/reject
    QHttpServerResponse handleRejectBotRequest(const QHttpServerRequest& request);
    // POST /api/bot/approve
    QHttpServerResponse handleApproveBotRequest(const QHttpServerRequest& request);
    // POST /api/bot/link
    QHttpServerResponse handleLinkBotRequest(const QHttpServerRequest& request);
    // GET /api/bot/requests
    QHttpServerResponse handleBotStatusRequest(const QHttpServerRequest& request);
    // GET /api/bot/users
    QHttpServerResponse handleGetBotUsersRequest(const QHttpServerRequest& request);
    // GET /api/bot/clients/<clientId>/stations
    QHttpServerResponse handleGetClientStations(const QString& clientId, const QHttpServerRequest& request);
    // GET /api/bot/clients/<clientId>/station/<terminalNo>
    QHttpServerResponse handleGetStationDetails(const QString& clientId, const QString& terminalNo, const QHttpServerRequest& request);
    // GET /api/export-tasks
    QHttpServerResponse handleGetExportTasksRequest(const QHttpServerRequest &request);
    // --- Export Tasks Management (Керування завданнями експорту) ---
    QHttpServerResponse handleGetAllExportTasksRequest(const QHttpServerRequest &request);
    QHttpServerResponse handleGetExportTaskRequest(const QString &taskId, const QHttpServerRequest &request);
    QHttpServerResponse handleCreateExportTaskRequest(const QHttpServerRequest &request);
    QHttpServerResponse handleUpdateExportTaskRequest(const QString &taskId, const QHttpServerRequest &request);

    // GET /api/dashboard
    QHttpServerResponse handleDashboardRequest(const QHttpServerRequest &request);

    // Новий метод: Отримати РРО (приймає або BotKey, або UserToken)
    QHttpServerResponse handleGetStationPosData(const QString& clientId, const QString& terminalNo, const QHttpServerRequest& request);

    // GET /api/clients/<clientId>/station/<terminalNo>/tanks
    QHttpServerResponse handleGetStationTanks(const QString& clientId, const QString& terminalNo, const QHttpServerRequest& request);

    // GET /api/clients/<clientId>/station/<terminalNo>/dispensers
    QHttpServerResponse handleGetStationDispensers(const QString& clientId, const QString& terminalNo, const QHttpServerRequest& request);

    /**
     * @brief Обробляє запит бота на отримання списку відкритих Redmine задач.
     * Маршрут: GET /api/bot/redmine/tasks
     */
    QHttpServerResponse handleGetRedmineTasks(const QHttpServerRequest& request);

    /**
     * @brief Обробляє запит бота на отримання списку відкритих Jira задач.
     * Маршрут: GET /api/bot/jira/tasks
     */
    QHttpServerResponse handleGetJiraTasks(const QHttpServerRequest& request);

    /**
     * @brief Обробляє запит на деталі задачі Redmine/Jira (для валідації).
     * Маршрут: GET /api/bot/tasks/details?tracker=...&id=...
     */
    QHttpServerResponse handleGetTaskDetails(const QHttpServerRequest& request);

    /**
     * @brief Обробляє запит на призначення задачі на себе.
     * Маршрут: POST /api/bot/tasks/assign
     */
    QHttpServerResponse handleAssignTaskToSelf(const QHttpServerRequest& request);

    /**
     * @brief Обробляє запит на фінальне звітування (коментар, закриття).
     */
    QHttpServerResponse handleReportTask(const QHttpServerRequest& request);

    QHttpServerResponse handleJiraAttach(const QHttpServerRequest &request);

    // POST /api/bot/tasks/comment
    QHttpServerResponse handleTaskComment(const QHttpServerRequest &request);

    /**
     * @brief Обробляє запит на пошук АЗС за номером терміналу.
     * Маршрут: GET /api/stations/search?terminal=XXXX
     */
    QHttpServerResponse handleSearchStations(const QHttpServerRequest& request);


private:
    QHttpServer* m_httpServer;
    quint16 m_port;
    QString m_botApiKey;
};

#endif // WEBSERVER_H
