#include "ApiClient.h"
#include "AppParams.h"
#include "User.h"
#include "Logger.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

// Використовуємо анонімний простір імен, щоб зробити цю функцію
// видимою тільки всередині цього файлу (це гарна практика).
namespace {
ApiError parseReply(QNetworkReply* reply)
{
    ApiError error;
    if (!reply) {
        error.errorString = "Internal error: QNetworkReply object is null.";
        return error;
    }

    error.requestUrl = reply->request().url().toString();
    error.httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    error.responseBody = reply->readAll(); // Читаємо тіло відповіді ОДИН РАЗ

    if (reply->error() != QNetworkReply::NoError) {
        // Мережева помилка (немає з'єднання, таймаут і т.д.)
        error.errorString = "Network error: " + reply->errorString();
    } else {
        // Сервер відповів, але це може бути помилка (4xx, 5xx)
        // Намагаємось розібрати JSON з тіла відповіді
        QJsonObject jsonObj = QJsonDocument::fromJson(error.responseBody).object();
        if (jsonObj.contains("error")) {
            error.errorString = jsonObj["error"].toString();
        } else if (jsonObj.contains("message")) {
            error.errorString = jsonObj["message"].toString();
        } else {
            error.errorString = "Unknown server error.";
        }
    }
    return error;
}
} // кінець анонімного простору імен


ApiClient& ApiClient::instance()
{
    static ApiClient self;
    return self;
}

ApiClient::ApiClient(QObject* parent) : QObject(parent)
{
    m_networkManager = new QNetworkAccessManager(this);
    // Тепер ми читаємо адресу з динамічних параметрів AppParams
    m_serverUrl = AppParams::instance().getParam("Global", "ApiBaseUrl").toString();

    if (m_serverUrl.isEmpty()) {
        // Логуємо критичну помилку, якщо URL не встановлено
        logCritical() << "API Base URL is not set in AppParams!";
    }
}

QNetworkRequest ApiClient::createAuthenticatedRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    if (!m_authToken.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + m_authToken).toUtf8());
    }
    return request;
}

void ApiClient::login(const QString& username)
{
    QJsonObject json;
    json["login"] = username;

    QNetworkRequest request(QUrl(m_serverUrl + "/api/login"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onLoginReplyFinished);
}

void ApiClient::onLoginReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    ApiError error;
    error.requestUrl = reply->request().url().toString();
    error.httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        error.errorString = "Network error: " + reply->errorString();
        emit loginFailed(error);
    } else if (error.httpStatusCode != 200) {
        QJsonObject jsonObj = QJsonDocument::fromJson(reply->readAll()).object();
        error.errorString = jsonObj["error"].toString("Unknown server error");
        emit loginFailed(error);
    } else {
        QJsonObject responseObj = QJsonDocument::fromJson(reply->readAll()).object();
        if (responseObj.contains("token")) {
            m_authToken = responseObj["token"].toString();
        }
        User* user = User::fromJson(responseObj["user"].toObject());
        if (user) {
            emit loginSuccess(user);
        } else {
            error.errorString = "Failed to parse user profile from server response.";
            emit loginFailed(error);
        }
    }
    reply->deleteLater();
}

void ApiClient::fetchAllUsers()
{
    QNetworkRequest request = createAuthenticatedRequest(QUrl(m_serverUrl + "/api/users"));
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onUsersReplyFinished);
}

void ApiClient::onUsersReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    ApiError error;
    error.requestUrl = reply->request().url().toString();
    error.httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        error.errorString = "Network error: " + reply->errorString();
        emit usersFetchFailed(error);
    } else if (error.httpStatusCode != 200) {
        QJsonObject jsonObj = QJsonDocument::fromJson(reply->readAll()).object();
        error.errorString = jsonObj["error"].toString("Unknown server error");
        emit usersFetchFailed(error);
    } else {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isArray()) {
            emit usersFetched(doc.array());
        } else {
            error.errorString = "Invalid response from server: expected a JSON array.";
            emit usersFetchFailed(error);
        }
    }
    reply->deleteLater();
}
void ApiClient::fetchUserById(int userId)
{
    QString url = m_serverUrl + QString("/api/users/%1").arg(userId);
    // Створюємо запит з аутентифікацією
    QNetworkRequest request = createAuthenticatedRequest(QUrl(url));
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onUserDetailsReplyFinished);
}

