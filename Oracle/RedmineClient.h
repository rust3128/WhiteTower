#ifndef REDMINECLIENT_H
#define REDMINECLIENT_H

#include <QObject>
#include <QJsonArray>
#include <QNetworkReply>

// Оголошуємо ApiError, оскільки він використовується в сигналах
struct ApiError;

class QNetworkAccessManager;

class RedmineClient : public QObject
{
    Q_OBJECT
public:
    explicit RedmineClient(QObject *parent = nullptr);

    /**
     * @brief Надсилає асинхронний запит до Redmine API для отримання відкритих задач
     * @param baseUrl Базовий URL Redmine (напр., http://redmine.mycompany.com)
     * @param apiKey РОЗШИФРОВАНИЙ ключ API користувача
     * @return QNetworkReply* (потрібен для синхронного очікування на сервері)
     */
    QNetworkReply* fetchOpenIssues(const QString& baseUrl, const QString& apiKey);

signals:
    /**
     * @brief Сигнал успішного отримання задач
     * @param issues Масив JSON з отриманими задачами
     */
    void issuesFetched(const QJsonArray& issues);

    /**
     * @brief Сигнал помилки
     * @param error Структура з деталями помилки
     */
    void fetchFailed(const ApiError& error);

private slots:
    void onIssuesReplyFinished();

private:
    QNetworkAccessManager* m_networkManager;
};

#endif // REDMINECLIENT_H
