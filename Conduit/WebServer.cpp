#include "WebServer.h"
#include "Oracle/ApiClient.h"
#include "Oracle/Logger.h"
#include "Oracle/DbManager.h"
#include "version.h"
#include "Oracle/SessionManager.h"
#include "Oracle/ConfigManager.h"

#include "Oracle/User.h"         // Потрібен для доступу до токенів користувача
#include "Oracle/CriptPass.h"    // Потрібен для дешифрування токенів
#include "Oracle/RedmineClient.h" // Потрібен для виклику зовнішнього API
#include "Oracle/JiraClient.h"
#include "Oracle/AppParams.h"    // Потрібен для Redmine Base URL
#include <QEventLoop>            // Потрібен для синхронного очікування відповіді Redmine


#include <QHttpServer>
#include <QHttpServerRequest>
#include <QJsonObject>
#include <QJsonDocument>
#include <QCoreApplication>
#include <QMetaEnum>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QHttpServerResponse>
#include <QUrlQuery>

WebServer::WebServer(quint16 port, const QString& botApiKey, QObject *parent)
    : QObject{parent},
    m_port(port),
    m_botApiKey(botApiKey) // <-- Зберігаємо ключ
{
    m_httpServer = new QHttpServer(this);
    setupRoutes();
}

void WebServer::setupRoutes()
{
    m_httpServer->route("/", [this](const QHttpServerRequest &request) {
        return handleRootRequest(request);
    });
    m_httpServer->route("/status", [this](const QHttpServerRequest &request) {
        return handleStatusRequest(request);
    });
    m_httpServer->route("/api/login", QHttpServerRequest::Method::Post, [this](const QHttpServerRequest &request) {
        return handleLoginRequest(request);
    });
    m_httpServer->route("/api/users", QHttpServerRequest::Method::Get, [this](const QHttpServerRequest &request) {
        return handleGetUsersRequest(request);
    });
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
    m_httpServer->route("/api/settings/<arg>", QHttpServerRequest::Method::Put,
                        [this](const QString& appName, const QHttpServerRequest& request){
                            return handleUpdateSettingsRequest(appName, request);
                        });
    m_httpServer->route("/api/clients/<arg>/sync", QHttpServerRequest::Method::Post,
                        [this](const QString& clientId, const QHttpServerRequest& request) {
                            return handleSyncClientObjectsRequest(clientId, request);
                        });
    m_httpServer->route("/api/clients/<arg>/sync-status", QHttpServerRequest::Method::Get,
                        [this](const QString& clientId, const QHttpServerRequest& request) {
                            return handleGetSyncStatusRequest(clientId, request);
                        });
    m_httpServer->route("/api/objects", QHttpServerRequest::Method::Get,
                        [this](const QHttpServerRequest &request) {
                            return handleGetObjectsRequest(request);
                        });
    m_httpServer->route("/api/regions-list", QHttpServerRequest::Method::Get,
                        [this](const QHttpServerRequest &request) {
                            return handleGetRegionsListRequest(request);
                        });
    m_httpServer->route("/api/bot/register", QHttpServerRequest::Method::Post, [this](const QHttpServerRequest &request) {
        return handleBotRegisterRequest(request);
    });
    m_httpServer->route("/api/bot/requests", QHttpServerRequest::Method::Get, [this](const QHttpServerRequest& request) {
        return handleGetBotRequests(request);
    });

    m_httpServer->route("/api/bot/reject", QHttpServerRequest::Method::Post, [this](const QHttpServerRequest& request) {
        return handleRejectBotRequest(request);
    });

    m_httpServer->route("/api/bot/approve", QHttpServerRequest::Method::Post, [this](const QHttpServerRequest& request) {
        return handleApproveBotRequest(request);
    });

    m_httpServer->route("/api/bot/link", QHttpServerRequest::Method::Post, [this](const QHttpServerRequest& request) {
        return handleLinkBotRequest(request);
    });

    m_httpServer->route("/api/bot/me", QHttpServerRequest::Method::Post, [this](const QHttpServerRequest& request) {
        return handleBotStatusRequest(request);
    });

    // Маршрут для отримання списку активних користувачів бота (для адмінів)
    m_httpServer->route("/api/bot/users", QHttpServerRequest::Method::Get, [this](const QHttpServerRequest& request) {
        return handleGetBotUsersRequest(request);
    });

    // Маршрут для отримання списку АЗС клієнта
    // /api/bot/clients/<clientId>/stations
    m_httpServer->route("/api/bot/clients/<arg>/stations", QHttpServerRequest::Method::Get,
                        [this](const QString& clientId, const QHttpServerRequest& request) {
                            return handleGetClientStations(clientId, request);
                        });

    // Маршрут для отримання деталей однієї АЗС
    // /api/bot/clients/<clientId>/station/<terminalNo>
    m_httpServer->route("/api/bot/clients/<arg>/station/<arg>", QHttpServerRequest::Method::Get,
                        [this](const QString& clientId, const QString& terminalNo, const QHttpServerRequest& request) {
                            return handleGetStationDetails(clientId, terminalNo, request);
                        });

    // --- Export Tasks Management (Керування завданнями експорту) ---

    // GET /api/export-tasks
    // Отримати список усіх завдань. Залишаємо лише один GET маршрут для списку.
    m_httpServer->route("/api/export-tasks", QHttpServerRequest::Method::Get, [this](const QHttpServerRequest &request) {
        // Видаляємо handleGetExportTasksRequest, оскільки він, ймовірно, дублює handleGetAllExportTasksRequest
        return handleGetAllExportTasksRequest(request);
    });

    // POST /api/export-tasks (НОВИЙ МАРШРУТ: СТВОРЕННЯ)
    m_httpServer->route("/api/export-tasks", QHttpServerRequest::Method::Post, [this](const QHttpServerRequest &request) {
        return handleCreateExportTaskRequest(request); // Метод для створення нового завдання
    });

    // GET /api/export-tasks/<taskId> (Отримання ДЕТАЛЕЙ одного завдання)
    m_httpServer->route("/api/export-tasks/<arg>", QHttpServerRequest::Method::Get,
                        [this](const QString &taskId, const QHttpServerRequest &request) {
                            return handleGetExportTaskRequest(taskId, request);
                        });

    // Маршрут для ОНОВЛЕННЯ ОДНОГО завдання (PUT /api/export-tasks/<taskId>)
    m_httpServer->route("/api/export-tasks/<arg>", QHttpServerRequest::Method::Put,
                        [this](const QString &taskId, const QHttpServerRequest &request) {
                            return handleUpdateExportTaskRequest(taskId, request);
                        });

    // --- API для Дашборду ---
    m_httpServer->route("/api/dashboard", QHttpServerRequest::Method::Get, [this](const QHttpServerRequest &request) {
        return handleDashboardRequest(request);
    });

    // Додаємо новий маршрут для РРО
    m_httpServer->route("/api/clients/<arg>/station/<arg>/pos", QHttpServerRequest::Method::Get,
                        [this](const QString &clientId, const QString &terminalNo, const QHttpServerRequest &request) {
                            return handleGetStationPosData(clientId, terminalNo, request);
                        });

    // Додаємо маршрут для резервуарів
    m_httpServer->route("/api/clients/<arg>/station/<arg>/tanks", QHttpServerRequest::Method::Get,
                        [this](const QString &clientId, const QString &terminalNo, const QHttpServerRequest &request) {
                            return handleGetStationTanks(clientId, terminalNo, request);
                        });

    // GET /api/clients/<clientId>/station/<terminalNo>/dispensers
    m_httpServer->route(QStringLiteral("/api/clients/<arg>/station/<arg>/dispensers"), [this](const QString& clientId, const QString& terminalNo, const QHttpServerRequest &request) {
        return handleGetStationDispensers(clientId, terminalNo, request);
    });

    m_httpServer->route("/api/bot/redmine/tasks", QHttpServerRequest::Method::Get, [this](const QHttpServerRequest &request) {
        return handleGetRedmineTasks(request);
    });

    // !!!  МАРШРУТ ДЛЯ ОТРИМАННЯ ЗАДАЧ JIRA ДЛЯ БОТА !!!
    m_httpServer->route("/api/bot/jira/tasks", QHttpServerRequest::Method::Get,
                        [this](const QHttpServerRequest& request) {
                            return handleGetJiraTasks(request);
                        });

}

