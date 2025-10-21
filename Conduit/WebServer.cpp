#include "WebServer.h"
#include "Oracle/Logger.h"
#include "Oracle/DbManager.h" // Підключаємо для перевірки статусу БД
#include "version.h"
#include "Oracle/SessionManager.h"


#include <QHttpServer>
#include <QHttpServerRequest>
#include <QJsonObject>        // Для створення JSON-відповіді
#include <QJsonDocument>
#include <QCoreApplication>   // Для отримання версії
#include <QMetaEnum>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QHttpServerResponse>


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

    m_httpServer->route("/api/users", QHttpServerRequest::Method::Get, [this](const QHttpServerRequest &request) {
        return handleGetUsersRequest(request);
    });

    // <arg> - це плейсхолдер для ID користувача
    m_httpServer->route("/api/users/<arg>", QHttpServerRequest::Method::Get,
                        [this](const QString &userId, const QHttpServerRequest &request) {
                            return handleGetUserByIdRequest(userId, request);
                        });

    m_httpServer->route("/api/roles", QHttpServerRequest::Method::Get, [this](const QHttpServerRequest &request) {
        return handleGetRolesRequest(request);
    });

    m_httpServer->route("/api/users/<arg>", QHttpServerRequest::Method::Put,
                        [this](const QString &userId, const QHttpServerRequest &request) {
                            return handleUpdateUserRequest(userId, request);
                        });

    m_httpServer->route("/api/clients", QHttpServerRequest::Method::Get, [this](const QHttpServerRequest &request) {
        return handleGetClientsRequest(request);
    });

    m_httpServer->route("/api/clients", QHttpServerRequest::Method::Post, [this](const QHttpServerRequest &request) {
        return handleCreateClientRequest(request);
    });

    m_httpServer->route("/api/clients/<arg>", QHttpServerRequest::Method::Get,
                        [this](const QString &clientId, const QHttpServerRequest &request) {
                            return handleGetClientByIdRequest(clientId, request);
                        });
    m_httpServer->route("/api/ip-gen-methods", QHttpServerRequest::Method::Get, [this](const QHttpServerRequest &request) {
        return handleGetIpGenMethodsRequest(request);
    });

    // Додайте в setupRoutes()
    m_httpServer->route("/api/connections/test", QHttpServerRequest::Method::Post, [this](const QHttpServerRequest &request) {
        return handleTestConnectionRequest(request);
    });

    m_httpServer->route("/api/clients/<arg>", QHttpServerRequest::Method::Put,
                        [this](const QString &clientId, const QHttpServerRequest &request) {
                            return handleUpdateClientRequest(clientId, request);
                        });
    m_httpServer->route("/api/settings/<arg>", QHttpServerRequest::Method::Get,
                        [this](const QString& appName, const QHttpServerRequest& request){
                            return handleGetSettingsRequest(appName, request);
                        });

    // === ДОДАЙТЕ ВІДСУТНІЙ МАРШРУТ ТУТ ===
    m_httpServer->route("/api/settings/<arg>", QHttpServerRequest::Method::Put,
                        [this](const QString& appName, const QHttpServerRequest& request){
                            return handleUpdateSettingsRequest(appName, request);
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

    // === ДОДАНО ТУТ: Автоматичне логування помилок ===
    // Якщо статус-код 400 або вище (Bad Request, Unauthorized, Not Found, Internal Server Error і т.д.),
    // то записуємо тіло помилки в лог сервера.
    if (statusCode >= QHttpServerResponse::StatusCode::BadRequest) {
        logCritical() << "Server Response Error:" << static_cast<int>(statusCode)
        << QJsonDocument(body).toJson(QJsonDocument::Compact);
    }
    // =======================================================
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

/**
 * @api {post} /api/login Авторизація користувача
 * @apiName LoginUser
 * @apiGroup User
 * @apiVersion 1.0.0
 *
 * @apiDescription Цей маршрут дозволяє користувачу увійти в систему, використовуючи свій логін.
 * Успішна авторизація повертає повний профіль користувача, інакше - помилку.
 *
 * @apiBody {String} login Унікальний логін користувача. Передається в тілі запиту у форматі JSON.
 *
 * @apiParamExample {json} Приклад запиту:
 * HTTP/1.1 200 OK
 * {
 * "login": "john_doe"
 * }
 * * @apiSuccess {Object} user Об'єкт, що містить повну інформацію про користувача.
 * @apiSuccess {String} user.id Унікальний ідентифікатор користувача.
 * @apiSuccess {String} user.login Логін користувача.
 * @apiSuccess {String} user.name Повне ім'я користувача.
 * @apiSuccess {String} user.role Роль користувача (напр., "admin", "user").
 * @apiSuccess {Boolean} user.isActive Статус активності користувача.
 *
 * @apiSuccessExample {json} Приклад успішної відповіді:
 * HTTP/1.1 200 OK
 * {
 * "id": "abc-123",
 * "login": "john_doe",
 * "name": "John Doe",
 * "role": "user",
 * "isActive": true
 * }
 *
 * @apiError (400 Bad Request) InvalidInput Некоректний JSON або відсутнє поле 'login'.
 * @apiError (403 Forbidden) UserInactive Ідентифікація користувача не вдалася, або користувач заблокований.
 *
 * @apiErrorExample {json} Приклад помилки 400 Bad Request:
 * HTTP/1.1 400 Bad Request
 * {
 * "error": "Invalid JSON or missing 'login' field"
 * }
 *
 * @apiErrorExample {json} Приклад помилки 403 Forbidden:
 * HTTP/1.1 403 Forbidden
 * {
 * "error": "User identification failed or user is inactive"
 * }
 */
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

    // Отримуємо пару <User*, QString>
    auto loginResult = SessionManager::instance().login(login);
    const User* user = loginResult.first;
    const QString token = loginResult.second;

    if (user && !token.isEmpty()) {
        // Успіх: створюємо складний JSON
        QJsonObject responseJson;
        responseJson["user"] = user->toJson(); // Вкладаємо об'єкт юзера
        responseJson["token"] = token;         // Додаємо токен
        return createJsonResponse(responseJson, QHttpServerResponse::StatusCode::Ok);
    } else {
        // Помилка: користувач заблокований або інша проблема
        QJsonObject error{{"error", "User identification failed or user is inactive"}};
        return createJsonResponse(error, QHttpServerResponse::StatusCode::Forbidden); // 403 Forbidden
    }
}

/**
 * @api {get} /api/users Отримати список користувачів
 * @apiName GetUsers
 * @apiGroup User
 * @apiVersion 1.0.0
 *
 * @apiDescription Цей маршрут повертає список усіх зареєстрованих користувачів.
 * Відповідь містить масив об'єктів, кожен з яких представляє повний профіль користувача.
 *
 * @apiSuccess {Object[]} users Масив об'єктів, що містять інформацію про користувачів.
 * @apiSuccess {String} users.id Унікальний ідентифікатор користувача.
 * @apiSuccess {String} users.login Логін користувача.
 * @apiSuccess {String} users.name Повне ім'я користувача.
 * @apiSuccess {String} users.role Роль користувача (напр., "admin", "user").
 * @apiSuccess {Boolean} users.isActive Статус активності користувача.
 *
 * @apiSuccessExample {json} Приклад успішної відповіді:
 * HTTP/1.1 200 OK
 * [
 * {
 * "id": "abc-123",
 * "login": "john_doe",
 * "name": "John Doe",
 * "role": "user",
 * "isActive": true
 * },
 * {
 * "id": "xyz-456",
 * "login": "jane_doe",
 * "name": "Jane Doe",
 * "role": "admin",
 * "isActive": true
 * }
 * ]
 *
 * @apiError (500 Internal Server Error) ServerError Внутрішня помилка сервера. Може виникнути, якщо не вдалося підключитися до бази даних або виникла інша непередбачувана помилка.
 *
 * @apiErrorExample {json} Приклад помилки:
 * HTTP/1.1 500 Internal Server Error
 * {
 * "error": "Internal Server Error"
 * }
 */
QHttpServerResponse WebServer::handleGetUsersRequest(const QHttpServerRequest &request)
{
    logRequest(request);

    // === БЛОК АВТЕНТИФІКАЦІЇ ===
    User* user = authenticateRequest(request);
    if (!user) {
        // Якщо автентифікація не пройдена, повертаємо помилку 401 Unauthorized
        return createJsonResponse({{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }
    // Тепер ми знаємо, хто робить запит, і можемо перевіряти його ролі
    logDebug() << "Request authenticated for user:" << user->login();
    // === КІНЕЦЬ БЛОКУ ===

    // Якщо ми тут, значить, токен валідний. Далі можна додати перевірку ролей.
    // if (!user->isAdmin()) { return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden); }

    QList<User*> users = DbManager::instance().loadAllUsers();
    QJsonArray jsonArray;
    for (User* user : users) {
        jsonArray.append(user->toJson());
    }

    // Важливо: не забудьте очистити пам'ять після того, як скопіювали дані
    qDeleteAll(users);
    delete user;

    return QHttpServerResponse(jsonArray); // QHttpServer сам зробить правильну JSON-відповідь
}

/**
 * @api {get} /api/users/:userId Отримати інформацію про користувача за ID
 * @apiName GetUserById
 * @apiGroup User
 * @apiVersion 1.0.0
 *
 * @apiDescription Цей маршрут дозволяє отримати повний профіль користувача, використовуючи його унікальний ідентифікатор.
 *
 * @apiParam {Number} userId Унікальний ідентифікатор користувача. Це число, що передається в URL.
 *
 * @apiSuccess {Object} user Об'єкт, що містить повну інформацію про користувача.
 * @apiSuccess {String} user.id Унікальний ідентифікатор користувача.
 * @apiSuccess {String} user.login Логін користувача.
 * @apiSuccess {String} user.name Повне ім'я користувача.
 * @apiSuccess {String} user.role Роль користувача (напр., "admin", "user").
 * @apiSuccess {Boolean} user.isActive Статус активності користувача.
 *
 * @apiSuccessExample {json} Приклад успішної відповіді:
 * HTTP/1.1 200 OK
 * {
 * "id": "123",
 * "login": "john_doe",
 * "name": "John Doe",
 * "role": "user",
 * "isActive": true
 * }
 *
 * @apiError (400 Bad Request) InvalidInput ID користувача має некоректний формат.
 * @apiError (404 Not Found) UserNotFound Користувача з вказаним ID не знайдено.
 *
 * @apiErrorExample {json} Приклад помилки 400 Bad Request:
 * HTTP/1.1 400 Bad Request
 * {
 * "error": "Invalid user ID format"
 * }
 *
 * @apiErrorExample {json} Приклад помилки 404 Not Found:
 * HTTP/1.1 404 Not Found
 * {
 * "error": "User not found"
 * }
 */
QHttpServerResponse WebServer::handleGetUserByIdRequest(const QString &userId, const QHttpServerRequest &request)
{
    logRequest(request);

    // --- 1. БЛОК АВТЕНТИФІКАЦІЇ ---
    User* requestingUser = authenticateRequest(request);
    if (!requestingUser) {
        return createJsonResponse({{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }
    logDebug() << "Request authenticated for user:" << requestingUser->login();


    // --- 2. ПЕРЕВІРКА ВХІДНИХ ДАНИХ ---
    bool ok;
    int targetUserId = userId.toInt(&ok);
    if (!ok) {
        delete requestingUser; // Не забуваємо звільнити пам'ять
        return createJsonResponse({{"error", "Invalid user ID format"}}, QHttpServerResponse::StatusCode::BadRequest);
    }


    // --- 3. БЛОК АВТОРИЗАЦІЇ ---
    if (!requestingUser->hasRole("Адміністратор") &&
        !requestingUser->hasRole("Менеджер") &&
        requestingUser->id() != targetUserId)
    {
        logWarning() << "ACCESS DENIED: User" << requestingUser->login()
        << "tried to access profile of user ID" << targetUserId;
        delete requestingUser; // Звільняємо пам'ять
        return createJsonResponse({{"error", "Forbidden"}}, QHttpServerResponse::StatusCode::Forbidden);
    }


    // --- 4. ОСНОВНА ЛОГІКА ---
    User* targetUser = DbManager::instance().loadUser(targetUserId);

    // === ВИПРАВЛЕНО ТУТ: Створюємо і повертаємо відповідь в одному місці ===
    if (targetUser) {
        QJsonObject json = targetUser->toJson();

        // Очищуємо пам'ять перед виходом
        delete requestingUser;
        delete targetUser;

        return createJsonResponse(json, QHttpServerResponse::StatusCode::Ok);
    } else {
        // Очищуємо пам'ять перед виходом
        delete requestingUser;

        return createJsonResponse({{"error", "User not found"}}, QHttpServerResponse::StatusCode::NotFound);
    }
}

/**
 * @api {get} /api/roles Отримати список ролей
 * @apiName GetRoles
 * @apiGroup User
 * @apiVersion 1.0.0
 *
 * @apiDescription Цей маршрут повертає список усіх доступних ролей користувачів.
 * Відповідь містить масив об'єктів, кожен з яких представляє одну роль.
 *
 * @apiSuccess {Object[]} roles Масив об'єктів, що містять інформацію про ролі.
 * @apiSuccess {Number} roles.id Унікальний ідентифікатор ролі.
 * @apiSuccess {String} roles.name Назва ролі (напр., "admin", "user", "guest").
 *
 * @apiSuccessExample {json} Приклад успішної відповіді:
 * HTTP/1.1 200 OK
 * [
 * {
 * "id": 1,
 * "name": "admin"
 * },
 * {
 * "id": 2,
 * "name": "user"
 * },
 * {
 * "id": 3,
 * "name": "guest"
 * }
 * ]
 *
 * @apiError (500 Internal Server Error) ServerError Внутрішня помилка сервера. Може виникнути, якщо не вдалося підключитися до бази даних.
 *
 * @apiErrorExample {json} Приклад помилки:
 * HTTP/1.1 500 Internal Server Error
 * {
 * "error": "Internal Server Error"
 * }
 */
QHttpServerResponse WebServer::handleGetRolesRequest(const QHttpServerRequest &request)
{
    logRequest(request);

    // 1. Отримуємо дані з БД
    QList<QVariantMap> rolesData = DbManager::instance().loadAllRoles();

    // 2. Конвертуємо дані в JSON-масив
    QJsonArray jsonArray;
    for (const QVariantMap &roleMap : rolesData) {
        jsonArray.append(QJsonObject::fromVariantMap(roleMap));
    }

    // 3. Створюємо та логуємо відповідь
    QByteArray bodyJson = QJsonDocument(jsonArray).toJson(QJsonDocument::Compact);
    logInfo().noquote() << QString("Response: status 200, type: application/json, body: %1")
                               .arg(QString::fromUtf8(bodyJson));

    return QHttpServerResponse(jsonArray);
}

/**
 * @api {put} /api/users/:userId Оновити інформацію про користувача
 * @apiName UpdateUser
 * @apiGroup User
 * @apiVersion 1.0.0
 *
 * @apiDescription Цей маршрут оновлює дані існуючого користувача за його унікальним ідентифікатором.
 * Передаються лише ті поля, які потрібно змінити.
 *
 * @apiParam {Number} userId Унікальний ідентифікатор користувача. Передається в URL.
 *
 * @apiBody {String} [login] Новий логін користувача (необов'язково).
 * @apiBody {String} [name] Нове повне ім'я користувача (необов'язково).
 * @apiBody {String} [role] Нова роль користувача (напр., "admin", "user") (необов'язково).
 * @apiBody {Boolean} [isActive] Новий статус активності користувача (необов'язково).
 *
 * @apiParamExample {json} Приклад тіла запиту (зміна тільки імені):
 * HTTP/1.1 200 OK
 * {
 * "name": "John D. Smith"
 * }
 *
 * @apiSuccess (200 OK) Success Оновлення пройшло успішно. Успішна відповідь не містить тіла.
 *
 * @apiSuccessExample {json} Приклад успішної відповіді:
 * HTTP/1.1 200 OK
 * // Порожнє тіло
 *
 * @apiError (400 Bad Request) InvalidIDFormat ID користувача має некоректний формат.
 * @apiError (400 Bad Request) InvalidJSON Некоректний формат JSON у тілі запиту.
 * @apiError (500 Internal Server Error) DatabaseError Помилка оновлення даних у базі даних.
 *
 * @apiErrorExample {json} Приклад помилки 400 Bad Request (Invalid ID):
 * HTTP/1.1 400 Bad Request
 * {
 * "error": "Invalid user ID format"
 * }
 *
 * @apiErrorExample {json} Приклад помилки 500 Internal Server Error:
 * HTTP/1.1 500 Internal Server Error
 * {
 * "error": "Failed to update user in database"
 * }
 */
QHttpServerResponse WebServer::handleUpdateUserRequest(const QString &userId, const QHttpServerRequest &request)
{
    logRequest(request);

    // --- 1. АВТЕНТИФІКАЦІЯ ---
    // Перевіряємо токен і дізнаємось, ХТО робить запит.
    User* requestingUser = authenticateRequest(request);
    if (!requestingUser) {
        return createJsonResponse({{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }
    logDebug() << "Request authenticated for user:" << requestingUser->login();


    // --- 2. ПЕРЕВІРКА ВХІДНИХ ДАНИХ ---
    bool ok;
    int targetUserId = userId.toInt(&ok);
    if (!ok) {
        delete requestingUser;
        return createJsonResponse({{"error", "Invalid user ID format"}}, QHttpServerResponse::StatusCode::BadRequest);
    }

    QJsonDocument doc = QJsonDocument::fromJson(request.body());
    QJsonObject userData = doc.object();
    if (!doc.isObject()) {
        delete requestingUser;
        return createJsonResponse({{"error", "Invalid JSON body"}}, QHttpServerResponse::StatusCode::BadRequest);
    }


    // --- 3. АВТОРИЗАЦІЯ (ПЕРЕВІРКА ПРАВ) ---
    // 3.1. Перевірка права на редагування цього профілю
    if (!requestingUser->hasRole("Адміністратор") &&
        !requestingUser->hasRole("Менеджер") &&
        requestingUser->id() != targetUserId)
    {
        logWarning() << "ACCESS DENIED: User" << requestingUser->login()
        << "tried to UPDATE profile of user ID" << targetUserId;
        delete requestingUser;
        return createJsonResponse({{"error", "Forbidden: You cannot edit this user"}},
                                  QHttpServerResponse::StatusCode::Forbidden);
    }

    // 3.2. Перевірка права на зміну ролей (тільки для адміна)
    if (userData.contains("roles") && !requestingUser->hasRole("Адміністратор")) {
        logWarning() << "ACCESS DENIED: Non-admin user" << requestingUser->login() << "tried to change roles.";
        delete requestingUser;
        return createJsonResponse({{"error", "Forbidden: Only administrators can change roles"}},
                                  QHttpServerResponse::StatusCode::Forbidden);
    }


    // --- 4. ОСНОВНА ЛОГІКА ---
    // Якщо всі перевірки пройдено, оновлюємо дані в БД
    if (DbManager::instance().updateUser(targetUserId, userData)) {
        delete requestingUser;
        return QHttpServerResponse(QHttpServerResponse::StatusCode::Ok); // Успіх
    } else {
        delete requestingUser;
        return createJsonResponse({{"error", "Failed to update user in database"}},
                                  QHttpServerResponse::StatusCode::InternalServerError);
    }
}

User* WebServer::authenticateRequest(const QHttpServerRequest &request)
{
    // 1. Отримуємо список всіх заголовків
    const auto &headers = request.headers();
    QByteArray authHeader;

    // 2. Проходимо по списку, щоб знайти заголовок "Authorization"
    // Пошук нечутливий до регістру, як вимагає стандарт HTTP.
    for (const auto &headerPair : headers) {
        if (headerPair.first.compare("Authorization", Qt::CaseInsensitive) == 0) {
            authHeader = headerPair.second;
            break; // Знайшли, виходимо з циклу
        }
    }

    // 3. Решта логіки залишається без змін
    if (!authHeader.startsWith("Bearer ")) {
        return nullptr;
    }

    QByteArray token = authHeader.mid(7);
    QByteArray tokenHash = QCryptographicHash::hash(token, QCryptographicHash::Sha256).toHex();

    int userId = DbManager::instance().findUserIdByToken(tokenHash);
    if (userId > 0) {
        return DbManager::instance().loadUser(userId);
    }

    return nullptr;
}


QHttpServerResponse WebServer::handleGetClientsRequest(const QHttpServerRequest &request)
{
    // Поки що робимо цей маршрут публічним, потім додамо автентифікацію
    logRequest(request);

    QList<QVariantMap> clientsData = DbManager::instance().loadAllClients();
    QJsonArray jsonArray;
    for (const QVariantMap &clientMap : clientsData) {
        jsonArray.append(QJsonObject::fromVariantMap(clientMap));
    }

    return QHttpServerResponse(jsonArray);
}

QHttpServerResponse WebServer::handleCreateClientRequest(const QHttpServerRequest &request)
{
    // Автентифікуємо запит
    User* user = authenticateRequest(request);
    if (!user) {
        return createJsonResponse({{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }
    // Тут можна додати перевірку, чи має користувач право створювати клієнтів, напр. if (!user->isAdmin()) ...
    delete user; // Користувач автентифікований, для простої перевірки цього достатньо

    logRequest(request);

    QJsonDocument doc = QJsonDocument::fromJson(request.body());
    if (!doc.isObject() || !doc.object().contains("client_name")) {
        return createJsonResponse({{"error", "Invalid JSON or missing 'client_name' field"}}, QHttpServerResponse::StatusCode::BadRequest);
    }

    QString clientName = doc.object()["client_name"].toString();
    int newClientId = DbManager::instance().createClient(clientName);

    if (newClientId > 0) {
        return createJsonResponse({{"client_id", newClientId}, {"client_name", clientName}},
                                  QHttpServerResponse::StatusCode::Created);
    } else {
        return createJsonResponse({{"error", "Failed to create client in database"}}, QHttpServerResponse::StatusCode::InternalServerError);
    }
}

QHttpServerResponse WebServer::handleGetClientByIdRequest(const QString &clientId, const QHttpServerRequest &request)
{
    User* user = authenticateRequest(request);
    if (!user) {
        return createJsonResponse({{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }
    delete user;

    logRequest(request);

    bool ok;
    int id = clientId.toInt(&ok);
    if (!ok) {
        return createJsonResponse({{"error", "Invalid client ID"}}, QHttpServerResponse::StatusCode::BadRequest);
    }

    QJsonObject clientData = DbManager::instance().loadClientDetails(id);

    if (clientData.isEmpty()) {
        return createJsonResponse({{"error", "Client not found"}}, QHttpServerResponse::StatusCode::NotFound);
    }

    return createJsonResponse(clientData, QHttpServerResponse::StatusCode::Ok);
}


QHttpServerResponse WebServer::handleGetIpGenMethodsRequest(const QHttpServerRequest &request)
{
    logRequest(request);
    QList<QVariantMap> methodsData = DbManager::instance().loadAllIpGenMethods();
    QJsonArray jsonArray;
    for (const QVariantMap &methodMap : methodsData) {
        jsonArray.append(QJsonObject::fromVariantMap(methodMap));
    }

    // 3. Створюємо та логуємо відповідь
    // QByteArray bodyJson = QJsonDocument(jsonArray).toJson(QJsonDocument::Compact);
    // logInfo().noquote() << QString("Response: status 200, type: application/json, body: %1")
    //                            .arg(QString::fromUtf8(bodyJson));
    return QHttpServerResponse(jsonArray);
}




// Додайте реалізацію обробника
QHttpServerResponse WebServer::handleTestConnectionRequest(const QHttpServerRequest &request)
{
    User* user = authenticateRequest(request);
    if (!user) {
        return createJsonResponse({{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }
    delete user;

    logRequest(request);

    QJsonDocument doc = QJsonDocument::fromJson(request.body());
    if (!doc.isObject()) {
        return createJsonResponse({{"error", "Invalid JSON"}}, QHttpServerResponse::StatusCode::BadRequest);
    }

    QString error;
    if (DbManager::testConnection(doc.object(), error)) {
        return createJsonResponse({{"status", "success"}, {"message", "Підключення успішне!"}}, QHttpServerResponse::StatusCode::Ok);
    } else {
        return createJsonResponse({{"status", "failure"}, {"message", error}}, QHttpServerResponse::StatusCode::Ok); // Повертаємо 200 ОК, але з помилкою в тілі
    }
}


QHttpServerResponse WebServer::handleUpdateClientRequest(const QString &clientId, const QHttpServerRequest &request)
{
    User* user = authenticateRequest(request);
    if (!user) {
        return createJsonResponse({{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }
    // TODO: Додати перевірку, чи має користувач право редагувати (адмін/менеджер)
    delete user;

    logRequest(request);

    bool ok;
    int id = clientId.toInt(&ok);
    if (!ok) {
        return createJsonResponse({{"error", "Invalid client ID format"}}, QHttpServerResponse::StatusCode::BadRequest);
    }

    QJsonDocument doc = QJsonDocument::fromJson(request.body());
    if (!doc.isObject()) {
        return createJsonResponse({{"error", "Invalid JSON body"}}, QHttpServerResponse::StatusCode::BadRequest);
    }

    if (DbManager::instance().updateClient(id, doc.object())) {
        return QHttpServerResponse(QHttpServerResponse::StatusCode::Ok); // Успіх, 200 OK
    } else {
        return createJsonResponse({{"error", "Failed to update client in database"}}, QHttpServerResponse::StatusCode::InternalServerError);
    }
}


QHttpServerResponse WebServer::handleGetSettingsRequest(const QString& appName, const QHttpServerRequest& request)
{
    if (!authenticateRequest(request))
        return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);

    logRequest(request);
    QVariantMap settings = DbManager::instance().loadSettings(appName);
    return createJsonResponse(QJsonObject::fromVariantMap(settings), QHttpServerResponse::StatusCode::Ok);
}

QHttpServerResponse WebServer::handleUpdateSettingsRequest(const QString& appName, const QHttpServerRequest& request)
{
    if (!authenticateRequest(request))
        return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);

    logRequest(request);
    QJsonDocument doc = QJsonDocument::fromJson(request.body());
    if (!doc.isObject())
        return createJsonResponse({{"error", "Invalid JSON body"}}, QHttpServerResponse::StatusCode::BadRequest);

    if (DbManager::instance().saveSettings(appName, doc.object().toVariantMap())) {
        return QHttpServerResponse(QHttpServerResponse::StatusCode::Ok);
    } else {
        return createJsonResponse({{"error", "Failed to save settings"}}, QHttpServerResponse::StatusCode::InternalServerError);
    }
}
