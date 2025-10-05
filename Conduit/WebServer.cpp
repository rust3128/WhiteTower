#include "WebServer.h"
#include "Oracle/Logger.h"
#include <QHttpServer>
// QHttpServerResponse вже підключено через WebServer.h

WebServer::WebServer(quint16 port, QObject *parent)
    : QObject{parent}, m_port(port)
{
    m_httpServer = new QHttpServer(this);

    // Налаштувальник маршрутів
    setupRoutes();
}

void WebServer::setupRoutes()
{
    // Це центральне місце для всіх маршрутів нашого сервера.
    // У майбутньому ми будемо додавати нові рядки сюди.

    m_httpServer->route("/", [this](const QHttpServerRequest &request) {
        return handleRootRequest(request);
    });

    // Приклад: ось як ми додамо новий маршрут у майбутньому
    // m_httpServer->route("/api/users", [this](const QHttpServerRequest &request) {
    //     return handleUsersRequest(request);
    // });
}


// Реалізація нашого нового методу
QHttpServerResponse WebServer::handleRootRequest(const QHttpServerRequest &request)
{
    Q_UNUSED(request); // Поки що ми не використовуємо запит

    // 1. Створюємо тіло відповіді як окремий об'єкт
    QByteArray body = "Hello from Conduit Server!";

    // 2. Створюємо відповідь, передаючи ТІЛЬКИ тіло в конструктор
    QHttpServerResponse response(body);

    // 3. Явно встановлюємо заголовок Content-Type
    response.setHeader("Content-Type", "text/plain");

    // 4. Повертаємо повністю налаштований об'єкт
    return response;
}

bool WebServer::start()
{
    const auto port = m_httpServer->listen(QHostAddress::Any, m_port);

    if (!port) {
        logCritical() << QString("Сервер не вдалося запустити на порту %1. Можливо, він зайнятий.").arg(m_port);
        return false;
    }

    logInfo() << QString("Сервер слухає на http://localhost:%1").arg(port);
    return true;
}
