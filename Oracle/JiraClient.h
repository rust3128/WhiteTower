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

    QNetworkReply* fetchIssueDetails(const QString& baseUrl, const QString& issueKey, const QString& userApiToken);

    QNetworkReply* searchIssuesByTerminal(const QString& baseUrl, int terminalID, const QString& userApiToken);

    /**
     * @brief Завантажує файл (вкладення) до задачі Jira.
     * @param baseUrl Базова адреса Jira (напр. https://jira.company.com)
     * @param issueKey Ключ задачі (напр. AZS-46937)
     * @param userApiToken Токен користувача (Bearer)
     * @param fileData Бінарні дані файлу
     * @param fileName Ім'я файлу
     */
    QNetworkReply* uploadAttachment(const QString& baseUrl, const QString& issueKey,
                                    const QString& userApiToken, const QByteArray& fileData,
                                    const QString& fileName);


    QNetworkReply* addComment(const QString& baseUrl, const QString& issueKey,
                              const QString& userApiToken, const QString& commentBody);

    /**
     * @brief Виконує перехід (зміну статусу) задачі.
     * @param payload JSON-об'єкт, що містить ID переходу, поля та (опціонально) коментар.
     */
    QNetworkReply* changeIssueStatus(const QString& baseUrl, const QString& issueKey,
                                     const QString& userApiToken, const QJsonObject& payload);

    /**
     * @brief Додає запис про витрачений час (Worklog).
     */
    QNetworkReply* addWorklog(const QString& baseUrl, const QString& issueKey,
                              const QString& userApiToken, const QString& timeSpent);

    QNetworkAccessManager* networkManager() const { return m_networkManager; }

signals:
    // Сигнал, що емітується при успішному отриманні задач
    void issuesFetched(const QJsonArray& tasks);
    // Сигнал помилки
    void issuesFetchFailed(const ApiError& error);

private slots:
    void onIssuesReplyFinished();
    void onIssueDetailsReplyFinished();

private:
    QNetworkAccessManager* m_networkManager;
};

#endif // JIRACLIENT_H