void ApiClient::onUserDetailsReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    ApiError error;
    error.requestUrl = reply->request().url().toString();
    error.httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();


    if (reply->error() != QNetworkReply::NoError) {
        error.errorString = "Network error: " + reply->errorString();
        emit userDetailsFetchFailed(error);
    } else if (error.httpStatusCode != 200) {
        QJsonObject jsonObj = QJsonDocument::fromJson(reply->readAll()).object();
        error.errorString = jsonObj["error"].toString("Unknown server error");
        emit userDetailsFetchFailed(error);
    } else {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isArray()) {
            emit userDetailsFetched(doc.object());
        } else {
            error.errorString = "Invalid response from server: expected a JSON array.";
            emit userDetailsFetchFailed(error);
        }
    }
    reply->deleteLater();

}

void ApiClient::fetchAllRoles()
{
    QNetworkRequest request(QUrl(m_serverUrl + "/api/roles"));
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onRolesReplyFinished);
}

void ApiClient::onRolesReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    ApiError error;
    error.requestUrl = reply->request().url().toString();
    error.httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        error.errorString = "Network error: " + reply->errorString();
        emit rolesFetchFailed(error);
    } else if (error.httpStatusCode != 200) {
        QJsonObject jsonObj = QJsonDocument::fromJson(reply->readAll()).object();
        error.errorString = jsonObj["error"].toString("Unknown server error");
        emit rolesFetchFailed(error);
    } else {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isArray()) {
            emit rolesFetched(doc.array());
        } else {
            error.errorString = "Invalid response from server: expected a JSON array.";
            emit rolesFetchFailed(error);
        }
    }
    reply->deleteLater();
}


void ApiClient::updateUser(int userId, const QJsonObject& userData)
{
    QString url = m_serverUrl + QString("/api/users/%1").arg(userId);
    // Створюємо запит з аутентифікацією
    QNetworkRequest request = createAuthenticatedRequest(QUrl(url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_networkManager->put(request, QJsonDocument(userData).toJson());
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onUserUpdateReplyFinished);
}

void ApiClient::onUserUpdateReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    // 1 & 2: Створюємо об'єкт помилки та заповнюємо базові дані
    ApiError error;
    error.requestUrl = reply->request().url().toString();
    error.httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // Перевіряємо на успіх (відсутність мережевих помилок І правильний статус-код)
    if (reply->error() == QNetworkReply::NoError && (error.httpStatusCode == 200 || error.httpStatusCode == 204))
    {
        // 4. Успіх: відправляємо відповідний сигнал
        emit userUpdateSuccess();
    }
    else
    {
        // 3. Помилка: заповнюємо деталі і відправляємо сигнал про помилку
        if (reply->error() != QNetworkReply::NoError) {
            // Це мережева помилка (напр., сервер недоступний)
            error.errorString = "Network error: " + reply->errorString();
        } else {
            // Це помилка від сервера (напр., 404 Not Found, 500 Internal Error)
            // Читаємо тіло відповіді, щоб отримати текст помилки від сервера
            QJsonObject jsonObj = QJsonDocument::fromJson(reply->readAll()).object();
            error.errorString = jsonObj["error"].toString("Unknown server error from body");
        }
        emit userUpdateFailed(error);
    }

    reply->deleteLater();
}

void ApiClient::fetchAllClients()
{
    QNetworkRequest request = createAuthenticatedRequest(QUrl(m_serverUrl + "/api/clients"));
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onClientsReplyFinished);
}

