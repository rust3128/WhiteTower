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
#include <QUrlQuery>

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

void ApiClient::setBotApiKey(const QString& key)
{
    m_botApiKey = key;
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

    ApiError error = parseReply(reply); // Використовуємо наш універсальний парсер

    if (reply->error() == QNetworkReply::NoError && error.httpStatusCode == 200)
    {
        QJsonDocument doc = QJsonDocument::fromJson(error.responseBody);

        // --- ПОЧАТОК ЗМІН ---
        // Сервер повертає ОБ'ЄКТ користувача, а не МАСИВ
        if (doc.isObject()) {
            emit userDetailsFetched(doc.object());
        } else {
            // --- КІНЕЦЬ ЗМІН ---
            error.errorString = "Invalid response from server: expected a JSON object.";
            emit userDetailsFetchFailed(error);
        }
    }
    else
    {
        emit userDetailsFetchFailed(error);
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


void ApiClient::fetchObjects(const QVariantMap &filters)
{
    QUrl url(m_serverUrl + "/api/objects");

    // Цей код вже готовий для майбутньої фільтрації
    if (!filters.isEmpty()) {
        QUrlQuery query;
        for (auto it = filters.constBegin(); it != filters.constEnd(); ++it) {
            query.addQueryItem(it.key(), it.value().toString());
        }
        url.setQuery(query);
    }

    QNetworkRequest request = createAuthenticatedRequest(url);
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onObjectsReplyFinished);
}

void ApiClient::onObjectsReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    ApiError error = parseReply(reply);
    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(error.responseBody);
        if (doc.isObject() && doc.object().contains("objects")) {
            emit objectsFetched(doc.object()["objects"].toArray());
        } else {
            error.errorString = "Invalid response from server: 'objects' array not found.";
            emit objectsFetchFailed(error);
        }
    } else {
        emit objectsFetchFailed(error);
    }
    reply->deleteLater();
}

void ApiClient::fetchRegionsList()
{
    QNetworkRequest request = createAuthenticatedRequest(QUrl(m_serverUrl + "/api/regions-list"));
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onRegionsListReplyFinished);
}

void ApiClient::onRegionsListReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    ApiError error = parseReply(reply);
    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(error.responseBody);
        if (doc.isObject() && doc.object().contains("regions")) {
            QStringList regions;
            for (const auto& val : doc.object()["regions"].toArray()) {
                regions.append(val.toString());
            }
            emit regionsListFetched(regions);
        } else {
            error.errorString = "Invalid response from server: 'regions' array not found.";
            emit regionsListFetchFailed(error);
        }
    } else {
        emit regionsListFetchFailed(error);
    }
    reply->deleteLater();
}


void ApiClient::setServerUrl(const QString& url)
{
    if (!url.isEmpty()) {
        m_serverUrl = url;
    }
}

void ApiClient::registerBotUser(const QJsonObject& userData)
{
    QNetworkRequest request(QUrl(m_serverUrl + "/api/bot/register"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(userData).toJson());

    // --- ДОДАНО: "Прикріплюємо" ID до запиту ---
    reply->setProperty("telegramId", userData["telegram_id"].toVariant().toLongLong());
    // ------------------------------------------

    connect(reply, &QNetworkReply::finished, this, &ApiClient::onBotRegisterReplyFinished);
}

void ApiClient::onBotRegisterReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    // --- ДОДАНО: Дістаємо "прикріплений" ID ---
    qint64 telegramId = reply->property("telegramId").toLongLong();
    // ---------------------------------------

    ApiError error = parseReply(reply);
    if (reply->error() == QNetworkReply::NoError) {
        // --- ЗМІНЕНО: Передаємо ID в сигнал ---
        emit botUserRegistered(QJsonDocument::fromJson(error.responseBody).object(), telegramId);
    } else {
        // --- ЗМІНЕНО: Передаємо ID в сигнал ---
        emit botUserRegistrationFailed(error, telegramId);
    }
    reply->deleteLater();
}


void ApiClient::fetchBotRequests()
{
    // Створюємо запит з аутентифікацією
    QNetworkRequest request = createAuthenticatedRequest(QUrl(m_serverUrl + "/api/bot/requests"));
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onBotRequestsReplyFinished);
}


void ApiClient::onBotRequestsReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    ApiError error = parseReply(reply); // Використовуємо наш універсальний парсер помилок

    // Перевіряємо, чи все пройшло успішно
    if (reply->error() == QNetworkReply::NoError && error.httpStatusCode == 200)
    {
        QJsonDocument doc = QJsonDocument::fromJson(error.responseBody);
        if (doc.isArray()) {
            // Успіх: відправляємо сигнал з масивом запитів
            emit botRequestsFetched(doc.array());
        } else {
            // Помилка: сервер повернув не масив
            error.errorString = "Invalid response from server: expected a JSON array.";
            emit botRequestsFetchFailed(error);
        }
    }
    else
    {
        // Помилка: або мережева, або від сервера
        emit botRequestsFetchFailed(error);
    }

    reply->deleteLater();
}

