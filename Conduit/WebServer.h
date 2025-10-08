#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <QObject>
#include <QHttpServerResponse> // Додаємо, оскільки метод повертає цей тип

class QHttpServer;
class QHttpServerRequest;

class WebServer : public QObject
{
    Q_OBJECT
public:
    explicit WebServer(quint16 port, QObject *parent = nullptr);
    bool start();

private:
    // Метод для налаштування всіх маршрутів
    void setupRoutes();
    void logRequest(const QHttpServerRequest &request); // Допоміжний метод для логування
    // Замінюємо старий logResponse на нові методи-помічники
    QHttpServerResponse createTextResponse(const QByteArray &body,
                                           QHttpServerResponse::StatusCode statusCode);
    QHttpServerResponse createJsonResponse(const QJsonObject &body,
                                           QHttpServerResponse::StatusCode statusCode);
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
private:
    QHttpServer* m_httpServer;
    quint16 m_port;
};

#endif // WEBSERVER_H
