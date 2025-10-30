#ifndef TELEGRAMCLIENT_H
#define TELEGRAMCLIENT_H

#include <QObject>
#include <QString>
#include <QJsonArray>

class QNetworkAccessManager;

class TelegramClient : public QObject
{
    Q_OBJECT
public:
    explicit TelegramClient(const QString& token, QObject *parent = nullptr);
    void getUpdates(qint64 offset = 0);
    void sendMessage(qint64 chatId, const QString& text);

signals:
    void updatesReceived(const QJsonArray& updates);
    void errorOccurred(const QString& errorString);

private slots:
    void onGetUpdatesFinished();

private:
    const QString m_apiUrl;
    QNetworkAccessManager* m_networkManager;
};

#endif // TELEGRAMCLIENT_H