void WebServer::logRequest(const QHttpServerRequest &request)
{
    const auto metaEnum = QMetaEnum::fromType<QHttpServerRequest::Method>();
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
    QHttpServerResponse response(body, statusCode);
    response.setHeader("Content-Type", "text/plain");
    return response;
}

QHttpServerResponse WebServer::createJsonResponse(const QJsonObject &body, QHttpServerResponse::StatusCode statusCode)
{
    QByteArray bodyJson = QJsonDocument(body).toJson(QJsonDocument::Compact);
    if (statusCode >= QHttpServerResponse::StatusCode::BadRequest) {
        logCritical() << "Server Response Error:" << static_cast<int>(statusCode)
        << bodyJson;
    }
    logInfo().noquote() << QString("Response: status %1, type: application/json, body: %2")
                               .arg(static_cast<int>(statusCode))
                               .arg(QString::fromUtf8(bodyJson));
    return QHttpServerResponse("application/json", bodyJson, statusCode);
}

QHttpServerResponse WebServer::createJsonResponse(const QJsonArray &body, QHttpServerResponse::StatusCode statusCode)
{
    QByteArray bodyJson = QJsonDocument(body).toJson(QJsonDocument::Compact);
    if (statusCode >= QHttpServerResponse::StatusCode::BadRequest) {
        logCritical() << "Server Response Error:" << static_cast<int>(statusCode)
        << bodyJson;
    }
    logInfo().noquote() << QString("Response: status %1, type: application/json, body: %2")
                               .arg(static_cast<int>(statusCode))
                               .arg(QString::fromUtf8(bodyJson));
    return QHttpServerResponse("application/json", bodyJson, statusCode);
}

QHttpServerResponse WebServer::handleRootRequest(const QHttpServerRequest &request)
{
    logRequest(request);
    return createTextResponse("Hello from Conduit Server!", QHttpServerResponse::StatusCode::Ok);
}

QHttpServerResponse WebServer::handleStatusRequest(const QHttpServerRequest &request)
{
    logRequest(request);
    QJsonObject json;
    json["application_version"] = PROJECT_VERSION_STR;
    json["build_date"] = PROJECT_BUILD_DATETIME;
    bool isDbConnected = DbManager::instance().isConnected();
    json["database_status"] = isDbConnected ? "connected" : "disconnected";
    return createJsonResponse(json, QHttpServerResponse::StatusCode::Ok);
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

QHttpServerResponse WebServer::handleLoginRequest(const QHttpServerRequest &request)
{
    logRequest(request);
    const QByteArray body = request.body();
    QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isNull() || !doc.isObject() || !doc.object().contains("login")) {
        return createJsonResponse(QJsonObject{{"error", "Invalid JSON or missing 'login' field"}}, QHttpServerResponse::StatusCode::BadRequest);
    }
    QString login = doc.object().value("login").toString();
    auto loginResult = SessionManager::instance().login(login);
    const User* user = loginResult.first;
    const QString token = loginResult.second;
    if (user && !token.isEmpty()) {
        QJsonObject responseJson;
        responseJson["user"] = user->toJson();
        responseJson["token"] = token;
        return createJsonResponse(responseJson, QHttpServerResponse::StatusCode::Ok);
    } else {
        return createJsonResponse(QJsonObject{{"error", "User identification failed or user is inactive"}}, QHttpServerResponse::StatusCode::Forbidden);
    }
}

QHttpServerResponse WebServer::handleGetUsersRequest(const QHttpServerRequest &request)
{
    logRequest(request);
    User* user = authenticateRequest(request);
    if (!user) {
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }
    logDebug() << "Request authenticated for user:" << user->login();
    QList<User*> users = DbManager::instance().loadAllUsers();
    QJsonArray jsonArray;
    for (User* u : users) {
        jsonArray.append(u->toJson());
    }
    qDeleteAll(users);
    delete user;
    return createJsonResponse(jsonArray, QHttpServerResponse::StatusCode::Ok); // Fixed to use helper
}

QHttpServerResponse WebServer::handleGetUserByIdRequest(const QString &userId, const QHttpServerRequest &request)
{
    logRequest(request);
    User* requestingUser = authenticateRequest(request);
    if (!requestingUser) {
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }
    logDebug() << "Request authenticated for user:" << requestingUser->login();
    bool ok;
    int targetUserId = userId.toInt(&ok);
    if (!ok) {
        delete requestingUser;
        return createJsonResponse(QJsonObject{{"error", "Invalid user ID format"}}, QHttpServerResponse::StatusCode::BadRequest);
    }
    if (!requestingUser->hasRole("Адміністратор") && !requestingUser->hasRole("Менеджер") && requestingUser->id() != targetUserId) {
        logWarning() << "ACCESS DENIED: User" << requestingUser->login() << "tried to access profile of user ID" << targetUserId;
        delete requestingUser;
        return createJsonResponse(QJsonObject{{"error", "Forbidden"}}, QHttpServerResponse::StatusCode::Forbidden);
    }
    User* targetUser = DbManager::instance().loadUser(targetUserId);
    if (targetUser) {
        QJsonObject json = targetUser->toJson();
        delete requestingUser;
        delete targetUser;
        return createJsonResponse(json, QHttpServerResponse::StatusCode::Ok);
    } else {
        delete requestingUser;
        return createJsonResponse(QJsonObject{{"error", "User not found"}}, QHttpServerResponse::StatusCode::NotFound);
    }
}

QHttpServerResponse WebServer::handleGetRolesRequest(const QHttpServerRequest &request)
{
    logRequest(request);
    QList<QVariantMap> rolesData = DbManager::instance().loadAllRoles();
    QJsonArray jsonArray;
    for (const QVariantMap &roleMap : rolesData) {
        jsonArray.append(QJsonObject::fromVariantMap(roleMap));
    }
    return createJsonResponse(jsonArray, QHttpServerResponse::StatusCode::Ok); // Fixed to use helper
}

QHttpServerResponse WebServer::handleUpdateUserRequest(const QString &userId, const QHttpServerRequest &request)
{
    logRequest(request);
    User* requestingUser = authenticateRequest(request);
    if (!requestingUser) {
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }
    logDebug() << "Request authenticated for user:" << requestingUser->login();
    bool ok;
    int targetUserId = userId.toInt(&ok);
    if (!ok) {
        delete requestingUser;
        return createJsonResponse(QJsonObject{{"error", "Invalid user ID format"}}, QHttpServerResponse::StatusCode::BadRequest);
    }
    QJsonDocument doc = QJsonDocument::fromJson(request.body());
    QJsonObject userData = doc.object();
    if (!doc.isObject()) {
        delete requestingUser;
        return createJsonResponse(QJsonObject{{"error", "Invalid JSON body"}}, QHttpServerResponse::StatusCode::BadRequest);
    }
    if (!requestingUser->hasRole("Адміністратор") && !requestingUser->hasRole("Менеджер") && requestingUser->id() != targetUserId) {
        logWarning() << "ACCESS DENIED: User" << requestingUser->login() << "tried to UPDATE profile of user ID" << targetUserId;
        delete requestingUser;
        return createJsonResponse(QJsonObject{{"error", "Forbidden: You cannot edit this user"}}, QHttpServerResponse::StatusCode::Forbidden);
    }
    if (userData.contains("roles") && !requestingUser->hasRole("Адміністратор")) {
        logWarning() << "ACCESS DENIED: Non-admin user" << requestingUser->login() << "tried to change roles.";
        delete requestingUser;
        return createJsonResponse(QJsonObject{{"error", "Forbidden: Only administrators can change roles"}}, QHttpServerResponse::StatusCode::Forbidden);
    }
    if (DbManager::instance().updateUser(targetUserId, userData)) {
        delete requestingUser;
        return QHttpServerResponse(QHttpServerResponse::StatusCode::Ok);
    } else {
        delete requestingUser;
        return createJsonResponse(QJsonObject{{"error", "Failed to update user in database"}}, QHttpServerResponse::StatusCode::InternalServerError);
    }
}

