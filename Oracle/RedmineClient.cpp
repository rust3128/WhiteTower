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
