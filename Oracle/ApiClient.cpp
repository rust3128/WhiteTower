#include "ApiClient.h"
#include "AppParams.h"
#include "User.h"
#include "Logger.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

ApiClient& ApiClient::instance()
{
    static ApiClient self;
    return self;
}

ApiClient::ApiClient(QObject* parent) : QObject(parent)
{
    m_networkManager = new QNetworkAccessManager(this);
    // Тепер ми читаємо адресу з динамічних параметрів AppParams
    m_serverUrl = AppParams::instance().getParam("Global", "ApiBaseUrl").toString();

    if (m_serverUrl.isEmpty()) {
        // Логуємо критичну помилку, якщо URL не встановлено
        logCritical() << "API Base URL is not set in AppParams!";
    }
}

QNetworkRequest ApiClient::createAuthenticatedRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    if (!m_authToken.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + m_authToken).toUtf8());
    }
    return request;
}

void ApiClient::login(const QString& username)
{
    QJsonObject json;
    json["login"] = username;

    QNetworkRequest request(QUrl(m_serverUrl + "/api/login"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onLoginReplyFinished);
}

void ApiClient::onLoginReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        emit loginFailed("Network error: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (statusCode == 200 && doc.isObject()) {
        QJsonObject responseObj = doc.object();

        // === ЗМІНЕНО ТУТ: Зберігаємо токен ===
        if (responseObj.contains("token")) {
            m_authToken = responseObj["token"].toString();
        }

        User* user = User::fromJson(responseObj["user"].toObject());
        if (user) {
            emit loginSuccess(user);
        } else {
            emit loginFailed("Failed to parse user profile from server response.");
        }
    } else {
        QString errorMsg = "Unknown error";
        if (doc.isObject() && doc.object().contains("error")) {
            errorMsg = doc.object()["error"].toString();
        }
        emit loginFailed(QString("Login failed (HTTP %1): %2").arg(statusCode).arg(errorMsg));
    }

    reply->deleteLater();
}


void ApiClient::fetchAllUsers()
{
    QNetworkRequest request = createAuthenticatedRequest(QUrl(m_serverUrl + "/api/users"));
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onUsersReplyFinished);
}

void ApiClient::onUsersReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        emit usersFetchFailed(reply->errorString());
        reply->deleteLater();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isArray()) {
        emit usersFetchFailed("Invalid response from server: expected a JSON array.");
        reply->deleteLater();
        return;
    }

    emit usersFetched(doc.array());
    reply->deleteLater();
}

void ApiClient::fetchUserById(int userId)
{
    QString url = m_serverUrl + QString("/api/users/%1").arg(userId);
    // Створюємо запит з аутентифікацією
    QNetworkRequest request = createAuthenticatedRequest(QUrl(url));
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onUserDetailsReplyFinished);
}

void ApiClient::onUserDetailsReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        emit userDetailsFetchFailed(reply->errorString());
    } else {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isObject()) {
            emit userDetailsFetched(doc.object());
        } else {
            emit userDetailsFetchFailed("Invalid response from server: expected a JSON object.");
        }
    }
    reply->deleteLater();
}

void ApiClient::fetchAllRoles()
{
    QNetworkRequest request(QUrl(m_serverUrl + "/api/roles"));
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onRolesReplyFinished);
}

void ApiClient::onRolesReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        emit rolesFetchFailed(reply->errorString());
    } else {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isArray()) {
            emit rolesFetched(doc.array());
        } else {
            emit rolesFetchFailed("Invalid response from server: expected a JSON array of roles.");
        }
    }
    reply->deleteLater();
}


void ApiClient::updateUser(int userId, const QJsonObject& userData)
{
    QString url = m_serverUrl + QString("/api/users/%1").arg(userId);
    // Створюємо запит з аутентифікацією
    QNetworkRequest request = createAuthenticatedRequest(QUrl(url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_networkManager->put(request, QJsonDocument(userData).toJson());
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onUserUpdateReplyFinished);
}

void ApiClient::onUserUpdateReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() == QNetworkReply::NoError && (statusCode == 200 || statusCode == 204)) { // 204 No Content теж є успіхом
        emit userUpdateSuccess();
    } else {
        emit userUpdateFailed(QString("HTTP %1: ").arg(statusCode) + reply->readAll());
    }
    reply->deleteLater();
}

void ApiClient::fetchAllClients()
{
    QNetworkRequest request = createAuthenticatedRequest(QUrl(m_serverUrl + "/api/clients"));
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onClientsReplyFinished);
}

void ApiClient::onClientsReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        emit clientsFetchFailed(reply->errorString());
    } else {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isArray()) {
            emit clientsFetched(doc.array());
        } else {
            emit clientsFetchFailed("Invalid response from server: expected a JSON array of clients.");
        }
    }
    reply->deleteLater();
}

void ApiClient::createClient(const QString& clientName)
{
    QNetworkRequest request = createAuthenticatedRequest(QUrl(m_serverUrl + "/api/clients"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject json{{"client_name", clientName}};

    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onCreateClientReplyFinished);
}

void ApiClient::onCreateClientReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    // 1. Спочатку перевіряємо наявність мережевих помилок
    if (reply->error() != QNetworkReply::NoError) {
        emit clientCreateFailed("Network error: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    // 2. Отримуємо статус-код відповіді
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // 3. Перевіряємо, чи є статус-код успішним (201 Created)
    if (statusCode == 201) {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isObject()) {
            // Якщо все добре, відправляємо сигнал про успіх з даними нового клієнта
            emit clientCreateSuccess(doc.object());
        } else {
            emit clientCreateFailed("Invalid success response from server: expected a JSON object.");
        }
    } else {
        // Якщо статус-код інший, це помилка
        QString errorBody = reply->readAll();
        emit clientCreateFailed(QString("Failed to create client (HTTP %1): %2").arg(statusCode).arg(errorBody));
    }

    // 4. Очищуємо пам'ять
    reply->deleteLater();
}
