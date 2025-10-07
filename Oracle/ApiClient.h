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

signals:
    void loginSuccess(User* user);
    void loginFailed(const QString& errorString);

private slots:
    void onLoginReplyFinished();

private:
    ApiClient(QObject* parent = nullptr);
    ~ApiClient() = default;
    ApiClient(const ApiClient&) = delete;
    ApiClient& operator=(const ApiClient&) = delete;

    QNetworkAccessManager* m_networkManager;
    QString m_serverUrl;
};

#endif // APICLIENT_H
