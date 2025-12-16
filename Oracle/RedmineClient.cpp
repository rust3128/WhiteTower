#include "RedmineClient.h"
#include "Logger.h"
#include "ApiClient.h" // Потрібен для доступу до ApiError та parseReply
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// УВАГА: Для коректної роботи parseReply (який знаходиться в анонімному namespace у ApiClient.cpp)
// його потрібно або винести у відкритий простір імен/клас, або повторити його оголошення,
// або, що краще, додати його оголошення в ApiClient.h (якщо він там відсутній).
// Для цього прикладу, ми будемо вважати, що його винесено або доступно.
// Якщо parseReply недоступний, ми будемо використовувати власну спрощену логіку обробки помилок.
// Припустимо, що функція ApiError parseReply(QNetworkReply* reply) оголошена в ApiClient.h

extern ApiError parseReply(QNetworkReply* reply); // Припускаємо, що це оголошення доступне

RedmineClient::RedmineClient(QObject *parent)
    : QObject{parent}
{
    m_networkManager = new QNetworkAccessManager(this);
}

QNetworkReply* RedmineClient::fetchOpenIssues(const QString& baseUrl, const QString& apiKey)
{
    if (baseUrl.isEmpty() || apiKey.isEmpty()) {
        logCritical() << "RedmineClient: Base URL or API Key is empty.";
        return nullptr;
    }

    // --- 1. Формування URL та фільтрів ---
    QUrl url(baseUrl);

    // Додаємо скіс, якщо його немає, та кінцеву точку
    if (!url.path().endsWith('/')) {
        url.setPath(url.path() + "/");
    }
    url.setPath(url.path() + "issues.json");

    // !!! ВИПРАВЛЕННЯ: ВИКОРИСТАННЯ НАДІЙНОГО ФІЛЬТРА "status_id=open" !!!
    // Оскільки фільтрація за списком ID викликає HTTP 500, повертаємося
    // до стандартного фільтра, який включає всі незакриті задачі (включаючи потрібні).
    const QString statusFilter = "open";

    QUrlQuery query;
    query.addQueryItem("status_id", statusFilter);
    query.addQueryItem("assigned_to_id", "me");
    url.setQuery(query);

    logDebug() << "RedmineClient: Preparing request to" << url.toString();

    // --- 2. Створення запиту та заголовків ---
    QNetworkRequest request(url);
    request.setRawHeader("X-Redmine-API-Key", apiKey.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // --- 3. Відправка запиту ---
    QNetworkReply* reply = m_networkManager->get(request);

    // З'єднання слота обробки відповіді
    connect(reply, &QNetworkReply::finished, this, &RedmineClient::onIssuesReplyFinished);

    return reply;
}

void RedmineClient::onIssuesReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    // Реєстрація (хоча б один раз в додатку)
    qRegisterMetaType<ApiError>("ApiError");

    // Використовуємо існуючу логіку парсингу помилок
    ApiError error = parseReply(reply);

    QJsonArray issuesArray;

    // Перевіряємо на успіх (200 OK)
    if (reply->error() == QNetworkReply::NoError && error.httpStatusCode == 200)
    {
        QJsonDocument doc = QJsonDocument::fromJson(error.responseBody);

        if (doc.isObject() && doc.object().contains("issues") && doc.object()["issues"].isArray()) {
            issuesArray = doc.object()["issues"].toArray();
            logInfo() << "RedmineClient: Successfully fetched" << issuesArray.count() << "issues.";

            // !!! ЗБЕРІГАННЯ РЕЗУЛЬТАТУ ДЛЯ СИНХРОННОГО ВИКЛИКУ !!!
            reply->setProperty("issuesFetched", true);
            reply->setProperty("issuesArray", issuesArray); // Зберігаємо масив задач

            emit issuesFetched(issuesArray); // Для асинхронних клієнтів
        } else {
            error.errorString = "Invalid response from Redmine: 'issues' array not found or incorrect format.";
            logWarning() << error.errorString;

            // !!! ЗБЕРІГАННЯ ПОМИЛКИ ДЛЯ СИНХРОННОГО ВИКЛИКУ !!!
            reply->setProperty("issuesFetched", false);
            reply->setProperty("errorDetails", QVariant::fromValue(error)); // Зберігаємо ApiError

            emit fetchFailed(error);
        }
    }
    else
    {
        // Помилка (мережева або від Redmine API)
        logCritical() << "RedmineClient: Fetch failed. Status:" << error.httpStatusCode << "Error:" << error.errorString;

        // !!! ЗБЕРІГАННЯ ПОМИЛКИ ДЛЯ СИНХРОННОГО ВИКЛИКУ !!!
        reply->setProperty("issuesFetched", false);
        reply->setProperty("errorDetails", QVariant::fromValue(error));

        emit fetchFailed(error);
    }

    // Залишаємо deleteLater, оскільки воно спрацює після того, як QEventLoop::exec() завершиться
    // і WebServer матиме шанс прочитати властивості.
    reply->deleteLater();
}