// --- Додайте цей метод кудись до інших публічних методів ---

void ApiClient::rejectBotRequest(int requestId)
{
    QNetworkRequest request = createAuthenticatedRequest(QUrl(m_serverUrl + "/api/bot/reject"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Створюємо тіло запиту: {"request_id": 123}
    QJsonObject jsonBody;
    jsonBody["request_id"] = requestId;

    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(jsonBody).toJson());

    // Зберігаємо ID запиту, щоб знати в слоті, про що прийшла відповідь
    reply->setProperty("requestId", requestId);

    connect(reply, &QNetworkReply::finished, this, &ApiClient::onBotRequestRejectReplyFinished);
}

// --- Додайте цей слот кудись до інших слотів-обробників ---

void ApiClient::onBotRequestRejectReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    int requestId = reply->property("requestId").toInt();
    ApiError error = parseReply(reply); // Використовуємо наш універсальний парсер

    // Перевіряємо на успіх (код 200 OK)
    if (reply->error() == QNetworkReply::NoError && error.httpStatusCode == 200)
    {
        emit botRequestRejected(requestId);
    }
    else
    {
        // Помилка (або мережева, або від сервера)
        emit botRequestRejectFailed(error);
    }

    reply->deleteLater();
}



void ApiClient::approveBotRequest(int requestId, const QString& login)
{
    QNetworkRequest request = createAuthenticatedRequest(QUrl(m_serverUrl + "/api/bot/approve"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Створюємо тіло запиту: {"request_id": 123, "login": "r.sydorenko"}
    QJsonObject jsonBody;
    jsonBody["request_id"] = requestId;
    jsonBody["login"] = login;

    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(jsonBody).toJson());

    // Зберігаємо ID запиту, щоб знати в слоті, про що прийшла відповідь
    reply->setProperty("requestId", requestId);

    connect(reply, &QNetworkReply::finished, this, &ApiClient::onBotRequestApproveReplyFinished);
}


void ApiClient::onBotRequestApproveReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    int requestId = reply->property("requestId").toInt();
    ApiError error = parseReply(reply); // Використовуємо наш універсальний парсер

    // Перевіряємо на успіх (код 200 OK)
    if (reply->error() == QNetworkReply::NoError && error.httpStatusCode == 200)
    {
        emit botRequestApproved(requestId);
    }
    else
    {
        // Помилка (або мережева, або від сервера)
        emit botRequestApproveFailed(error);
    }

    reply->deleteLater();
}


void ApiClient::linkBotRequest(int requestId, int userId)
{
    QNetworkRequest request = createAuthenticatedRequest(QUrl(m_serverUrl + "/api/bot/link"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Створюємо тіло запиту: {"request_id": 123, "user_id": 456}
    QJsonObject jsonBody;
    jsonBody["request_id"] = requestId;
    jsonBody["user_id"] = userId;

    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(jsonBody).toJson());

    // Зберігаємо ID запиту, щоб знати в слоті, про що прийшла відповідь
    reply->setProperty("requestId", requestId);

    connect(reply, &QNetworkReply::finished, this, &ApiClient::onBotRequestLinkReplyFinished);
}

void ApiClient::onBotRequestLinkReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    int requestId = reply->property("requestId").toInt();
    ApiError error = parseReply(reply); // Використовуємо наш універсальний парсер

    // Перевіряємо на успіх (код 200 OK)
    if (reply->error() == QNetworkReply::NoError && error.httpStatusCode == 200)
    {
        emit botRequestLinked(requestId);
    }
    else
    {
        // Помилка (або мережева, або від сервера)
        emit botRequestLinkFailed(error);
    }

    reply->deleteLater();
}


void ApiClient::checkBotUserStatus(const QJsonObject& message)
{
    // --- ЗМІНЕНО ТУТ ---
    // Ми просто замінюємо QNetworkRequest на наш новий хелпер,
    // який автоматично додає "X-Bot-Token".
    QNetworkRequest request = createBotRequest(QUrl(m_serverUrl + "/api/bot/me"));
    // --- КІНЕЦЬ ЗМІН ---

    // (Решта вашого коду залишається БЕЗ ЗМІН)
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject jsonBody;
    jsonBody["telegram_id"] = message["from"].toObject()["id"].toVariant().toLongLong();

    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(jsonBody).toJson());

    reply->setProperty("messageData", message);

    connect(reply, &QNetworkReply::finished, this, &ApiClient::onBotUserStatusReplyFinished);
}

