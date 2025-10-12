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
    void createClient(const QString& clientName);

signals:
    // Сигнали для логіну
    void loginSuccess(User* user);
    void loginFailed(const QString& errorString);

    // Сигнали для списку користувачів
    void usersFetched(const QJsonArray& users);
    void usersFetchFailed(const QString& error);

    // Сигнали для одного користувача
    void userDetailsFetched(const QJsonObject& user);
    void userDetailsFetchFailed(const QString& error);

    // Сигнали для ролей
    void rolesFetched(const QJsonArray& roles);
    void rolesFetchFailed(const QString& error);

    // Сигнали для оновлення користувача
    void userUpdateSuccess();
    void userUpdateFailed(const QString& error);

    // Сигнали для списку клієнтів
    void clientsFetched(const QJsonArray& clients);
    void clientsFetchFailed(const QString& error);

    // Сигнали для створення клієнта
    void clientCreateSuccess(const QJsonObject& newClient);
    void clientCreateFailed(const QString& error); // <-- ЦЕЙ СИГНАЛ БУЛО ПРОПУЩЕНО

private slots:
    void onLoginReplyFinished();
    void onUsersReplyFinished();
    void onUserDetailsReplyFinished();
    void onRolesReplyFinished();
    void onUserUpdateReplyFinished();
    void onClientsReplyFinished();
    void onCreateClientReplyFinished(); // <-- ЦЕЙ СЛОТ БУЛО ПРОПУЩЕНО

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
