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

    if (statusCode == 200) { // OK
        User* user = User::fromJson(doc.object());
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
    QNetworkRequest request(QUrl(m_serverUrl + "/api/users"));
    // Тут можна додати токен авторизації в майбутньому
    // request.setRawHeader("Authorization", "Bearer ...");

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
    QNetworkRequest request((QUrl(url)));

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
