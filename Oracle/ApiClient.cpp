#include "ApiClient.h"
#include "AppParams.h"
#include "User.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

ApiClient& ApiClient::instance()
{
    static ApiClient self;
    return self;
}

ApiClient::ApiClient(QObject* parent) : QObject(parent)
{
    m_networkManager = new QNetworkAccessManager(this);
    m_serverUrl = AppParams::instance().API_BASE_URL;
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
