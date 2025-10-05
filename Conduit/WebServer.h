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
    // Маршрут "/"
    QHttpServerResponse handleRootRequest(const QHttpServerRequest &request);
private:
    QHttpServer* m_httpServer;
    quint16 m_port;
};

#endif // WEBSERVER_H
