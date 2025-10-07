#include "SessionManager.h"
#include "User.h"
#include "DbManager.h"
#include "Logger.h"
#include <QCoreApplication>
#include <QProcessEnvironment>

SessionManager& SessionManager::instance()
{
    static SessionManager self;
    return self;
}

SessionManager::SessionManager() = default;
SessionManager::~SessionManager()
{
    delete m_currentUser;
}

const User* SessionManager::login(const QString& username)
{
    // Прибираємо перевірку isLoggedIn() і отримання логіну з оточення
    if (username.isEmpty()) {
        logCritical() << "Login attempt with empty username.";
        return nullptr;
    }
    logInfo() << "Attempting to identify user by login:" << username;

    bool ok = false;
    int userId = DbManager::instance().getOrCreateUser(username, ok);

    if (!ok || userId <= 0) {
        logCritical() << "Failed to get or create a user in the database.";
        return nullptr;
    }

    // Видаляємо старий m_currentUser перед створенням нового, щоб уникнути витоків пам'яті
    delete m_currentUser;
    m_currentUser = DbManager::instance().loadUser(userId);

    if (!m_currentUser) {
        logCritical() << "Failed to load user profile from database.";
        return nullptr;
    }

    if (!m_currentUser->isActive()) {
        logCritical() << "User" << m_currentUser->login() << "is disabled.";
        delete m_currentUser;
        m_currentUser = nullptr;
        return nullptr;
    }

    logInfo() << "User identified successfully:" << m_currentUser->fio()
              << "(" << m_currentUser->login() << ")"
              << "with roles:" << m_currentUser->roles().join(", ");

    return m_currentUser; // Повертаємо об'єкт User
}

const User* SessionManager::currentUser() const
{
    return m_currentUser;
}

bool SessionManager::isLoggedIn() const
{
    return m_currentUser != nullptr;
}