// User* WebServer::authenticateRequest(const QHttpServerRequest &request)
// {
//     const auto &headers = request.headers();
//     QByteArray authHeader;
//     for (const auto &headerPair : headers) {
//         if (headerPair.first.compare("Authorization", Qt::CaseInsensitive) == 0) {
//             authHeader = headerPair.second;
//             break;
//         }
//     }
//     if (!authHeader.startsWith("Bearer ")) {
//         return nullptr;
//     }
//     QByteArray token = authHeader.mid(7);
//     QByteArray tokenHash = QCryptographicHash::hash(token, QCryptographicHash::Sha256).toHex();
//     int userId = DbManager::instance().findUserIdByToken(tokenHash);
//     if (userId > 0) {
//         return DbManager::instance().loadUser(userId);
//     }
//     return nullptr;
// }

/**
 * @brief Перевіряє запит на аутентифікацію. (ВЕРСІЯ 4: ВИПРАВЛЕНО СИСТЕМНУ АУТЕНТИФІКАЦІЮ БОТА)
 * Розуміє три методи:
 * 1. Gandalf (GUI): Заголовок "Authorization: Bearer <session_token>"
 * 2. Isengard (Bot): Заголовок "X-Bot-Token: <secret_key>" (Для системних запитів)
 * 3. Isengard (Bot): Заголовки "X-Bot-Token: <secret_key>" + "X-Telegram-ID: <user_id>" (Для запитів користувача)
 * @return Об'єкт User* у разі успіху (вимагає delete), або nullptr.
 */
User* WebServer::authenticateRequest(const QHttpServerRequest &request)
{
    // --- 1. Пошук всіх заголовків ---
    const auto &headers = request.headers();
    QByteArray authHeader;
    QByteArray botTokenHeader;
    QByteArray telegramIdHeader;

    // Збираємо заголовки (залишаємо як є)
    for (const auto &headerPair : headers) {
        if (headerPair.first.compare("Authorization", Qt::CaseInsensitive) == 0) {
            authHeader = headerPair.second;
        } else if (headerPair.first.compare("X-Bot-Token", Qt::CaseInsensitive) == 0) {
            botTokenHeader = headerPair.second;
        } else if (headerPair.first.compare("X-Telegram-ID", Qt::CaseInsensitive) == 0) {
            telegramIdHeader = headerPair.second;
        }
    }

    // --- Спроба №1: Аутентифікація Gandalf (токен сесії) ---
    if (authHeader.startsWith("Bearer ")) {
        QByteArray token = authHeader.mid(7);
        QByteArray tokenHash = QCryptographicHash::hash(token, QCryptographicHash::Sha256).toHex();
        int userId = DbManager::instance().findUserIdByToken(tokenHash);

        if (userId > 0) {
            User* user = DbManager::instance().loadUser(userId);
            if (user && user->isActive()) {
                return user; // Успіх (Gandalf)
            }
            if (user) delete user;
        }
    }

    // !!! НОВА Спроба №2: АУТЕНТИФІКАЦІЯ СИСТЕМИ БОТА (ТІЛЬКИ X-Bot-Token) !!!
    // Це потрібно для запитів, що не залежать від конкретного користувача (напр. fetchSettings).
    if (!botTokenHeader.isEmpty() && telegramIdHeader.isEmpty()) {

        // 1. Перевіряємо секретний ключ бота
        if (!m_botApiKey.isEmpty() && botTokenHeader == m_botApiKey.toUtf8()) {

            // Токен бота вірний. Аутентифікація успішна.
            logDebug() << "Bot API Key valid. Authenticating as System User (ID 1).";

            // Завантажуємо System User (ID 1), який повинен мати права адміністратора
            User* systemUser = DbManager::instance().loadUser(1);

            if (systemUser && systemUser->isActive()) {
                return systemUser; // Успіх (Системна аутентифікація)
            }
            if (systemUser) delete systemUser;

            logCritical() << "Bot API Key valid, but System User (ID 1) cannot be loaded or is inactive!";
        }
    }

    // --- Спроба №3: Аутентифікація Isengard (Токен + Telegram ID, для user-specific запитів) ---
    // Це стара Спроба №2.
    if (!botTokenHeader.isEmpty() && !telegramIdHeader.isEmpty()) {

        // 1. Перевіряємо секретний ключ бота (ВИПРАВЛЕНО: беремо з m_botApiKey)
        if (m_botApiKey.isEmpty() || m_botApiKey == "YOUR_DEFAULT_BOT_KEY_HERE") {
            // Цей лог може спрацювати, якщо m_botApiKey не було налаштовано,
            // але ми припускаємо, що він налаштований.
            logCritical() << "Bot API Key is not set on the server! Cannot authenticate bot user.";
            return nullptr;
        }

        if (botTokenHeader == m_botApiKey.toUtf8()) {
            // 2. Токен бота вірний. Шукаємо користувача за telegram_id
            qint64 telegramId = telegramIdHeader.toLongLong();
            if (telegramId == 0) {
                logWarning() << "Bot authentication failed: Invalid X-Telegram-ID header.";
                return nullptr;
            }

            int userId = DbManager::instance().findUserIdByTelegramId(telegramId);

            if (userId > 0) {
                User* user = DbManager::instance().loadUser(userId);
                if (user && user->isActive()) {
                    return user; // Успіх (Isengard User)
                }
                if (user) delete user;
            }
        } else {
            logWarning() << "Bot authentication failed: Invalid X-Bot-Token.";
        }
    }

    // --- Провал ---
    return nullptr;
}


QHttpServerResponse WebServer::handleGetClientsRequest(const QHttpServerRequest &request)
{
    logRequest(request);
    QList<QVariantMap> clientsData = DbManager::instance().loadAllClients();
    QJsonArray jsonArray;
    for (const QVariantMap &clientMap : clientsData) {
        jsonArray.append(QJsonObject::fromVariantMap(clientMap));
    }
    return createJsonResponse(jsonArray, QHttpServerResponse::StatusCode::Ok); // Fixed to use helper
}

QHttpServerResponse WebServer::handleCreateClientRequest(const QHttpServerRequest &request)
{
    User* user = authenticateRequest(request);
    if (!user) {
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }
    delete user;
    logRequest(request);
    QJsonDocument doc = QJsonDocument::fromJson(request.body());
    if (!doc.isObject() || !doc.object().contains("client_name")) {
        return createJsonResponse(QJsonObject{{"error", "Invalid JSON or missing 'client_name' field"}}, QHttpServerResponse::StatusCode::BadRequest);
    }
    QString clientName = doc.object()["client_name"].toString();
    int newClientId = DbManager::instance().createClient(clientName);
    if (newClientId > 0) {
        return createJsonResponse(QJsonObject{{"client_id", newClientId}, {"client_name", clientName}}, QHttpServerResponse::StatusCode::Created);
    } else {
        return createJsonResponse(QJsonObject{{"error", "Failed to create client in database"}}, QHttpServerResponse::StatusCode::InternalServerError);
    }
}