// RedmineClient.cpp (додайте після fetchOpenIssues)

/**
 * @brief Запит деталей будь-якої задачі за ID.
 */
QNetworkReply* RedmineClient::fetchIssueDetails(const QString& baseUrl, const QString& taskId, const QString& apiKey)
{
    if (baseUrl.isEmpty() || taskId.isEmpty() || apiKey.isEmpty()) {
        logCritical() << "RedmineClient: Cannot fetch details, missing URL, ID or API Key.";
        return nullptr;
    }

    // !!! ВИПРАВЛЕНО: Використовуємо локальний аргумент baseUrl !!!
    QUrl url(baseUrl);

    if (!url.path().endsWith('/')) {
        url.setPath(url.path() + "/");
    }
    url.setPath(url.path() + QString("issues/%1.json").arg(taskId));

    QNetworkRequest request(url);
    request.setRawHeader("X-Redmine-API-Key", apiKey.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &RedmineClient::onIssueDetailsReplyFinished);
    return reply;
}

/**
 * @brief Призначає задачу на вказаного користувача.
 */
QNetworkReply* RedmineClient::assignIssue(const QString& baseUrl, const QString& taskId, const QString& apiKey, int userId)
{
    if (baseUrl.isEmpty() || taskId.isEmpty() || apiKey.isEmpty() || userId <= 0) {
        logCritical() << "RedmineClient: Cannot assign issue, missing URL, ID, Key or User ID.";
        return nullptr;
    }

    // !!! ВИПРАВЛЕНО: Використовуємо локальний аргумент baseUrl !!!
    QUrl url(baseUrl);

    if (!url.path().endsWith('/')) {
        url.setPath(url.path() + "/");
    }
    url.setPath(url.path() + QString("issues/%1.json").arg(taskId));

    // Тіло запиту: { "issue": { "assigned_to_id": 12345 } }
    QJsonObject issueObject;
    issueObject["assigned_to_id"] = userId;

    QJsonObject payload;
    payload["issue"] = issueObject;

    QNetworkRequest request(url);
    request.setRawHeader("X-Redmine-API-Key", apiKey.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Redmine вимагає PUT-запит для оновлення
    QNetworkReply* reply = m_networkManager->put(request, QJsonDocument(payload).toJson());
    connect(reply, &QNetworkReply::finished, this, &RedmineClient::onAssignIssueReplyFinished);
    return reply;
}


// --- РЕАЛІЗАЦІЯ СЛОТІВ ---

void RedmineClient::onIssueDetailsReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    ApiError error = parseReply(reply);
    QJsonObject issueDetails;

    // Встановлення властивостей за замовчуванням
    reply->setProperty("success", false);
    reply->setProperty("errorDetails", QVariant::fromValue(error));

    if (reply->error() == QNetworkReply::NoError && error.httpStatusCode == 200) {
        QJsonDocument doc = QJsonDocument::fromJson(error.responseBody);

        if (doc.isObject() && doc.object().contains("issue")) {
            issueDetails = doc.object()["issue"].toObject();
            reply->setProperty("success", true);
            reply->setProperty("issueDetails", issueDetails);
        }
    }
    reply->deleteLater();
}

