#ifndef APICLIENT_H
#define APICLIENT_H

#include <QObject>
#include <QString>
#include <QJsonArray>  // Рекомендовано додати для сигналів
#include <QJsonObject> // Рекомендовано додати для сигналів

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;
class User;

// Структура для зберігання детальної інформації про помилку API
struct ApiError {
    int httpStatusCode = 0;         // Напр., 404, 500
    QString requestUrl;             // URL, на який йшов запит
    QString errorString;            // Текстовий опис помилки
    QByteArray responseBody;        // Тіло відповіді від сервера (може містити JSON з деталями)
};
Q_DECLARE_METATYPE(ApiError)

class ApiClient : public QObject
{
    Q_OBJECT
public:
    static ApiClient& instance();

    // Методи для виклику API
    void login(const QString& username);
    void fetchAllUsers();
    void fetchUserById(int userId);
    void fetchAllRoles();
    void updateUser(int userId, const QJsonObject& userData);
    void fetchAllClients();
    void createClient(const QJsonObject& clientData);
    void fetchClientById(int clientId);
    void fetchAllIpGenMethods();
    void testDbConnection(const QJsonObject& config);
    void updateClient(int clientId, const QJsonObject& clientData);
    void fetchSettings(const QString& appName);
    void updateSettings(const QString& appName, const QVariantMap& settings);
    void syncClientObjects(int clientId);
    void fetchSyncStatus(int clientId);
    void fetchObjects(const QVariantMap& filters = {});
    void fetchRegionsList();
    // метод для встановлення URL:
    void setServerUrl(const QString& url);
    // метод для реєстрації:
    void registerBotUser(const QJsonObject& userData);
    void fetchBotRequests();
    void rejectBotRequest(int requestId);
    void approveBotRequest(int requestId, const QString& login);
    void linkBotRequest(int requestId, int userId);
    void checkBotUserStatus(const QJsonObject& message);

signals:
    // Сигнали для логіну
    void loginSuccess(User* user);
    void loginFailed(const ApiError& error);

    // Сигнали для списку користувачів
    void usersFetched(const QJsonArray& users);
    void usersFetchFailed(const ApiError& error);

    // Сигнали для одного користувача
    void userDetailsFetched(const QJsonObject& user);
    void userDetailsFetchFailed(const ApiError& error);

    // Сигнали для ролей
    void rolesFetched(const QJsonArray& roles);
    void rolesFetchFailed(const ApiError& error);

    // Сигнали для оновлення користувача
    void userUpdateSuccess();
    void userUpdateFailed(const ApiError& error);

    // Сигнали для списку клієнтів
    void clientsFetched(const QJsonArray& clients);
    void clientsFetchFailed(const ApiError& error);

    // Сигнали для створення клієнта
    void clientCreateSuccess(const QJsonObject& newClient);
    void clientCreateFailed(const ApiError& error); // <-- ЦЕЙ СИГНАЛ БУЛО ПРОПУЩЕНО

    void clientDetailsFetched(const QJsonObject& client);
    void clientDetailsFetchFailed(const ApiError& error);

    void ipGenMethodsFetched(const QJsonArray& methods);
    void ipGenMethodsFetchFailed(const ApiError& error);

    void connectionTestSuccess(const QString& message);
    void connectionTestFailed(const ApiError& error);

    void clientUpdateSuccess();
    void clientUpdateFailed(const ApiError& error);

    // Додайте в signals секцію
    void settingsFetched(const QVariantMap& settings);
    void settingsFetchFailed(const ApiError& error);

    void settingsUpdateSuccess();
    void settingsUpdateFailed(const ApiError& error);

    void syncRequestAcknowledged(int clientId, bool success, const ApiError& details);

    void syncStatusFetched(int clientId, const QJsonObject& status);
    void syncStatusFetchFailed(int clientId, const ApiError& error);

    void objectsFetched(const QJsonArray& objects);
    void objectsFetchFailed(const ApiError& error);

    void regionsListFetched(const QStringList& regions);
    void regionsListFetchFailed(const ApiError& error);

    void botUserRegistered(const QJsonObject& result, qint64 telegramId);
    void botUserRegistrationFailed(const ApiError& error, qint64 telegramId);

    void botRequestsFetched(const QJsonArray& requests);
    void botRequestsFetchFailed(const ApiError& error);

    void botRequestRejected(int requestId);
    void botRequestRejectFailed(const ApiError& error);

    void botRequestApproved(int requestId);
    void botRequestApproveFailed(const ApiError& error);

    void botRequestLinked(int requestId);
    void botRequestLinkFailed(const ApiError& error);

    void botUserStatusReceived(const QJsonObject& status, const QJsonObject& message);
    void botUserStatusCheckFailed(const ApiError& error);

private slots:
    void onLoginReplyFinished();
    void onUsersReplyFinished();
    void onUserDetailsReplyFinished();
    void onRolesReplyFinished();
    void onUserUpdateReplyFinished();
    void onClientsReplyFinished();
    void onCreateClientReplyFinished();
    void onClientDetailsReplyFinished();
    void onIpGenMethodsReplyFinished();
    void onConnectionTestReplyFinished();
    void onClientUpdateReplyFinished();
    void onSettingsReplyFinished();
    void onSettingsUpdateReplyFinished();
    void onSyncReplyFinished();
    void onSyncStatusReplyFinished();
    void onObjectsReplyFinished();
    void onRegionsListReplyFinished();
    void onBotRegisterReplyFinished();
    void onBotRequestsReplyFinished();
    void onBotRequestRejectReplyFinished();
    void onBotRequestApproveReplyFinished();
    void onBotRequestLinkReplyFinished();
    void onBotUserStatusReplyFinished();
private:
    ApiClient(QObject* parent = nullptr);
    ~ApiClient() = default;
    ApiClient(const ApiClient&) = delete;
    ApiClient& operator=(const ApiClient&) = delete;

    QNetworkRequest createAuthenticatedRequest(const QUrl &url);

    QNetworkAccessManager* m_networkManager;
    QString m_serverUrl;
    QString m_authToken;
};

#endif // APICLIENT_H