QHttpServerResponse WebServer::handleGetClientByIdRequest(const QString &clientId, const QHttpServerRequest &request)
{
    User* user = authenticateRequest(request);
    if (!user) {
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }
    delete user;
    logRequest(request);
    bool ok;
    int id = clientId.toInt(&ok);
    if (!ok) {
        return createJsonResponse(QJsonObject{{"error", "Invalid client ID"}}, QHttpServerResponse::StatusCode::BadRequest);
    }
    QJsonObject clientData = DbManager::instance().loadClientDetails(id);
    if (clientData.isEmpty()) {
        return createJsonResponse(QJsonObject{{"error", "Client not found"}}, QHttpServerResponse::StatusCode::NotFound);
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
    return createJsonResponse(jsonArray, QHttpServerResponse::StatusCode::Ok); // Fixed to use helper
}

QHttpServerResponse WebServer::handleTestConnectionRequest(const QHttpServerRequest &request)
{
    User* user = authenticateRequest(request);
    if (!user) {
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }
    delete user;
    logRequest(request);
    QJsonDocument doc = QJsonDocument::fromJson(request.body());
    if (!doc.isObject()) {
        return createJsonResponse(QJsonObject{{"error", "Invalid JSON"}}, QHttpServerResponse::StatusCode::BadRequest);
    }
    QString error;
    if (DbManager::testConnection(doc.object(), error)) {
        return createJsonResponse(QJsonObject{{"status", "success"}, {"message", "Підключення успішне!"}}, QHttpServerResponse::StatusCode::Ok);
    } else {
        return createJsonResponse(QJsonObject{{"status", "failure"}, {"message", error}}, QHttpServerResponse::StatusCode::Ok);
    }
}

QHttpServerResponse WebServer::handleUpdateClientRequest(const QString &clientId, const QHttpServerRequest &request)
{
    User* user = authenticateRequest(request);
    if (!user) {
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }
    delete user;
    logRequest(request);
    bool ok;
    int id = clientId.toInt(&ok);
    if (!ok) {
        return createJsonResponse(QJsonObject{{"error", "Invalid client ID format"}}, QHttpServerResponse::StatusCode::BadRequest);
    }
    QJsonDocument doc = QJsonDocument::fromJson(request.body());
    if (!doc.isObject()) {
        return createJsonResponse(QJsonObject{{"error", "Invalid JSON body"}}, QHttpServerResponse::StatusCode::BadRequest);
    }
    if (DbManager::instance().updateClient(id, doc.object())) {
        return QHttpServerResponse(QHttpServerResponse::StatusCode::Ok);
    } else {
        return createJsonResponse(QJsonObject{{"error", "Failed to update client in database"}}, QHttpServerResponse::StatusCode::InternalServerError);
    }
}

QHttpServerResponse WebServer::handleGetSettingsRequest(const QString& appName, const QHttpServerRequest& request)
{
    if (!authenticateRequest(request))
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    logRequest(request);
    QVariantMap settings = DbManager::instance().loadSettings(appName);
    return createJsonResponse(QJsonObject::fromVariantMap(settings), QHttpServerResponse::StatusCode::Ok);
}

QHttpServerResponse WebServer::handleUpdateSettingsRequest(const QString& appName, const QHttpServerRequest& request)
{
    if (!authenticateRequest(request))
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    logRequest(request);
    QJsonDocument doc = QJsonDocument::fromJson(request.body());
    if (!doc.isObject())
        return createJsonResponse(QJsonObject{{"error", "Invalid JSON body"}}, QHttpServerResponse::StatusCode::BadRequest);
    if (DbManager::instance().saveSettings(appName, doc.object().toVariantMap())) {
        return QHttpServerResponse(QHttpServerResponse::StatusCode::Ok);
    } else {
        return createJsonResponse(QJsonObject{{"error", "Failed to save settings"}}, QHttpServerResponse::StatusCode::InternalServerError);
    }
}

QHttpServerResponse WebServer::handleSyncClientObjectsRequest(const QString& clientId, const QHttpServerRequest& request)
{
    logRequest(request);
    User* user = authenticateRequest(request);
    if (!user) {
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }
    delete user;
    bool ok;
    int id = clientId.toInt(&ok);
    if (!ok) {
        return createJsonResponse(QJsonObject{{"error", "Invalid client ID format"}}, QHttpServerResponse::StatusCode::BadRequest);
    }
    QThreadPool::globalInstance()->start([id]() {
        try {
            logInfo() << "Starting background synchronization for client ID:" << id;
            QVariantMap result = DbManager::instance().syncClientObjects(id);
            if (result.contains("error")) {
                logCritical() << "Synchronization failed for client" << id << ":" << result["error"].toString();
            } else {
                logInfo() << "Synchronization completed for client" << id
                          << ". Processed" << result["processed_count"].toInt() << "objects.";
            }
        } catch (const std::exception& e) {
            logCritical() << "!!! CRITICAL EXCEPTION in sync thread for client" << id << ":" << e.what();
        } catch (...) {
            logCritical() << "!!! UNKNOWN CRITICAL EXCEPTION in sync thread for client" << id;
        }
    });
    return createJsonResponse(QJsonObject{{"status", "Synchronization started in background"}}, QHttpServerResponse::StatusCode::Accepted);
}

QHttpServerResponse WebServer::handleGetSyncStatusRequest(const QString &clientId, const QHttpServerRequest &request)
{
    logRequest(request);
    User* user = authenticateRequest(request);
    if (!user) {
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }
    delete user;
    bool ok;
    int id = clientId.toInt(&ok);
    if (!ok) {
        return createJsonResponse(QJsonObject{{"error", "Invalid client ID"}}, QHttpServerResponse::StatusCode::BadRequest);
    }
    QVariantMap status = DbManager::instance().getSyncStatus(id);
    return createJsonResponse(QJsonObject::fromVariantMap(status), QHttpServerResponse::StatusCode::Ok);
}

QHttpServerResponse WebServer::handleGetObjectsRequest(const QHttpServerRequest &request)
{
    logRequest(request);
    if (!authenticateRequest(request)) {
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }
    QVariantMap filters;
    const QUrlQuery query(request.url());
    if (query.hasQueryItem("clientId"))
        filters["clientId"] = query.queryItemValue("clientId").toInt();
    if (query.hasQueryItem("region"))
        filters["region"] = query.queryItemValue("region");
    if (query.hasQueryItem("search"))
        filters["search"] = query.queryItemValue("search");
    if (query.hasQueryItem("isActive"))
        filters["isActive"] = query.queryItemValue("isActive") == "true";
    if (query.hasQueryItem("isWork"))
        filters["isWork"] = query.queryItemValue("isWork") == "true";
    if (query.hasQueryItem("terminalId"))
        filters["terminalId"] = query.queryItemValue("terminalId").toInt();
    QList<QVariantMap> objects = DbManager::instance().getObjects(filters);
    QJsonArray jsonArray;
    for (const auto& map : objects) {
        jsonArray.append(QJsonObject::fromVariantMap(map));
    }
    QJsonObject responseBody;
    responseBody["objects"] = jsonArray;
    return createJsonResponse(responseBody, QHttpServerResponse::StatusCode::Ok);
}

QHttpServerResponse WebServer::handleGetRegionsListRequest(const QHttpServerRequest &request)
{
    if (!authenticateRequest(request))
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    QStringList regions = DbManager::instance().getUniqueRegionsList();
    return createJsonResponse(QJsonObject{{"regions", QJsonArray::fromStringList(regions)}}, QHttpServerResponse::StatusCode::Ok);
}

QHttpServerResponse WebServer::handleBotRegisterRequest(const QHttpServerRequest &request)
{
    logRequest(request);
    QJsonDocument doc = QJsonDocument::fromJson(request.body());
    if (!doc.isObject()) {
        return createJsonResponse(QJsonObject{{"error", "Invalid JSON body"}}, QHttpServerResponse::StatusCode::BadRequest);
    }
    QJsonObject userData = doc.object();
    if (!userData.contains("telegram_id") || !userData.contains("username")) {
        return createJsonResponse(QJsonObject{{"error", "Missing required fields: telegram_id, username"}}, QHttpServerResponse::StatusCode::BadRequest);
    }
    QJsonObject result = DbManager::instance().registerBotUser(userData);
    if (result["status"] == "error") {
        return createJsonResponse(result, QHttpServerResponse::StatusCode::InternalServerError);
    }
    return createJsonResponse(result, QHttpServerResponse::StatusCode::Ok);
}

QHttpServerResponse WebServer::handleGetBotRequests(const QHttpServerRequest& request)
{
    logRequest(request);
    User* user = authenticateRequest(request);
    if (!user) {
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }
    if (!user->hasRole("Адміністратор")) {
        delete user;
        return createJsonResponse(QJsonObject{{"error", "Forbidden"}}, QHttpServerResponse::StatusCode::Forbidden);
    }
    delete user;
    QJsonArray requests = DbManager::instance().getPendingBotRequests();
    return createJsonResponse(requests, QHttpServerResponse::StatusCode::Ok);
}


/**
 * @brief Обробляє запит на відхилення заявки від бота (POST /api/bot/reject)
 */
QHttpServerResponse WebServer::handleRejectBotRequest(const QHttpServerRequest &request)
{
    logRequest(request);

    // 1. Перевірка, чи є запит від адміністратора
    User* adminUser = authenticateRequest(request);
    if (!adminUser || !adminUser->hasRole("Адміністратор")) {
        if (adminUser) delete adminUser;
        return createJsonResponse(QJsonObject{{"error", "Forbidden"}}, QHttpServerResponse::StatusCode::Forbidden);
    }
    delete adminUser;

    // 2. Парсимо тіло запиту, щоб отримати request_id
    QJsonDocument doc = QJsonDocument::fromJson(request.body());
    if (!doc.isObject() || !doc.object().contains("request_id")) {
        return createJsonResponse(QJsonObject{{"error", "Invalid JSON or missing 'request_id' field"}}, QHttpServerResponse::StatusCode::BadRequest);
    }

    int requestId = doc.object()["request_id"].toInt();
    if (requestId <= 0) {
        return createJsonResponse(QJsonObject{{"error", "Invalid 'request_id'"}}, QHttpServerResponse::StatusCode::BadRequest);
    }

    // 3. Викликаємо метод для відхилення
    if (DbManager::instance().rejectBotRequest(requestId)) {
        // Успіх
        return createJsonResponse(QJsonObject{{"status", "success"}, {"message", "Request rejected successfully."}}, QHttpServerResponse::StatusCode::Ok);
    } else {
        // Помилка (напр., запит не знайдено або помилка БД)
        return createJsonResponse(QJsonObject{{"error", "Failed to reject request."}}, QHttpServerResponse::StatusCode::InternalServerError);
    }
}

/**
 * @brief Обробляє запит на схвалення заявки та створення/прив'язку нового користувача
 * (POST /api/bot/approve)
 */
QHttpServerResponse WebServer::handleApproveBotRequest(const QHttpServerRequest &request)
{
    logRequest(request);

    // 1. Перевірка, чи є запит від адміністратора
    User* adminUser = authenticateRequest(request);
    if (!adminUser || !adminUser->hasRole("Адміністратор")) {
        if (adminUser) delete adminUser;
        return createJsonResponse(QJsonObject{{"error", "Forbidden"}}, QHttpServerResponse::StatusCode::Forbidden);
    }
    delete adminUser;

    // 2. Парсимо тіло запиту
    QJsonDocument doc = QJsonDocument::fromJson(request.body());
    if (!doc.isObject()) {
        return createJsonResponse(QJsonObject{{"error", "Invalid JSON body"}}, QHttpServerResponse::StatusCode::BadRequest);
    }
    QJsonObject body = doc.object();

    // 3. Перевіряємо наявність обох полів: request_id та login
    if (!body.contains("request_id") || !body.contains("login")) {
        return createJsonResponse(QJsonObject{{"error", "Missing 'request_id' or 'login' field"}}, QHttpServerResponse::StatusCode::BadRequest);
    }

    int requestId = body["request_id"].toInt();
    QString login = body["login"].toString().trimmed();

    if (requestId <= 0 || login.isEmpty()) {
        return createJsonResponse(QJsonObject{{"error", "Invalid 'request_id' or empty 'login'"}}, QHttpServerResponse::StatusCode::BadRequest);
    }

    // 4. Викликаємо метод DbManager
    if (DbManager::instance().approveBotRequest(requestId, login)) {
        // Успіх
        return createJsonResponse(QJsonObject{{"status", "success"}, {"message", "Request approved successfully."}}, QHttpServerResponse::StatusCode::Ok);
    } else {
        // Помилка (напр., запит не знайдено, помилка транзакції)
        return createJsonResponse(QJsonObject{{"error", "Failed to approve request."}}, QHttpServerResponse::StatusCode::InternalServerError);
    }
}

/**
 * @brief Обробляє запит на прив'язку заявки бота до існуючого користувача
 * (POST /api/bot/link)
 */
QHttpServerResponse WebServer::handleLinkBotRequest(const QHttpServerRequest &request)
{
    logRequest(request);

    // 1. Перевірка, чи є запит від адміністратора
    User* adminUser = authenticateRequest(request);
    if (!adminUser || !adminUser->hasRole("Адміністратор")) {
        if (adminUser) delete adminUser;
        return createJsonResponse(QJsonObject{{"error", "Forbidden"}}, QHttpServerResponse::StatusCode::Forbidden);
    }
    delete adminUser;

    // 2. Парсимо тіло запиту
    QJsonDocument doc = QJsonDocument::fromJson(request.body());
    if (!doc.isObject()) {
        return createJsonResponse(QJsonObject{{"error", "Invalid JSON body"}}, QHttpServerResponse::StatusCode::BadRequest);
    }
    QJsonObject body = doc.object();

    // 3. Перевіряємо наявність обох полів: request_id та user_id
    if (!body.contains("request_id") || !body.contains("user_id")) {
        return createJsonResponse(QJsonObject{{"error", "Missing 'request_id' or 'user_id' field"}}, QHttpServerResponse::StatusCode::BadRequest);
    }

    int requestId = body["request_id"].toInt();
    int userId = body["user_id"].toInt();

    if (requestId <= 0 || userId <= 0) {
        return createJsonResponse(QJsonObject{{"error", "Invalid 'request_id' or 'user_id'"}}, QHttpServerResponse::StatusCode::BadRequest);
    }

    // 4. Викликаємо метод DbManager
    if (DbManager::instance().linkBotRequest(requestId, userId)) {
        // Успіх
        return createJsonResponse(QJsonObject{{"status", "success"}, {"message", "Request linked successfully."}}, QHttpServerResponse::StatusCode::Ok);
    } else {
        // Помилка
        return createJsonResponse(QJsonObject{{"error", "Failed to link request."}}, QHttpServerResponse::StatusCode::InternalServerError);
    }
}

//
/**
 * @brief (ОНОВЛЕНО) Обробляє запит від бота для перевірки статусу (POST /api/bot/me)
 * Тепер цей маршрут вимагає наявності валідного X-Bot-Token.
 */
QHttpServerResponse WebServer::handleBotStatusRequest(const QHttpServerRequest &request)
{
    // --- ДОДАНО ПЕРЕВІРКУ БЕЗПЕКИ ---
    QByteArray botTokenHeader;
    for (const auto &headerPair : request.headers()) {
        if (headerPair.first.compare("X-Bot-Token", Qt::CaseInsensitive) == 0) {
            botTokenHeader = headerPair.second;
            break;
        }
    }

    // m_botApiKey ми зберегли в конструкторі WebServer
    if (botTokenHeader.isEmpty() || botTokenHeader != m_botApiKey.toUtf8()) {
        logWarning() << "Bot status request failed: Invalid or missing X-Bot-Token.";

        // --- ВИПРАВЛЕНО ТУТ ---
        // Використовуємо явний конструктор, як у вашому коді
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}},
                                  QHttpServerResponse::StatusCode::Unauthorized);
    }
    // --- КІНЕЦЬ ПЕРЕВІРКИ БЕЗПЕКИ ---


    // +++ ВАШ ІСНУЮЧИЙ КОД (БЕЗ ЗМІН) +++
    logRequest(request);

    // 1. Парсимо тіло запиту
    QJsonDocument doc = QJsonDocument::fromJson(request.body());
    if (!doc.isObject() || !doc.object().contains("telegram_id")) {
        return createJsonResponse(QJsonObject{{"error", "Invalid JSON or missing 'telegram_id' field"}},
                                  QHttpServerResponse::StatusCode::BadRequest);
    }

    qint64 telegramId = doc.object()["telegram_id"].toVariant().toLongLong();
    if (telegramId <= 0) {
        return createJsonResponse(QJsonObject{{"error", "Invalid 'telegram_id'"}},
                                  QHttpServerResponse::StatusCode::BadRequest);
    }

    // 2. Викликаємо наш новий "розумний" метод
    QJsonObject status = DbManager::instance().getBotUserStatus(telegramId);

    // 3. Повертаємо результат
    return createJsonResponse(status, QHttpServerResponse::StatusCode::Ok);
    // +++ КІНЕЦЬ ВАШОГО КОДУ +++
}