void ApiClient::onBotUserStatusReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    // --- ЗМІНЕНО: Дістаємо "прикріплене" повідомлення ---
    QJsonObject message = reply->property("messageData").toJsonObject();

    ApiError error = parseReply(reply);

    if (reply->error() == QNetworkReply::NoError && error.httpStatusCode == 200)
    {
        QJsonDocument doc = QJsonDocument::fromJson(error.responseBody);
        if (doc.isObject()) {
            // "Прокидуємо" і статус, і оригінальне повідомлення
            emit botUserStatusReceived(doc.object(), message);
        } else {
            error.errorString = "Invalid response from server: expected a JSON object.";
            emit botUserStatusCheckFailed(error);
        }
    }
    else
    {
        emit botUserStatusCheckFailed(error);
    }

    reply->deleteLater();
}

//
/**
 * @brief Створює запит для Бота з ключем API та ID користувача Telegram.
 * Використовується для доступу до ЗАХИЩЕНИХ маршрутів (напр., /api/clients).
 */
QNetworkRequest ApiClient::createBotRequest(const QUrl &url, qint64 telegramId)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // Додаємо два наші нові заголовки
    request.setRawHeader("X-Bot-Token", m_botApiKey.toUtf8());
    request.setRawHeader("X-Telegram-ID", QByteArray::number(telegramId));
    return request;
}

/**
 * @brief Створює запит для Бота ТІЛЬКИ з ключем API.
 * Використовується для ПУБЛІЧНИХ маршрутів бота (напр., /api/bot/me),
 * щоб захистити їх від сторонніх.
 */
QNetworkRequest ApiClient::createBotRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // Додаємо один заголовок
    request.setRawHeader("X-Bot-Token", m_botApiKey.toUtf8());
    return request;
}


/**
 * @brief (Новий) Запитує список клієнтів, доступних для цього користувача бота.
 */
void ApiClient::fetchBotClients(qint64 telegramId)
{
    // Використовуємо наш новий хелпер, що додає X-Bot-Token та X-Telegram-ID
    QNetworkRequest request = createBotRequest(QUrl(m_serverUrl + "/api/clients"), telegramId);

    QNetworkReply* reply = m_networkManager->get(request);

    // Зберігаємо telegramId, щоб знати, кому відповідати у слоті
    reply->setProperty("telegram_id", QVariant(telegramId));

    connect(reply, &QNetworkReply::finished, this, &ApiClient::onBotClientsReplyFinished);
}

/**
 * @brief (Новий) Обробляє відповідь від /api/clients для бота.
 */
void ApiClient::onBotClientsReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    // Отримуємо telegramId, який ми зберегли
    qint64 telegramId = reply->property("telegram_id").toLongLong();

    ApiError error = parseReply(reply); // Використовуємо наш універсальний парсер

    if (reply->error() == QNetworkReply::NoError && error.httpStatusCode == 200)
    {
        QJsonDocument doc = QJsonDocument::fromJson(error.responseBody);
        if (doc.isArray()) {
            // Успіх
            emit botClientsFetched(doc.array(), telegramId);
        } else {
            // Помилка: відповідь не є масивом
            error.errorString = "Invalid response from server: expected a JSON array.";
            emit botClientsFetchFailed(error, telegramId);
        }
    }
    else
    {
        // Помилка: мережева або серверна
        emit botClientsFetchFailed(error, telegramId);
    }
    reply->deleteLater();
}


//

/**
 * @brief (НОВИЙ) Адмін-бот запитує список запитів на реєстрацію.
 */
void ApiClient::fetchBotRequestsForAdmin(qint64 telegramId)
{
    // Використовуємо той самий маршрут, що й Gandalf, але з аутентифікацією бота
    QNetworkRequest request = createBotRequest(QUrl(m_serverUrl + "/api/bot/requests"), telegramId);

    QNetworkReply* reply = m_networkManager->get(request);

    // Зберігаємо telegramId, щоб знати, кому відповідати
    reply->setProperty("telegram_id", QVariant(telegramId));

    connect(reply, &QNetworkReply::finished, this, &ApiClient::onBotAdminRequestsReplyFinished);
}

/**
 * @brief (НОВИЙ СЛОТ) Обробляє відповідь від /api/bot/requests для адмін-бота.
 */
