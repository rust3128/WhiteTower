#include "WebServer.h"
#include "Oracle/Logger.h"
#include "Oracle/DbManager.h" // Підключаємо для перевірки статусу БД
#include "version.h"
#include "Oracle/SessionManager.h"
#include "Oracle/User.h"

#include <QHttpServer>
#include <QHttpServerRequest>
#include <QJsonObject>        // Для створення JSON-відповіді
#include <QJsonDocument>
#include <QCoreApplication>   // Для отримання версії
#include <QMetaEnum>

WebServer::WebServer(quint16 port, QObject *parent)
    : QObject{parent}, m_port(port)
{
    m_httpServer = new QHttpServer(this);
    setupRoutes();
}

void WebServer::setupRoutes()
{
    m_httpServer->route("/", [this](const QHttpServerRequest &request) {
        return handleRootRequest(request);
    });

    // Додаємо новий маршрут /status
    m_httpServer->route("/status", [this](const QHttpServerRequest &request) {
        return handleStatusRequest(request);
    });

    m_httpServer->route("/api/login", QHttpServerRequest::Method::Post, [this](const QHttpServerRequest &request) {
        return handleLoginRequest(request);
    });
}

// Допоміжний метод для логування запиту
void WebServer::logRequest(const QHttpServerRequest &request)
{
    const auto metaEnum = QMetaEnum::fromType<QHttpServerRequest::Method>();

    // === ВИПРАВЛЕНО ТУТ: Додаємо явне перетворення типу на int ===
    const QString methodName = metaEnum.valueToKey(static_cast<int>(request.method()));

    logInfo() << QString("Request: %1 %2 from %3")
                     .arg(methodName)
                     .arg(request.url().toString())
                     .arg(request.remoteAddress().toString());
}

QHttpServerResponse WebServer::createTextResponse(const QByteArray &body, QHttpServerResponse::StatusCode statusCode)
{
    logInfo() << QString("Response: status %1, type: text/plain, body: \"%2\"")
    .arg(static_cast<int>(statusCode))
        .arg(QString::fromUtf8(body.left(200)));

    // 1. Створюємо відповідь, передаючи тіло і СТАТУС-КОД в конструктор.
    QHttpServerResponse response(body, statusCode);

    // 2. Встановлюємо заголовок (ця операція дозволена).
    response.setHeader("Content-Type", "text/plain");

    return response;
}

// Замініть і цю функцію
QHttpServerResponse WebServer::createJsonResponse(const QJsonObject &body, QHttpServerResponse::StatusCode statusCode)
{
    QByteArray bodyJson = QJsonDocument(body).toJson(QJsonDocument::Compact);

    // === ВИПРАВЛЕНО ТУТ: Додаємо .noquote() до виклику логера ===
    // Цей метод каже QDebug виводити наступний рядок "як є", без лапок.
    logInfo().noquote() << QString("Response: status %1, type: application/json, body: %2")
                               .arg(static_cast<int>(statusCode))
                               .arg(QString::fromUtf8(bodyJson));

    return QHttpServerResponse(body, statusCode);
}

// === ОНОВЛЕНІ ОБРОБНИКИ ===

QHttpServerResponse WebServer::handleRootRequest(const QHttpServerRequest &request)
{
    logRequest(request);

    // Тепер просто викликаємо наш новий метод-помічник
    return createTextResponse("Hello from Conduit Server!", QHttpServerResponse::StatusCode::Ok);
}

/**
 * @api {get} /status Отримати статус сервера
 * @apiName GetStatus
 * @apiGroup Server
 * @apiVersion 1.0.0
 *
 * @apiDescription Цей маршрут надає базову інформацію про стан сервера,
 * версію додатку та статус підключення до бази даних.
 *
 * @apiSuccess {String} application_version Повна версія додатку (напр., "1.0.12").
 * @apiSuccess {String} build_date Дата та час збірки цього екземпляра сервера.
 * @apiSuccess {String} database_status Статус підключення до бази даних ("connected" або "disconnected").
 * @apiSuccess {String} server_time Поточний час сервера в форматі ISO 8601.
 *
 * @apiSuccessExample {json} Приклад успішної відповіді:
 * HTTP/1.1 200 OK
 * {
 * "application_version": "1.0.12",
 * "build_date": "06.10.2025 10:38:00",
 * "database_status": "connected",
 * "server_time": "2025-10-06T10:38:15"
 * }
 */

QHttpServerResponse WebServer::handleStatusRequest(const QHttpServerRequest &request)
{
    logRequest(request);

    QJsonObject json;
    json["application_version"] = PROJECT_VERSION_STR; // Використовуємо універсальний макрос
    json["build_date"] = PROJECT_BUILD_DATETIME;

    bool isDbConnected = DbManager::instance().isConnected();
    json["database_status"] = isDbConnected ? "connected" : "disconnected";

    // І тут так само
    return createJsonResponse(json, QHttpServerResponse::StatusCode::Ok);
}

bool WebServer::start()
{
    // ... (цей метод залишається без змін) ...
    const auto port = m_httpServer->listen(QHostAddress::Any, m_port);
    if (!port) {
        logCritical() << QString("Сервер не вдалося запустити на порту %1. Можливо, він зайнятий.").arg(m_port);
        return false;
    }
    logInfo() << QString("Сервер слухає на http://localhost:%1").arg(port);
    return true;
}


QHttpServerResponse WebServer::handleLoginRequest(const QHttpServerRequest &request)
{
    logRequest(request);

    // 1. Читаємо тіло запиту і парсимо JSON
    const QByteArray body = request.body();
    QJsonDocument doc = QJsonDocument::fromJson(body);

    if (doc.isNull() || !doc.isObject() || !doc.object().contains("login")) {
        QJsonObject error{{"error", "Invalid JSON or missing 'login' field"}};
        return createJsonResponse(error, QHttpServerResponse::StatusCode::BadRequest); // 400 Bad Request
    }

    // 2. Викликаємо SessionManager з логіном від клієнта
    QString login = doc.object().value("login").toString();
    const User* user = SessionManager::instance().login(login);

    // 3. Обробляємо результат
    if (user) {
        // Успіх: повертаємо повний профіль користувача
        return createJsonResponse(user->toJson(), QHttpServerResponse::StatusCode::Ok); // 200 OK
    } else {
        // Помилка: користувач заблокований або інша проблема
        QJsonObject error{{"error", "User identification failed or user is inactive"}};
        return createJsonResponse(error, QHttpServerResponse::StatusCode::Forbidden); // 403 Forbidden
    }
}
