#include "TelegramClient.h"
#include "Oracle/Logger.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

TelegramClient::TelegramClient(const QString &token, QObject *parent)
    : QObject{parent}
    , m_apiUrl(QString("https://api.telegram.org/bot%1").arg(token))
{
    m_networkManager = new QNetworkAccessManager(this);
}

void TelegramClient::getUpdates(qint64 offset)
{
    QUrl url(m_apiUrl + "/getUpdates");
    QUrlQuery query;
    query.addQueryItem("offset", QString::number(offset));
    query.addQueryItem("timeout", "10"); // Чекати на відповідь 10 секунд
    url.setQuery(query);

    QNetworkRequest request(url);
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &TelegramClient::onGetUpdatesFinished);
}

void TelegramClient::onGetUpdatesFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred("Network error: " + reply->errorString());
    } else {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isObject() && doc.object()["ok"].toBool()) {
            emit updatesReceived(doc.object()["result"].toArray());
        } else {
            emit errorOccurred("Telegram API error: " + doc.toJson());
        }
    }
    reply->deleteLater();
}

void TelegramClient::sendMessage(qint64 chatId, const QString &text)
{
    QUrl url(m_apiUrl + "/sendMessage");
    QJsonObject json;
    json["chat_id"] = chatId;
    json["text"] = text;

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Цей запит нас не дуже цікавить, тому відповідь не обробляємо
    m_networkManager->post(request, QJsonDocument(json).toJson());
}
