#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QString>
#include <QPair>

class User; // Випереджувальне оголошення

class SessionManager
{
public:
    static SessionManager& instance();

    const User* identifyAndLoadUser(const QString& username);
    QPair<const User*, QString> login(const QString& username);
    void setCurrentUser(User* user);
    const User* currentUser() const;
    bool isLoggedIn() const;


private:
    SessionManager();
    ~SessionManager();
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    User* m_currentUser = nullptr;
};

#endif // SESSIONMANAGER_H