//

/**
 * @brief (НОВИЙ) Обробляє запит на список активних користувачів бота (GET /api/bot/users).
 * Вимагає аутентифікації адміна.
 */
QHttpServerResponse WebServer::handleGetBotUsersRequest(const QHttpServerRequest &request)
{
    logRequest(request);

    // 1. Перевірка, чи є запит від адміністратора
    User* adminUser = authenticateRequest(request);
    if (!adminUser || !adminUser->hasRole("Адміністратор")) {
        if (adminUser) delete adminUser;
        return createJsonResponse(QJsonObject{{"error", "Forbidden"}}, QHttpServerResponse::StatusCode::Forbidden);
    }
    delete adminUser; // Більше не потрібен

    // 2. Отримуємо дані з БД
    QJsonArray users = DbManager::instance().getActiveBotUsers();

    // 3. Повертаємо результат
    return createJsonResponse(users, QHttpServerResponse::StatusCode::Ok);
}


//

/**
 * @brief (НОВИЙ) Обробляє запит на список АЗС (GET /api/bot/clients/<id>/stations).
 */
QHttpServerResponse WebServer::handleGetClientStations(const QString &clientId, const QHttpServerRequest &request)
{
    logRequest(request);

    // 1. Аутентифікація (ВАЖЛИВО: не адмін, а будь-який активний юзер бота)
    User* user = authenticateRequest(request);
    if (!user) { // Немає токена або невалідний
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }

    // 2. Отримуємо дані з БД (метод сам перевірить права user vs client)
    QJsonArray stations = DbManager::instance().getStationsForClient(user->id(), clientId.toInt());
    delete user;

    return createJsonResponse(stations, QHttpServerResponse::StatusCode::Ok);
}

