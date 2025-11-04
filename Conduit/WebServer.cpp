#include "WebServer.h"
#include "Oracle/Logger.h"
#include "Oracle/DbManager.h"
#include "version.h"
#include "Oracle/SessionManager.h"
#include "Oracle/ConfigManager.h"
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
 * @brief Перевіряє запит на аутентифікацію. (ВЕРСІЯ 3)
 * Розуміє два методи:
 * 1. Gandalf (GUI): Заголовок "Authorization: Bearer <session_token>"
 * 2. Isengard (Bot): Заголовки "X-Bot-Token: <secret_key>" + "X-Telegram-ID: <user_id>"
 * @return Об'єкт User* у разі успіху (вимагає delete), або nullptr.
 */
User* WebServer::authenticateRequest(const QHttpServerRequest &request)
{
    // --- ВИПРАВЛЕНО: Шукаємо всі заголовки за один прохід ---
    const auto &headers = request.headers();
    QByteArray authHeader;
    QByteArray botTokenHeader;
    QByteArray telegramIdHeader;

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

    // --- Спроба №2: Аутентифікація Isengard (токен бота) ---
    if (!botTokenHeader.isEmpty() && !telegramIdHeader.isEmpty()) {

        // 1. Перевіряємо секретний ключ бота (ВИПРАВЛЕНО: беремо з m_botApiKey)
        if (m_botApiKey.isEmpty() || m_botApiKey == "YOUR_DEFAULT_BOT_KEY_HERE") {
            logCritical() << "Bot API Key is not set on the server! Cannot authenticate bot.";
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
                    return user; // Успіх (Isengard)
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

    m_httpServer->route("/api/bot/approve", QHttpServerRequest::Method::Post, [this](const QHttpServerRequest& request) {
        return handleApproveBotRequest(request);
    });
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