void ApiClient::onClientsReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    // 1 & 2: Створюємо об'єкт помилки та заповнюємо базові дані
    ApiError error;
    error.requestUrl = reply->request().url().toString();
    error.httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // Перевіряємо на успіх (відсутність мережевих помилок І статус-код 200 ОК)
    if (reply->error() == QNetworkReply::NoError && error.httpStatusCode == 200)
    {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isArray()) {
            // 4. Успіх: тіло відповіді - це масив, як і очікувалося
            emit clientsFetched(doc.array());
        } else {
            // 3. Помилка: сервер повернув 200 ОК, але тіло відповіді - не масив
            error.errorString = "Invalid response from server: expected a JSON array.";
            emit clientsFetchFailed(error);
        }
    }
    else
    {
        // 3. Помилка: або мережева, або від сервера
        if (reply->error() != QNetworkReply::NoError) {
            // Мережева помилка
            error.errorString = "Network error: " + reply->errorString();
        } else {
            // Помилка, яку повернув сервер (напр. 401, 404, 500)
            QJsonObject jsonObj = QJsonDocument::fromJson(reply->readAll()).object();
            error.errorString = jsonObj["error"].toString("Unknown server error");
        }
        emit clientsFetchFailed(error);
    }

    reply->deleteLater();
}

void ApiClient::createClient(const QJsonObject& clientData)
{
    QNetworkRequest request = createAuthenticatedRequest(QUrl(m_serverUrl + "/api/clients"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Тепер ми відправляємо повний об'єкт, а не тільки ім'я
    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(clientData).toJson());
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onCreateClientReplyFinished);
}

void ApiClient::onCreateClientReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    // 1 & 2: Створюємо об'єкт помилки та заповнюємо базові дані
    ApiError error;
    error.requestUrl = reply->request().url().toString();
    error.httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // Перевіряємо на успіх (відсутність мережевих помилок І статус-код 201 Created)
    if (reply->error() == QNetworkReply::NoError && error.httpStatusCode == 201)
    {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isObject()) {
            // 4. Успіх: тіло відповіді - це об'єкт, як і очікувалося
            emit clientCreateSuccess(doc.object());
        } else {
            // 3. Помилка: сервер повернув 201, але тіло відповіді некоректне
            error.errorString = "Invalid success response from server: expected a JSON object.";
            emit clientCreateFailed(error);
        }
    }
    else
    {
        // 3. Помилка: або мережева, або від сервера
        if (reply->error() != QNetworkReply::NoError) {
            // Мережева помилка
            error.errorString = "Network error: " + reply->errorString();
        } else {
            // Помилка, яку повернув сервер (напр. 400, 404, 500)
            QJsonObject jsonObj = QJsonDocument::fromJson(reply->readAll()).object();
            error.errorString = jsonObj["error"].toString("Unknown server error");
        }
        emit clientCreateFailed(error);
    }

    reply->deleteLater();
}


void ApiClient::fetchClientById(int clientId)
{
    QNetworkRequest request = createAuthenticatedRequest(QUrl(m_serverUrl + QString("/api/clients/%1").arg(clientId)));
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onClientDetailsReplyFinished);
}

void ApiClient::onClientDetailsReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    // 1 & 2: Створюємо об'єкт помилки та заповнюємо базові дані
    ApiError error;
    error.requestUrl = reply->request().url().toString();
    error.httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // Перевіряємо на успіх (відсутність мережевих помилок І статус-код 200 ОК)
    if (reply->error() == QNetworkReply::NoError && error.httpStatusCode == 200)
    {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isObject()) {
            // 4. Успіх: тіло відповіді - це об'єкт, як і очікувалося
            emit clientDetailsFetched(doc.object());
        } else {
            // 3. Помилка: сервер повернув 200 ОК, але тіло відповіді - не об'єкт
            error.errorString = "Invalid response from server: expected a JSON object.";
            emit clientDetailsFetchFailed(error);
        }
    }
    else
    {
        // 3. Помилка: або мережева, або від сервера
        if (reply->error() != QNetworkReply::NoError) {
            // Мережева помилка
            error.errorString = "Network error: " + reply->errorString();
        } else {
            // Помилка, яку повернув сервер (напр. 401, 404, 500)
            QJsonObject jsonObj = QJsonDocument::fromJson(reply->readAll()).object();
            error.errorString = jsonObj["error"].toString("Unknown server error");
        }
        emit clientDetailsFetchFailed(error);
    }

    reply->deleteLater();
}