/**
 * @brief (НОВИЙ) Обробляє запит на деталі АЗС (GET /api/bot/clients/<id>/station/<termNo>).
 */
QHttpServerResponse WebServer::handleGetStationDetails(const QString &clientId, const QString &terminalNo, const QHttpServerRequest &request)
{
    logRequest(request);

    // 1. Аутентифікація
    User* user = authenticateRequest(request);
    if (!user) {
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }

    // 2. Отримуємо дані з БД
    QJsonObject details = DbManager::instance().getStationDetails(user->id(), clientId.toInt(), terminalNo);
    delete user;

    if (details.contains("error")) {
        return createJsonResponse(details, QHttpServerResponse::StatusCode::NotFound);
    }

    return createJsonResponse(details, QHttpServerResponse::StatusCode::Ok);
}


//
/**
 * @brief Обробляє запит GET /api/export-tasks
 * Повертає список всіх *активних* шаблонів завдань для Експортера.
 * Вимагає прав адміністратора.
 */
QHttpServerResponse WebServer::handleGetExportTasksRequest(const QHttpServerRequest &request)
{
    logRequest(request);

    // 1. Аутентифікація та Авторизація
    User* user = authenticateRequest(request);
    if (!user) {
        // Явно вказуємо тип QJsonObject
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }

    // ТІЛЬКИ Адміністратори можуть отримувати список завдань
    if (!user->hasRole("Адміністратор")) {
        delete user;
        // Явно вказуємо тип QJsonObject
        return createJsonResponse(QJsonObject{{"error", "Forbidden"}}, QHttpServerResponse::StatusCode::Forbidden);
    }
    delete user; // Більше не потрібен

    // 2. Отримуємо дані
    QList<QVariantMap> tasks = DbManager::instance().loadAllExportTasks();

    QJsonArray tasksArray;
    for (const QVariantMap& taskMap : tasks) {
        tasksArray.append(QJsonObject::fromVariantMap(taskMap));
    }


    // 4. Відправляємо відповідь
    return createJsonResponse(tasksArray, QHttpServerResponse::StatusCode::Ok);
}


QHttpServerResponse WebServer::handleGetAllExportTasksRequest(const QHttpServerRequest &request)
{
    logRequest(request);
    User* user = authenticateRequest(request);

    if (!user) {
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }
    if (!user->isAdmin()) {
        delete user;
        return createJsonResponse(QJsonObject{{"error", "Forbidden. Requires Administrator role"}}, QHttpServerResponse::StatusCode::Forbidden);
    }
    delete user;

    // Отримуємо список завдань з БД
    QList<QVariantMap> tasks = DbManager::instance().loadAllExportTasks();

    QJsonArray tasksArray;
    for (const QVariantMap& task : tasks) {
        tasksArray.append(QJsonObject::fromVariantMap(task));
    }

    return createJsonResponse(tasksArray, QHttpServerResponse::StatusCode::Ok);
}

/**
 * @brief Обробляє GET-запит для отримання деталей одного завдання (включаючи SQL-шаблон).
 * Маршрут: GET /api/export-tasks/<taskId>
 * @param taskId ID завдання, яке потрібно завантажити (з URL).
 * @return QHttpServerResponse з деталями завдання (200 OK) або помилкою (404 Not Found).
 */
QHttpServerResponse WebServer::handleGetExportTaskRequest(const QString &taskId, const QHttpServerRequest &request)
{
    logRequest(request);
    User* user = authenticateRequest(request);

    // 1. АУТЕНТИФІКАЦІЯ ТА АВТОРИЗАЦІЯ
    if (!user) {
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }
    if (!user->isAdmin()) {
        delete user;
        return createJsonResponse(QJsonObject{{"error", "Forbidden. Requires Administrator role"}}, QHttpServerResponse::StatusCode::Forbidden);
    }
    delete user;

    // 2. ВАЛІДАЦІЯ ID
    int id = taskId.toInt();
    if (id <= 0) {
        return createJsonResponse(QJsonObject{{"error", "Invalid TASK_ID specified"}}, QHttpServerResponse::StatusCode::BadRequest);
    }

    // 3. ВИКЛИК ЗАВАНТАЖЕННЯ З БАЗИ ДАНИХ
    QJsonObject taskDetails = DbManager::instance().loadExportTaskById(id);

    // 4. ФОРМУВАННЯ ВІДПОВІДІ
    if (taskDetails.isEmpty()) {
        // Якщо завдання не знайдено або DB Manager повернув помилку
        qWarning() << "Export Task ID" << id << "not found or DB error.";

        // Перевіряємо, чи є деталі помилки в DbManager (хоча для 404 це зазвичай просто відсутність запису)
        QString dbError = DbManager::instance().lastError();
        if (dbError.contains("not found", Qt::CaseInsensitive)) {
            return createJsonResponse(QJsonObject{{"error", QString("Export Task ID %1 not found.").arg(id)}}, QHttpServerResponse::StatusCode::NotFound);
        }

        // Якщо це інша помилка, повертаємо 500
        return createJsonResponse(QJsonObject{{"error", "Failed to fetch task details from DB: " + dbError}}, QHttpServerResponse::StatusCode::InternalServerError);

    } else if (taskDetails.contains("error")) {
        // Якщо DB Manager повернув об'єкт з внутрішньою помилкою (наприклад, помилка SQL)
        qCritical() << "DbManager returned an error for task ID" << id << ":" << taskDetails["error"].toString();
        return createJsonResponse(taskDetails, QHttpServerResponse::StatusCode::InternalServerError);
    }

    // УСПІХ: Повертаємо деталі завдання
    logDebug() << "Successfully fetched details for Export Task ID:" << id;
    return createJsonResponse(taskDetails, QHttpServerResponse::StatusCode::Ok);
}

/**
 * @brief Обробляє POST-запит для створення нового завдання на експорт.
 * Маршрут: POST /api/export-tasks
 * @return QHttpServerResponse з новим ID (201 Created) або помилкою.
 */
