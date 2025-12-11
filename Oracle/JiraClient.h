#ifndef JIRACLIENT_H
#define JIRACLIENT_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>

// Для ApiError та QNetworkReply
#include "ApiClient.h"

class QNetworkAccessManager;

class JiraClient : public QObject
{
    Q_OBJECT
public:
    explicit JiraClient(QObject *parent = nullptr);

    /**
     * @brief Запитує задачі Jira для вказаного користувача.
     * @param baseUrl Базовий URL Jira.
     * @param userLogin Логін користувача (для формування Basic Auth/Bearer).
     * @param userApiToken API токен/пароль користувача (для аутентифікації).
     * @return QNetworkReply*, або nullptr у разі помилки конфігурації.
     */
    QNetworkReply* fetchIssues(const QString& baseUrl, const QString& userLogin, const QString& userApiToken);

signals:
    // Сигнал, що емітується при успішному отриманні задач
    void issuesFetched(const QJsonArray& tasks);
    // Сигнал помилки
    void issuesFetchFailed(const ApiError& error);

private slots:
    void onIssuesReplyFinished();

private:
    QNetworkAccessManager* m_networkManager;
};

#endif // JIRACLIENT_H
