#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QString>
class User; // Випереджувальне оголошення

class SessionManager
{
public:
    static SessionManager& instance();

    const User* login(const QString& username);
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
