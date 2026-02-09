#ifndef JIRAWORKFLOWMANAGER_H
#define JIRAWORKFLOWMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QEventLoop>

// Підключаємо клієнта, бо менеджер буде використовувати його для запитів
#include "../Oracle/JiraClient.h"

class JiraWorkflowManager : public QObject
{
    Q_OBJECT
public:
    explicit JiraWorkflowManager(JiraClient* client, QObject *parent = nullptr);

    /**
     * @brief Головний метод: виконує безпечний перехід, автоматично шукаючи потрібні ID.
     * @return true, якщо успішно, false - якщо помилка (текст помилки запишеться в outError).
     */
    bool performSafeTransition(
        const QString& baseUrl,
        const QString& issueKey,
        const QString& userToken,
        const QString& actionType,       // "close" або "reject"
        const QString& resolutionMethod, // "visit" або "remote"
        const QString& comment,
        const QString& timeSpent,
        QString& outError
        );

private:
    JiraClient* m_client; // Посилання на наш існуючий JiraClient

    // Робить запит GET /issue/{key}/transitions?expand=transitions.fields
    // Повертає "сирий" JSON з відповіддю Jira або порожній об'єкт при помилці.
    QJsonObject fetchTransitionsMeta(const QString& baseUrl, const QString& issueKey, const QString& userToken, QString& outError);

    // Шукає ID переходу, назва якого містить одне з ключових слів
    QString findTransitionId(const QJsonObject& meta, const QStringList& keywords, QString& outNameFound);

    // Шукає ID опції всередині поля (наприклад, шукає "Візит" у полі "customfield_13102")
    QString findOptionId(const QJsonObject& fieldJson, const QStringList& keywords);

    // Відправляє фінальний POST запит (Transition)
    bool sendTransition(const QString& url, const QString& userToken, const QJsonObject& payload, QString& outError);
};

#endif // JIRAWORKFLOWMANAGER_H
