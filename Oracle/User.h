#ifndef USER_H
#define USER_H

#include <QString>
#include <QStringList>
#include <QJsonObject>

class User
{
public:
    User(int id, QString login, QString fio, bool isActive, QStringList roles);

    int id() const;
    const QString& login() const;
    const QString& fio() const;
    bool isActive() const;
    const QStringList& roles() const;
    bool hasRole(const QString& roleName) const;
    QJsonObject toJson() const;
    static User* fromJson(const QJsonObject& json);
private:
    int m_id;
    QString m_login;
    QString m_fio;
    bool m_isActive;
    QStringList m_roles;
};

#endif // USER_H