void ApiClient::onBotAdminRequestsReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    qint64 telegramId = reply->property("telegram_id").toLongLong();
    ApiError error = parseReply(reply); // Використовуємо наш універсальний парсер

    if (reply->error() == QNetworkReply::NoError && error.httpStatusCode == 200)
    {
        QJsonDocument doc = QJsonDocument::fromJson(error.responseBody);
        if (doc.isArray()) {
            // Успіх
            emit botAdminRequestsFetched(doc.array(), telegramId);
        } else {
            // Помилка: відповідь не є масивом
            error.errorString = "Invalid response from server: expected a JSON array.";
            emit botAdminRequestsFetchFailed(error, telegramId);
        }
    }
    else
    {
        // Помилка: мережева або серверна
        emit botAdminRequestsFetchFailed(error, telegramId);
    }
    reply->deleteLater();
}

//
/**
 * @brief (ОНОВЛЕНО) Адмін-бот схвалює запит на реєстрацію.
 * Тепер також надсилає 'login'.
 */
void ApiClient::approveBotRequestForAdmin(qint64 adminTelegramId, int requestId, const QString& login)
{
    QNetworkRequest request = createBotRequest(QUrl(m_serverUrl + "/api/bot/approve"), adminTelegramId);

    QJsonObject body;
    body["request_id"] = requestId;
    body["login"] = login; // <-- ДОДАНО (це виправляє помилку 400)

    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(body).toJson());

    reply->setProperty("telegram_id", QVariant(adminTelegramId));
    reply->setProperty("request_id", QVariant(requestId));

    connect(reply, &QNetworkReply::finished, this, &ApiClient::onBotAdminApproveReplyFinished);
}

/**
 * @brief (ОНОВЛЕНО) Адмін-бот відхиляє запит на реєстрацію.
 * Тепер також надсилає 'login'.
 */
void ApiClient::rejectBotRequestForAdmin(qint64 adminTelegramId, int requestId, const QString& login)
{
    QNetworkRequest request = createBotRequest(QUrl(m_serverUrl + "/api/bot/reject"), adminTelegramId);

    QJsonObject body;
    body["request_id"] = requestId;
    body["login"] = login; // <-- ДОДАНО (для узгодженості)

    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(body).toJson());

    reply->setProperty("telegram_id", QVariant(adminTelegramId));
    reply->setProperty("request_id", QVariant(requestId));

    connect(reply, &QNetworkReply::finished, this, &ApiClient::onBotAdminRejectReplyFinished);
}


/**
 * @brief (НОВИЙ СЛОТ) Обробляє відповідь на схвалення запиту.
 */
void ApiClient::onBotAdminApproveReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    qint64 telegramId = reply->property("telegram_id").toLongLong();
    int requestId = reply->property("request_id").toInt();
    ApiError error = parseReply(reply);

    if (reply->error() == QNetworkReply::NoError && error.httpStatusCode == 200) {
        emit botAdminRequestApproved(requestId, telegramId);
    } else {
        emit botAdminRequestApproveFailed(error, telegramId);
    }
    reply->deleteLater();
}

/**
 * @brief (НОВИЙ СЛОТ) Обробляє відповідь на відхилення запиту.
 */
void ApiClient::onBotAdminRejectReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    qint64 telegramId = reply->property("telegram_id").toLongLong();
    int requestId = reply->property("request_id").toInt();
    ApiError error = parseReply(reply);

    if (reply->error() == QNetworkReply::NoError && error.httpStatusCode == 200) {
        emit botAdminRequestRejected(requestId, telegramId);
    } else {
        emit botAdminRequestRejectFailed(error, telegramId);
    }
    reply->deleteLater();
}

//

/**
 * @brief Адмін-бот запитує список активних користувачів бота.
 */
void ApiClient::fetchBotActiveUsers(qint64 adminTelegramId)
{
    // Викликаємо новий маршрут /api/bot/users
    QNetworkRequest request = createBotRequest(QUrl(m_serverUrl + "/api/bot/users"), adminTelegramId);

    QNetworkReply* reply = m_networkManager->get(request);

    reply->setProperty("telegram_id", QVariant(adminTelegramId));
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onBotActiveUsersReplyFinished);
}

/**
 * @brief Обробляє відповідь від /api/bot/users.
 */
void ApiClient::onBotActiveUsersReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    qint64 telegramId = reply->property("telegram_id").toLongLong();
    ApiError error = parseReply(reply);

    if (reply->error() == QNetworkReply::NoError && error.httpStatusCode == 200)
    {
        QJsonDocument doc = QJsonDocument::fromJson(error.responseBody);
        if (doc.isArray()) {
            emit botActiveUsersFetched(doc.array(), telegramId);
        } else {
            error.errorString = "Invalid response from server: expected a JSON array.";
            emit botActiveUsersFetchFailed(error, telegramId);
        }
    }
    else
    {
        emit botActiveUsersFetchFailed(error, telegramId);
    }
    reply->deleteLater();
}
