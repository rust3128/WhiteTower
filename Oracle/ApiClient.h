#ifndef APICLIENT_H
#define APICLIENT_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;
class User;

class ApiClient : public QObject
{
    Q_OBJECT
public:
    static ApiClient& instance();

    void login(const QString& username);
    void fetchAllUsers();
    void fetchUserById(int userId);
    void fetchAllRoles();
    void updateUser(int userId, const QJsonObject& userData);

signals:
    void loginSuccess(User* user);
    void loginFailed(const QString& errorString);
    void usersFetched(const QJsonArray& users);
    void usersFetchFailed(const QString& error);
    void userDetailsFetched(const QJsonObject& user);
    void userDetailsFetchFailed(const QString& error);
    void rolesFetched(const QJsonArray& roles);
    void rolesFetchFailed(const QString& error);
    void userUpdateSuccess();
    void userUpdateFailed(const QString& error);

private slots:
    void onLoginReplyFinished();
    void onUsersReplyFinished();
    void onUserDetailsReplyFinished();
    void onRolesReplyFinished();
    void onUserUpdateReplyFinished();

private:
    ApiClient(QObject* parent = nullptr);
    ~ApiClient() = default;
    ApiClient(const ApiClient&) = delete;
    ApiClient& operator=(const ApiClient&) = delete;
    QNetworkRequest createAuthenticatedRequest(const QUrl &url);

private:
    QNetworkAccessManager* m_networkManager;
    QString m_serverUrl;
    QString m_authToken;
};

#endif // APICLIENT_H