QHttpServerResponse WebServer::handleCreateExportTaskRequest(const QHttpServerRequest &request)
{
    logRequest(request);
    User* user = authenticateRequest(request);

    // 1. Аутентифікація та авторизація
    if (!user) {
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }
    // Перевіряємо, чи є користувач адміністратором
    if (!user->isAdmin()) {
        delete user;
        return createJsonResponse(QJsonObject{{"error", "Forbidden. Requires Administrator role"}}, QHttpServerResponse::StatusCode::Forbidden);
    }
    delete user;

    // 2. Парсинг тіла запиту (JSON)
    QJsonDocument doc = QJsonDocument::fromJson(request.body());
    if (!doc.isObject()) {
        return createJsonResponse(QJsonObject{{"error", "Invalid JSON body"}}, QHttpServerResponse::StatusCode::BadRequest);
    }

    QJsonObject taskData = doc.object();

    // 3. Валідація полів (мінімальна)
    if (!taskData.contains("task_name") ||
        !taskData.contains("sql_template") ||
        !taskData.contains("query_filename") ||
        !taskData.contains("is_active") ||
        !taskData.contains("match_fields") || // <-- ДОДАНО
        !taskData.contains("target_table")) {
        return createJsonResponse(QJsonObject{{"error", "Missing required fields for create (task_name, sql_template, query_filename, is_active)"}},
                                  QHttpServerResponse::StatusCode::BadRequest);
    }

    // 4. Виклик створення в БД
    // Очікуємо newId > 0, оскільки це створення
    int newId = DbManager::instance().createExportTask(taskData);

    // 5. Формування відповіді
    if (newId > 0) {
        // УСПІХ: Повертаємо новий ID (HTTP 201 Created)
        logInfo() << "Successfully created new Export Task with ID:" << newId;
        return createJsonResponse({{"status", "success"}, {"task_id", newId}}, QHttpServerResponse::StatusCode::Created);
    } else {
        // Помилка DB Manager
        qCritical() << "Failed to create Export Task. Details:" << DbManager::instance().lastError();
        return createJsonResponse(QJsonObject{{"error", "Failed to create task in database. Details: " + DbManager::instance().lastError()}},
                                  QHttpServerResponse::StatusCode::InternalServerError);
    }
}

QHttpServerResponse WebServer::handleUpdateExportTaskRequest(const QString &taskId, const QHttpServerRequest &request)
{
    logRequest(request);
    User* user = authenticateRequest(request);

    // 1. АВТОРИЗАЦІЯ
    if (!user || !user->isAdmin()) {
        delete user;
        return createJsonResponse(QJsonObject{{"error", "Forbidden. Requires Administrator role"}}, QHttpServerResponse::StatusCode::Forbidden);
    }
    delete user;

    // 2. ВАЛІДАЦІЯ ID
    int id = taskId.toInt();
    if (id <= 0) {
        return createJsonResponse(QJsonObject{{"error", "Invalid TASK_ID for update (must be > 0)"}}, QHttpServerResponse::StatusCode::BadRequest);
    }

    // 3. ПАРСИНГ ТІЛА ЗАПИТУ
    QJsonDocument doc = QJsonDocument::fromJson(request.body());
    if (!doc.isObject()) {
        return createJsonResponse(QJsonObject{{"error", "Invalid JSON body"}}, QHttpServerResponse::StatusCode::BadRequest);
    }

    QJsonObject taskData = doc.object();

    // 4. ВАЛІДАЦІЯ ПОЛІВ
    if (!taskData.contains("task_name") ||
        !taskData.contains("sql_template") ||
        !taskData.contains("query_filename") ||
        !taskData.contains("is_active") ||
        !taskData.contains("match_fields") ||
        !taskData.contains("target_table")) { // <-- Варто додати сюди

        return createJsonResponse(QJsonObject{{"error", "Missing required fields"}}, QHttpServerResponse::StatusCode::BadRequest);
    }

    // 5. ВИКЛИК ОНОВЛЕННЯ
    if (DbManager::instance().updateExportTask(id, taskData)) {
        // УСПІХ: Повертаємо ID та статус 200 OK
        logInfo() << "Successfully updated Export Task ID:" << id;
        return createJsonResponse(QJsonObject{{"status", "success"}, {"task_id", id}}, QHttpServerResponse::StatusCode::Ok);
    } else {
        // Помилка DB Manager
        qCritical() << "Failed to update Export Task ID" << id << ". Details:" << DbManager::instance().lastError();
        return createJsonResponse(QJsonObject{{"error", "Failed to update task in database. Details: " + DbManager::instance().lastError()}},
                                  QHttpServerResponse::StatusCode::InternalServerError);
    }
}

/**
 * @brief Обробляє запит на отримання даних для дашборду (список клієнтів зі статусами).
 * Маршрут: GET /api/dashboard
 */
QHttpServerResponse WebServer::handleDashboardRequest(const QHttpServerRequest &request)
{
    // 1. Перевірка авторизації (Gandalf повинен надсилати токен)
    User* user = authenticateRequest(request);
    if (!user) {
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }

    // 2. Отримуємо дані з DbManager
    QJsonArray data = DbManager::instance().getDashboardData();

    // 3. Віддаємо результат
    return createJsonResponse(data, QHttpServerResponse::StatusCode::Ok);
}


QHttpServerResponse WebServer::handleGetStationPosData(const QString& clientId, const QString& terminalNo, const QHttpServerRequest& request)
{
    // Ваша функція authenticateRequest сама розбереться, хто це (Бот чи Гандальф)
    User* user = authenticateRequest(request);

    if (!user) {
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }

    // Якщо дійшли сюди - користувач авторизований.
    // Отримуємо дані:
    QJsonArray data = DbManager::instance().getPosDataByTerminal(clientId.toInt(), terminalNo.toInt());

    // Важливо: authenticateRequest повертає об'єкт, який треба видалити (згідно коментаря у вашому коді)
    delete user;

    return createJsonResponse(data, QHttpServerResponse::StatusCode::Ok);
}


QHttpServerResponse WebServer::handleGetStationTanks(const QString& clientId, const QString& terminalNo, const QHttpServerRequest& request)
{
    // 1. Універсальна авторизація (Бот або User)
    User* user = authenticateRequest(request);
    if (!user) {
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }

    // 2. Отримуємо дані
    QJsonArray data = DbManager::instance().getTanksByTerminal(clientId.toInt(), terminalNo.toInt());

    // 3. Прибираємо за собою
    delete user;

    return createJsonResponse(data, QHttpServerResponse::StatusCode::Ok);
}

// WebServer.cpp (Додайте після handleGetStationTanks)

/**
 * @brief Обробляє запит на отримання конфігурації ТРК та Пістолетів.
 * Маршрут: GET /api/clients/<clientId>/station/<terminalNo>/dispensers
 */
QHttpServerResponse WebServer::handleGetStationDispensers(const QString& clientId, const QString& terminalNo, const QHttpServerRequest& request)
{
    // 1. Універсальна авторизація (Бот або User)
    User* user = authenticateRequest(request);
    if (!user) {
        return createJsonResponse(QJsonObject{{"error", "Unauthorized"}}, QHttpServerResponse::StatusCode::Unauthorized);
    }

    // 2. Отримуємо дані з DbManager
    QJsonArray data = DbManager::instance().getDispenserConfigByTerminal(clientId.toInt(), terminalNo.toInt());

    delete user;

    // 3. Повертаємо масив
    return createJsonResponse(data, QHttpServerResponse::StatusCode::Ok);
}


// Conduit/WebServer.cpp

// ... (після всіх існуючих обробників запитів)

/**
 * @brief Обробляє запит бота на отримання списку відкритих Redmine задач.
 * Маршрут: GET /api/bot/redmine/tasks
 */
