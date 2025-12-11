#include "JiraClient.h"
#include "Logger.h"
#include "ApiClient.h" // Для ApiError
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSslConfiguration> // Для ігнорування SSL
#include <QSslSocket>


JiraClient::JiraClient(QObject *parent) : QObject(parent)
{
    // Ініціалізуємо менеджер мережі
    m_networkManager = new QNetworkAccessManager(this);
}

QNetworkReply* JiraClient::fetchIssues(const QString& baseUrl, const QString& userLogin, const QString& userApiToken)
{
    if (baseUrl.isEmpty() || userLogin.isEmpty() || userApiToken.isEmpty()) {
        logCritical() << "JiraClient: Base URL, login, or API Key is empty.";
        // У цьому синхронному сценарії, просто повертаємо nullptr
        return nullptr;
    }

    // --- 1. ФІНАЛЬНИЙ JQL: ВИКОРИСТАННЯ currentUser() ---
    // Jira автоматично визначить користувача за Bearer Token.
    // Використовуємо точний стан Unresolved, як у вашому скріншоті.
    const QString jqlQuery = "assignee = currentUser() AND resolution = Unresolved ORDER BY updated DESC";

    // --- 2. Створення JSON-об'єкту з запитом ---
    QJsonObject jsonPayload;
    jsonPayload["jql"] = jqlQuery;
    jsonPayload["fields"] = QJsonArray({"summary", "status", "key", "project"});
    // !!! ВИДАЛЯЄМО: jsonPayload["maxResults"] = 10; !!! (бо це була тимчасова діагностика)
    // --- 3. Створення мережевого запиту ---
    QUrl url(baseUrl + "/rest/api/2/search");
    QNetworkRequest request(url);

    // 4. Встановлюємо заголовки для Bearer Token
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // !!! ВИКОРИСТОВУЄМО BEARER TOKEN (як у вашому прикладі) !!!
    request.setRawHeader("Authorization", ("Bearer " + userApiToken).toUtf8());

    // ВАЖЛИВО: Ігнорування SSL (для роботи з .local доменами або самопідписаними сертифікатами)
    QSslConfiguration sslConfig = request.sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);

    // --- 5. Відправляємо POST-запит ---
    QNetworkReply *reply = m_networkManager->post(request, QJsonDocument(jsonPayload).toJson());

    // Підключаємо наш слот обробки
    connect(reply, &QNetworkReply::finished, this, &JiraClient::onIssuesReplyFinished);

    return reply;
}

/**
 * @brief Обробляє відповідь від Jira API та встановлює властивості для синхронного читання у WebServer.
 */
void JiraClient::onIssuesReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    // Використовуємо універсальний парсер помилок з ApiClient
    ApiError error = parseReply(reply);

    // Ініціалізуємо властивості для WebServer
    reply->setProperty("success", false);
    reply->setProperty("errorDetails", QVariant::fromValue(error));

    if (reply->error() == QNetworkReply::NoError && error.httpStatusCode == 200) {
        QJsonDocument doc = QJsonDocument::fromJson(error.responseBody);

        // Jira повертає об'єкт з масивом "issues"
        if (doc.isObject() && doc.object().contains("issues") && doc.object()["issues"].isArray()) {
            QJsonArray issues = doc.object()["issues"].toArray();

            // Встановлюємо властивості успіху
            reply->setProperty("success", true);
            reply->setProperty("tasksArray", issues);
        } else {
            // Помилка парсингу або невірний формат
            error.errorString = "Invalid response from Jira: expected issues array.";
            error.httpStatusCode = 500;
            reply->setProperty("errorDetails", QVariant::fromValue(error));
        }
    } else {
        // Помилка мережі або сервера Jira (4xx, 5xx)
        // errorDetails вже містить інформацію про помилку, отриману від parseReply
    }

    // !!! УВАГА: НЕ ВИКЛИКАЄМО reply->deleteLater() тут,
    // оскільки WebServer.cpp керує життєвим циклом відповіді через QEventLoop.

    // Емітуємо сигнал (для чистоти), хоча в WebServer.cpp ми читаємо властивості.
    if (reply->property("success").toBool()) {
        emit issuesFetched(reply->property("tasksArray").toJsonArray());
    } else {
        emit issuesFetchFailed(reply->property("errorDetails").value<ApiError>());
    }
}
