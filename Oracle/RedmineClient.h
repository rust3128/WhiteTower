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


    // Визначення всіх ID статусів Redmine для зручності супроводу
    enum class StatusId {
        New = 1,               // Новый (Новий)
        InDevelopment = 2,     // В разработке (В розробці)
        Testing = 3,           // Тестирование (Тестування)
        Feedback = 4,          // Обратная связь (Закритий)
        Closed = 5,            // Закрыт (Закритий)
        Rejected = 6,          // Отказ (Відмова)
        Deferred = 7           // Отложена (Відкладена)
    };

    /**
     * @brief Надсилає асинхронний запит до Redmine API для отримання відкритих задач
     * @param baseUrl Базовий URL Redmine (напр., http://redmine.mycompany.com)
     * @param apiKey РОЗШИФРОВАНИЙ ключ API користувача
     * @return QNetworkReply* (потрібен для синхронного очікування на сервері)
     */
    QNetworkReply* fetchOpenIssues(const QString& baseUrl, const QString& apiKey);

    /**
     * @brief Запит деталей будь-якої задачі за ID.
     * @param taskId ID задачі Redmine.
     * @param apiKey РОЗШИФРОВАНИЙ ключ API користувача.
     * @return QNetworkReply* (потрібен для синхронного очікування).
     */
    QNetworkReply* fetchIssueDetails(const QString& baseUrl, const QString& taskId, const QString& apiKey);

    /**
     * @brief Призначає задачу на вказаного користувача.
     * @param taskId ID задачі Redmine.
     * @param apiKey РОЗШИФРОВАНИЙ ключ API користувача.
     * @param userId ID користувача в Redmine (що має збігатися з нашим User ID).
     * @return QNetworkReply*.
     */
    QNetworkReply* assignIssue(const QString& baseUrl, const QString& taskId, const QString& apiKey, int userId);

    /**
     * @brief Отримує ID поточного користувача Redmine за API-ключем.
     * @param baseUrl Базовий URL Redmine.
     * @param apiKey РОЗШИФРОВАНИЙ ключ API користувача.
     * @return QNetworkReply* (потрібен для синхронного очікування).
     */
    QNetworkReply* fetchCurrentUserId(const QString& baseUrl, const QString& apiKey);

    /**
     * @brief Додає коментар або закриває задачу в Redmine, включаючи вкладення.
     * @param fileContents Список бінарних даних файлів.
     */
    QNetworkReply* reportTaskWithAttachments(const QString& baseUrl, const QString& taskId,
                                             const QString& apiKey, int redmineUserId,
                                             const QString& action, const QString& comment,
                                             const QList<QByteArray>& fileContents);

    /**
     * @brief Додає коментар або закриває задачу в Redmine.
     * @param baseUrl Базовий URL Redmine.
     * @param taskId ID задачі.
     * @param apiKey API-ключ користувача.
     * @param redmineUserId Числовий ID виконавця Redmine.
     * @param action Дія ("comment" або "close").
     * @param comment Текст коментаря/рішення.
     * @return QNetworkReply*
     */
    QNetworkReply* reportTask(const QString& baseUrl, const QString& taskId,
                              const QString& apiKey, int redmineUserId,
                              const QString& action, const QString& comment);

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

    void currentUserIdFetched(int redmineId, const QString& apiKey);
    void currentUserIdFetchFailed(const ApiError& error, const QString& apiKey);



private slots:
    void onIssuesReplyFinished();
    void onIssueDetailsReplyFinished();
    void onAssignIssueReplyFinished();
    void onCurrentUserIdReplyFinished();
    void onReportTaskReplyFinished();

private:
    QNetworkAccessManager* m_networkManager;
};

#endif // REDMINECLIENT_H