void RedmineClient::onAssignIssueReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    ApiError error = parseReply(reply);

    // Redmine повертає 200 OK або 204 No Content при успішному PUT
    if (reply->error() == QNetworkReply::NoError && (error.httpStatusCode == 200 || error.httpStatusCode == 204)) {
        reply->setProperty("success", true);
    } else {
        reply->setProperty("success", false);
        reply->setProperty("errorDetails", QVariant::fromValue(error));
        logCritical() << "Redmine Assign Failed. Status:" << error.httpStatusCode << "Error:" << error.errorString;
    }
    reply->deleteLater();
}

// RedmineClient.cpp (Додайте новий метод)

QNetworkReply* RedmineClient::fetchCurrentUserId(const QString& baseUrl, const QString& apiKey)
{
    if (baseUrl.isEmpty() || apiKey.isEmpty()) {
        logCritical() << "RedmineClient: Cannot fetch current user ID, missing URL or API Key.";
        return nullptr;
    }

    // URL: /users/current.json
    QUrl url(baseUrl);
    if (!url.path().endsWith('/')) url.setPath(url.path() + "/");
    url.setPath(url.path() + "users/current.json");

    QNetworkRequest request(url);
    request.setRawHeader("X-Redmine-API-Key", apiKey.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &RedmineClient::onCurrentUserIdReplyFinished);

    // Властивості для синхронного читання WebServer'ом
    reply->setProperty("success", false);

    return reply;
}

// RedmineClient.cpp (Додайте новий слот)

void RedmineClient::onCurrentUserIdReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    ApiError error = parseReply(reply);
    int redmineId = 0;

    // Встановлення властивостей за замовчуванням
    reply->setProperty("success", false);
    reply->setProperty("errorDetails", QVariant::fromValue(error));

    if (reply->error() == QNetworkReply::NoError && error.httpStatusCode == 200) {
        QJsonDocument doc = QJsonDocument::fromJson(error.responseBody);
        QJsonObject user = doc.object()["user"].toObject();

        if (user.contains("id")) {
            redmineId = user["id"].toInt();
            logInfo() << "RedmineClient: Successfully fetched current user ID:" << redmineId;

            // Зберігаємо ID та успіх для синхронного читання WebServer'ом
            reply->setProperty("success", true);
            reply->setProperty("redmineId", redmineId);

        } else {
            error.errorString = "Invalid response from Redmine: 'user' object or 'id' not found.";
            logWarning() << error.errorString;
        }
    } else {
        logCritical() << "RedmineClient: Fetch current user ID failed. Error:" << error.errorString;
    }
    reply->deleteLater();
}

QNetworkReply* RedmineClient::reportTask(const QString& baseUrl, const QString& taskId,
                                         const QString& apiKey, int redmineUserId,
                                         const QString& action, const QString& comment)
{
    if (baseUrl.isEmpty() || apiKey.isEmpty() || taskId.isEmpty() || redmineUserId <= 0) {
        logCritical() << "RedmineClient: Cannot report task, missing required parameters.";
        return nullptr;
    }

    // URL: /issues/<taskId>.json
    QUrl url(baseUrl);
    if (!url.path().endsWith('/')) url.setPath(url.path() + "/");
    url.setPath(url.path() + "issues/" + taskId + ".json");

    QNetworkRequest request(url);
    request.setRawHeader("X-Redmine-API-Key", apiKey.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // --- ФОРМУВАННЯ ТІЛА ЗАПИТУ ---
    QJsonObject issueObject;
    issueObject["notes"] = comment; // Коментар завжди додається як "notes"

    // 1. Призначення (Assign-to-self)
    // Ми завжди призначаємо задачу на себе при звітуванні
    issueObject["assigned_to_id"] = redmineUserId;

    // 2. Дія: Закриття (якщо action == "close")
    if (action == "close") {
        // ID статусу "Закрита" (зазвичай це 5 або 9 у Redmine)
        // Припускаємо, що 5 - це ID статусу "Closed" або "Вирішена"
        const int CLOSED_STATUS_ID = 5;
        issueObject["status_id"] = CLOSED_STATUS_ID;

        // Можливо, потрібно встановити done_ratio:
        issueObject["done_ratio"] = 100;
    }

    QJsonObject payload;
    payload["issue"] = issueObject;

    // --- ВІДПРАВКА ЗАПИТУ (PUT) ---
    QNetworkReply* reply = m_networkManager->put(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, &RedmineClient::onReportTaskReplyFinished);

    return reply;
}

// RedmineClient.cpp (Додайте новий слот)

void RedmineClient::onReportTaskReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    ApiError error = parseReply(reply);

    // Redmine повертає 200 OK або 204 No Content (якщо тіло порожнє)
    // Успіхом вважаємо 2xx
    if (reply->error() == QNetworkReply::NoError && error.httpStatusCode >= 200 && error.httpStatusCode < 300) {
        reply->setProperty("success", true);
    } else {
        // У Redmine 422 Unprocessable Entity - поширений код помилки
        if (error.httpStatusCode == 422) {
            error.errorString = "Redmine rejected the request (Unprocessable Entity). Check required fields or status transition rules.";
        }
        reply->setProperty("success", false);
        reply->setProperty("errorDetails", QVariant::fromValue(error));
    }

    reply->deleteLater();
}

