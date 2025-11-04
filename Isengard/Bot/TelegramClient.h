#ifndef TELEGRAMCLIENT_H
#define TELEGRAMCLIENT_H

#include <QObject>
#include <QString>
#include <QJsonArray>

class QNetworkAccessManager;
class QTimer;

class TelegramClient : public QObject
{
    Q_OBJECT
public:
    explicit TelegramClient(const QString& token, QObject* parent = nullptr);

    void startPolling();
    void stopPolling();

    void sendMessage(qint64 chatId, const QString& text);
    void sendMessage(qint64 chatId, const QString& text, const QJsonObject& replyMarkup);

    void sendChatAction(qint64 chatId, const QString& action = "typing");

signals:
    void updatesReceived(const QJsonArray& updates);
    void errorOccurred(const QString& error);

private slots:
    void getUpdates();
    void onUpdatesReplyFinished();

private:
    QString m_token;
    qint64 m_lastUpdateId = 0;
    QNetworkAccessManager* m_networkManager;
    QTimer* m_pollingTimer;
};

#endif // TELEGRAMCLIENT_H