void ApiClient::fetchAllIpGenMethods()
{
    QNetworkRequest request = createAuthenticatedRequest(QUrl(m_serverUrl + "/api/ip-gen-methods"));
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onIpGenMethodsReplyFinished);
}

void ApiClient::onIpGenMethodsReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    ApiError error;
    error.requestUrl = reply->request().url().toString();
    error.httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() == QNetworkReply::NoError && error.httpStatusCode == 200)
    {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isArray()) {
            emit ipGenMethodsFetched(doc.array());
        } else {
            error.errorString = "Invalid response from server: expected a JSON array.";
            emit ipGenMethodsFetchFailed(error);
        }
    }
    else
    {
        if (reply->error() != QNetworkReply::NoError) {
            error.errorString = "Network error: " + reply->errorString();
        } else {
            QJsonObject jsonObj = QJsonDocument::fromJson(reply->readAll()).object();
            error.errorString = jsonObj["error"].toString("Unknown server error");
        }
        emit ipGenMethodsFetchFailed(error);
    }
    reply->deleteLater();
}


void ApiClient::testDbConnection(const QJsonObject& config)
{
    QNetworkRequest request = createAuthenticatedRequest(QUrl(m_serverUrl + "/api/connections/test"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(config).toJson());
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onConnectionTestReplyFinished);
}

void ApiClient::onConnectionTestReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    ApiError error;
    error.requestUrl = reply->request().url().toString();
    error.httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        error.errorString = "Network error: " + reply->errorString();
        emit connectionTestFailed(error);
    } else if (error.httpStatusCode != 200) {
        // Якщо статус не 200, це точно помилка
        QJsonObject jsonObj = QJsonDocument::fromJson(reply->readAll()).object();
        error.errorString = jsonObj["error"].toString("Non-200 status code received");
        emit connectionTestFailed(error);
    } else {
        // Статус 200 ОК, тепер аналізуємо тіло відповіді
        QJsonObject jsonObj = QJsonDocument::fromJson(reply->readAll()).object();
        if (jsonObj["status"] == "success") {
            // Успіх, відправляємо звичайний QString, як і раніше
            emit connectionTestSuccess(jsonObj["message"].toString());
        } else {
            // Логічна помилка, яку повернув сервер
            error.errorString = jsonObj["message"].toString("Connection test failed on server side.");
            emit connectionTestFailed(error);
        }
    }

    reply->deleteLater();
}

void ApiClient::updateClient(int clientId, const QJsonObject& clientData)
{
    QString url = m_serverUrl + QString("/api/clients/%1").arg(clientId);
    QNetworkRequest request = createAuthenticatedRequest(QUrl(url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Використовуємо метод PUT для оновлення
    QNetworkReply* reply = m_networkManager->put(request, QJsonDocument(clientData).toJson());
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onClientUpdateReplyFinished);
}

void ApiClient::onClientUpdateReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    ApiError error;
    error.requestUrl = reply->request().url().toString();
    error.httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // === ВИПРАВЛЕНО ТУТ: Правильна перевірка статусу для оновлення (200 або 204) ===
    // 201 (Created) - для створення, а 200 (OK) або 204 (No Content) - для оновлення.
    if (reply->error() == QNetworkReply::NoError && (error.httpStatusCode == 200 || error.httpStatusCode == 204)) {
        emit clientUpdateSuccess();
    } else {
        if (reply->error() != QNetworkReply::NoError) {
            error.errorString = "Network error: " + reply->errorString();
        } else {
            QJsonObject jsonObj = QJsonDocument::fromJson(reply->readAll()).object();
            error.errorString = jsonObj["error"].toString("Unknown server error");
        }

        // === ВИПРАВЛЕНО ТУТ: Викликаємо правильний сигнал ===
        emit clientUpdateFailed(error);
    }

    reply->deleteLater();
}


void ApiClient::fetchSettings(const QString& appName)
{
    QNetworkRequest request = createAuthenticatedRequest(QUrl(m_serverUrl + "/api/settings/" + appName));
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onSettingsReplyFinished);
}