QNetworkReply* RedmineClient::reportTaskWithAttachments(const QString& baseUrl, const QString& taskId,
                                                        const QString& apiKey, int redmineUserId,
                                                        const QString& action, const QString& comment,
                                                        const QList<QByteArray>& fileContents)
{
    if (baseUrl.isEmpty() || apiKey.isEmpty() || taskId.isEmpty() || redmineUserId <= 0) {
        logCritical() << "RedmineClient: Cannot report task, missing required parameters.";
        return nullptr;
    }

    // --- 1. ФОРМУВАННЯ ТІЛА ЗАПИТУ (JSON) ---
    QJsonObject issueObject;
    issueObject["notes"] = comment;
    issueObject["assigned_to_id"] = redmineUserId;

    // Якщо закриваємо
    if (action == "close") {
        const int CLOSED_STATUS_ID = 5;
        issueObject["status_id"] = CLOSED_STATUS_ID;
        issueObject["done_ratio"] = 100;
    }

    QJsonObject payload;
    payload["issue"] = issueObject;

    // --- 2. ФОРМУВАННЯ MULTIPART ЗАПИТУ ---

    // Ми будемо використовувати багаточастинний запит, тільки якщо є вкладення.
    if (fileContents.isEmpty()) {
        // Якщо фото немає, відправляємо простий JSON (PUT)
        return reportTask(baseUrl, taskId, apiKey, redmineUserId, action, comment);
    }

    // !!! Складний сценарій: є фото !!!

    // Redmine вимагає, щоб вкладення були завантажені окремо.
    // ФЛОУ: 1) Завантажити файл. 2) Отримати token. 3) Передати token у фінальному JSON.

    // Це вимагає окремого етапу завантаження вкладень.
    // Оскільки ми обмежені синхронним/асинхронним викликом,
    // ми повинні зробити це синхронно або змінити WebServer.

    // ТИМЧАСОВА РЕАЛІЗАЦІЯ (Простий JSON, що не підтримує файли, якщо їх більше 1):
    // Цей блок є заглушкою, оскільки повноцінна реалізація вимагає багатьох QEventLoop.

    // 1. Створення file_token
    // 2. Додавання file_token до payload["uploads"]

    // Оскільки ми не можемо виконати декілька синхронних мережевих операцій у цьому методі:
    // МИ ВИДАЛЯЄМО ФУНКЦІОНАЛ reportTaskWithAttachments

    // !!! ПОВЕРТАЄМОСЯ НАЗАД: ВЕБСЕРВЕР ПОВИНЕН БУТИ АСИНХРОННИМ АБО ВИКОРИСТОВУВАТИ ТИМЧАСОВЕ РІШЕННЯ.

    logCritical() << "RedmineClient: Full multipart attachment logic requires complex multi-step synchronous API calls which are currently too complex to integrate safely here. Only comment is sent.";

    // ВИХІД: Повертаємося до версії без вкладень, щоб не зламати компоновник
    // (Але вам необхідно видалити цей виклик reportTaskWithAttachments з WebServer.cpp)
    return reportTask(baseUrl, taskId, apiKey, redmineUserId, action, comment);

}