QHttpServerResponse WebServer::handleGetRedmineTasks(const QHttpServerRequest& request)
{
    // --- 1. АУТЕНТИФІКАЦІЯ ---
    // Аутентифікація повертає User* або nullptr
    User* user = authenticateRequest(request);
    if (!user) {
        return createJsonResponse(QJsonObject{{"error", "Unauthorized or invalid credentials."}},
                                  QHttpServerResponse::StatusCode::Unauthorized);
    }

    // !!! ДЕБАГ !!!
    logDebug() << "Handling Redmine Tasks request for user ID:" << user->id();

    // --- 2. ДЕШИФРУВАННЯ ТОКЕНА ---
    QString encryptedToken = user->redmineToken(); // Отримуємо зашифрований токен

    if (encryptedToken.isEmpty()) {
        delete user;
        return createJsonResponse(QJsonObject{{"error", "Redmine API token not configured for this user."}},
                                  QHttpServerResponse::StatusCode::BadRequest);
    }

    // !!! ВИКЛИК КРИПТОГРАФІЇ ПЕРЕД ВИКОРИСТАННЯМ !!!
    QString decryptedToken = CriptPass::instance().decriptPass(encryptedToken);

    // --- 3. НАЛАШТУВАННЯ ---
    QString redmineUrl = AppParams::instance().getParam("Global", "RedmineBaseUrl").toString();

    if (redmineUrl.isEmpty()) {
        delete user;
        return createJsonResponse(QJsonObject{{"error", "Redmine Base URL is not configured in application settings."}},
                                  QHttpServerResponse::StatusCode::InternalServerError);
    }

    // --- 4. ВИКЛИК ЗОВНІШНЬОГО КЛІЄНТА (Синхронний виклик) ---
    QJsonArray tasksArray;
    ApiError clientError;

    // Створюємо клієнт для виконання запиту
    RedmineClient client;
    QNetworkReply* reply = client.fetchOpenIssues(redmineUrl, decryptedToken);

    if (reply) {
        // !!! Блокування та очікування відповіді Redmine !!!
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        // Перевіряємо, чи був успішно випущений сигнал issuesFetched
        if (reply->property("issuesFetched").toBool()) {
            // Якщо issuesFetched спрацював, масив задач зберігається у властивостях
            tasksArray = reply->property("issuesArray").toJsonArray();
            logInfo() << "Successfully retrieved" << tasksArray.count() << "tasks from Redmine.";
        } else {
            // Якщо issuesFetched НЕ спрацював, помилка була оброблена в onIssuesReplyFinished
            // Помилка вже знаходиться у властивості "errorDetails"
            clientError = reply->property("errorDetails").value<ApiError>();
            logCritical() << "Redmine API failed during sync call. Error:" << clientError.errorString;
        }

        // Оскільки reply вже буде видалено у RedmineClient::onIssuesReplyFinished,
        // тут ми просто продовжуємо.
        // Але нам потрібно тимчасово змінити RedmineClient.cpp, щоб він зберігав дані
        // у властивостях, а не просто емітував сигнал, бо тут ми синхронно чекаємо.
    } else {
        // Помилка вхідних параметрів (URL/токен порожні)
        clientError.errorString = "RedmineClient failed to initiate fetch (check URL/Token).";
        clientError.httpStatusCode = 500;
    }

    // --- 5. ФІНАЛЬНА ВІДПОВІДЬ ---
    delete user;

    if (tasksArray.isEmpty() && clientError.httpStatusCode != 0 && clientError.httpStatusCode != 404) {
        // Помилка, відмінна від "не знайдено"
        return createJsonResponse(QJsonObject{{"error", clientError.errorString}},
                                  (QHttpServerResponse::StatusCode)clientError.httpStatusCode);
    } else {
        // Успіх або 404/порожній список (що вважається успіхом для бота)
        return createJsonResponse(tasksArray, QHttpServerResponse::StatusCode::Ok);
    }
}

// WebServer.cpp
// ... (додайте після handleGetRedmineTasks)

/**
 * @brief Обробляє запит бота на отримання списку відкритих Jira задач.
 * Маршрут: GET /api/bot/jira/tasks
 */
// WebServer.cpp
// ... у WebServer::handleGetJiraTasks

QHttpServerResponse WebServer::handleGetJiraTasks(const QHttpServerRequest &request)
{
    logRequest(request);

    // --- 1. АУТЕНТИФІКАЦІЯ БОТА ---
    User* user = authenticateRequest(request);
    if (!user) {
        delete user;
        return createTextResponse("Unauthorized", QHttpServerResponse::StatusCode::Unauthorized);
    }

    if (user->telegramId() == 0) {
        logWarning() << "Jira tasks request received without valid Telegram ID.";
        delete user;
        return createTextResponse("Invalid bot request context (missing telegramId).", QHttpServerResponse::StatusCode::BadRequest);
    }

    // --- 2. ОТРИМАННЯ ПАРАМЕТРІВ ПІДКЛЮЧЕННЯ ---
    const QString jiraBaseUrl = AppParams::instance().getParam("Global", "JiraBaseUrl").toString();

    // ВИКОРИСТОВУЄМО КОРЕКТНИЙ МЕТОД З User.h: jiraToken()
    const QString jiraTokenEncrypted = user->jiraToken();

    // ВИКОРИСТОВУЄМО ЛОКАЛЬНИЙ ЛОГІН (login()) ДЛЯ ФІЛЬТРАЦІЇ В JQL
    const QString jiraLogin = user->login();

    if (jiraBaseUrl.isEmpty() || jiraTokenEncrypted.isEmpty() || jiraLogin.isEmpty()) {
        logWarning() << "Jira configuration (URL, login, or user token) is missing for user:" << user->telegramId();
        delete user;
        QJsonObject errorBody;
        errorBody["error"] = "Jira configuration (URL, login, or user token) is missing.";
        return createJsonResponse(errorBody, QHttpServerResponse::StatusCode::Forbidden);
    }

    // Розшифровуємо токен
    QString jiraUserToken;
    try {
        jiraUserToken = CriptPass::instance().decriptPass(jiraTokenEncrypted);
    } catch (const std::exception& e) {
        logCritical() << "Failed to decrypt Jira API Token for user" << user->id() << ". Error:" << e.what();
        delete user;
        QJsonObject errorBody;
        errorBody["error"] = "Failed to decrypt Jira API Token.";
        return createJsonResponse(errorBody, QHttpServerResponse::StatusCode::InternalServerError);
    }

    // --- 3. ЗАПИТ ДО ЗОВНІШНЬОГО API (JIRA) ---
    QJsonArray tasksArray;
    ApiError clientError;

    // Використовуємо існуючий JiraClient з Oracle/
    JiraClient jiraClient;

    // Викликаємо метод, який здійснює прямий запит до Jira API
    // Передаємо BaseUrl, Логін (для JQL) та Розшифрований Токен (для Bearer Auth)
    // !!! JIRA CLIENT ТЕПЕР ПОВИНЕН ФОРМУВАТИ POST-ЗАПИТ З BEARER TOKEN !!!
    QNetworkReply* reply = jiraClient.fetchIssues(jiraBaseUrl, jiraLogin, jiraUserToken);

    if (reply) {
        // Синхронне очікування відповіді
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        // Перевіряємо властивості, встановлені у JiraClient::onIssuesReplyFinished
        if (reply->property("success").toBool()) {
            tasksArray = reply->property("tasksArray").toJsonArray();
            logInfo() << "Successfully fetched" << tasksArray.count() << "tasks from Jira.";
        } else {
            // Помилка була оброблена в JiraClient
            clientError = reply->property("errorDetails").value<ApiError>();
            logCritical() << "Jira API failed during sync call. Error:" << clientError.errorString;
        }

        reply->deleteLater();
    } else {
        // Помилка ініціалізації запиту
        clientError.errorString = "JiraClient failed to initiate fetch (check URL/Token).";
        clientError.httpStatusCode = 500;
    }

    // --- 4. ФІНАЛЬНА ВІДПОВІДЬ ---
    delete user;

    if (tasksArray.isEmpty() && clientError.httpStatusCode != 0 && clientError.httpStatusCode != 404) {
        // Помилка
        return createJsonResponse(QJsonObject{{"error", clientError.errorString}},
                                  (QHttpServerResponse::StatusCode)clientError.httpStatusCode);
    } else {
        // Успіх (навіть якщо список завдань порожній)
        return createJsonResponse(tasksArray, QHttpServerResponse::StatusCode::Ok);
    }
}
