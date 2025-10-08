#ifndef APICLIENT_H
#define APICLIENT_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
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

signals:
    void loginSuccess(User* user);
    void loginFailed(const QString& errorString);
    void usersFetched(const QJsonArray& users);
    void usersFetchFailed(const QString& error);
    void userDetailsFetched(const QJsonObject& user);
    void userDetailsFetchFailed(const QString& error);
    void rolesFetched(const QJsonArray& roles);
    void rolesFetchFailed(const QString& error);

private slots:
    void onLoginReplyFinished();
    void onUsersReplyFinished();
    void onUserDetailsReplyFinished();
    void onRolesReplyFinished();

private:
    ApiClient(QObject* parent = nullptr);
    ~ApiClient() = default;
    ApiClient(const ApiClient&) = delete;
    ApiClient& operator=(const ApiClient&) = delete;

    QNetworkAccessManager* m_networkManager;
    QString m_serverUrl;
};

#endif // APICLIENT_H