void ApiClient::updateSettings(const QString& appName, const QVariantMap& settings)
{
    QNetworkRequest request = createAuthenticatedRequest(QUrl(m_serverUrl + "/api/settings/" + appName));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply* reply = m_networkManager->put(request, QJsonDocument::fromVariant(settings).toJson());
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onSettingsUpdateReplyFinished);
}

void ApiClient::onSettingsReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    ApiError error;
    error.requestUrl = reply->request().url().toString();
    error.httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() == QNetworkReply::NoError && error.httpStatusCode == 200)
    {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isObject()) {
            // Успіх: конвертуємо JSON-об'єкт в QVariantMap і відправляємо
            emit settingsFetched(doc.object().toVariantMap());
        } else {
            error.errorString = "Invalid response from server: expected a JSON object.";
            emit settingsFetchFailed(error);
        }
    }
    else
    {
        if (reply->error() != QNetworkReply::NoError) {
            error.errorString = "Network error: " + reply->errorString();
        } else {
            QJsonObject jsonObj = QJsonDocument::fromJson(reply->readAll()).object();
            error.errorString = jsonObj["error"].toString("Unknown server error");
        }
        emit settingsFetchFailed(error);
    }
    reply->deleteLater();
}

void ApiClient::onSettingsUpdateReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    ApiError error;
    error.requestUrl = reply->request().url().toString();
    error.httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // Для оновлення успіхом вважається 200 OK або 204 No Content
    if (reply->error() == QNetworkReply::NoError && (error.httpStatusCode == 200 || error.httpStatusCode == 204))
    {
        emit settingsUpdateSuccess();
    }
    else
    {
        if (reply->error() != QNetworkReply::NoError) {
            error.errorString = "Network error: " + reply->errorString();
        } else {
            QJsonObject jsonObj = QJsonDocument::fromJson(reply->readAll()).object();
            error.errorString = jsonObj["error"].toString("Unknown server error");
        }
        emit settingsUpdateFailed(error);
    }
    reply->deleteLater();
}


void ApiClient::syncClientObjects(int clientId)
{
    logInfo() << " Call syncClientObjects";

    QString url = m_serverUrl + QString("/api/clients/%1/sync").arg(clientId);
    QNetworkRequest request = createAuthenticatedRequest(QUrl(url));

    QNetworkReply* reply = m_networkManager->post(request, QByteArray());

    // Зберігаємо ID клієнта у властивості запиту, щоб отримати його в слоті
    reply->setProperty("clientId", clientId);

    connect(reply, &QNetworkReply::finished, this, &ApiClient::onSyncReplyFinished);
}

void ApiClient::onSyncReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    int clientId = reply->property("clientId").toInt();
    ApiError errorDetails = parseReply(reply); // Отримуємо повну структуру з помилкою

    if (reply->error() == QNetworkReply::NoError && errorDetails.httpStatusCode == 202) {
        // Успіх: сервер прийняв завдання
        QJsonObject body = QJsonDocument::fromJson(errorDetails.responseBody).object();

        // Створюємо тимчасовий об'єкт ApiError для передачі повідомлення про успіх
        ApiError successDetails;
        successDetails.errorString = body["status"].toString("Sync started.");

        emit syncRequestAcknowledged(clientId, true, successDetails);
    } else {
        // Помилка: сервер відхилив запит, передаємо всю структуру помилки
        emit syncRequestAcknowledged(clientId, false, errorDetails);
    }
    reply->deleteLater();
}


void ApiClient::fetchSyncStatus(int clientId)
{
    QString url = m_serverUrl + QString("/api/clients/%1/sync-status").arg(clientId);
    QNetworkRequest request = createAuthenticatedRequest(QUrl(url));

    QNetworkReply* reply = m_networkManager->get(request);
    reply->setProperty("clientId", clientId); // Зберігаємо ID для слота
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onSyncStatusReplyFinished);
}

void ApiClient::onSyncStatusReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    int clientId = reply->property("clientId").toInt();
    ApiError errorDetails = parseReply(reply);

    if (reply->error() == QNetworkReply::NoError) {
        emit syncStatusFetched(clientId, QJsonDocument::fromJson(errorDetails.responseBody).object());
    } else {
        emit syncStatusFetchFailed(clientId, errorDetails);
    }
    reply->deleteLater();
}
