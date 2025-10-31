#include "TelegramClient.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

TelegramClient::TelegramClient(const QString& token, QObject* parent)
    : QObject(parent), m_token(token)
{
    m_networkManager = new QNetworkAccessManager(this);
    m_pollingTimer = new QTimer(this);
    // Таймер буде викликати слот getUpdates кожні 2 секунди
    connect(m_pollingTimer, &QTimer::timeout, this, &TelegramClient::getUpdates);
}

void TelegramClient::startPolling()
{
    if (!m_pollingTimer->isActive()) {
        getUpdates(); // Робимо перший запит негайно
        m_pollingTimer->start(2000); // Потім запускаємо опитування кожні 2 секунди
    }
}

void TelegramClient::stopPolling()
{
    m_pollingTimer->stop();
}

// Приватний метод для запиту оновлень
void TelegramClient::getUpdates()
{
    QUrl url("https://api.telegram.org/bot" + m_token + "/getUpdates");
    QUrlQuery query;
    query.addQueryItem("offset", QString::number(m_lastUpdateId + 1));
    query.addQueryItem("timeout", "10"); // Час очікування на стороні Telegram
    url.setQuery(query);

    QNetworkRequest request(url);
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &TelegramClient::onUpdatesReplyFinished);
}

void TelegramClient::onUpdatesReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(reply->errorString());
    } else {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj["ok"].toBool()) {
                QJsonArray updates = obj["result"].toArray();
                if (!updates.isEmpty()) {
                    // Оновлюємо ID останнього повідомлення
                    m_lastUpdateId = updates.last().toObject()["update_id"].toVariant().toLongLong();
                    // І сповіщаємо "мозок" про нові події
                    emit updatesReceived(updates);
                }
            }
        }
    }
    reply->deleteLater();
}
