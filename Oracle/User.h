#ifndef USER_H
#define USER_H

#include <QString>
#include <QStringList>
#include <QJsonObject>

class User
{
public:
    User(int id, QString login, QString fio, bool isActive, QStringList roles,
         qint64 telegramId, const QString& jiraToken);

    int id() const;
    const QString& login() const;
    const QString& fio() const;
    bool isActive() const;
    const QStringList& roles() const;
    bool hasRole(const QString& roleName) const;
    QJsonObject toJson() const;
    static User* fromJson(const QJsonObject& json);
    qint64 telegramId() const;
    const QString& jiraToken() const;
    bool isAdmin() const;
private:
    int m_id;
    QString m_login;
    QString m_fio;
    bool m_isActive;
    QStringList m_roles;
    qint64 m_telegramId;
    QString m_jiraToken;
};

#endif // USER_H
