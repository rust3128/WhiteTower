#include "SessionManager.h"
#include "User.h"
#include "DbManager.h"
#include "Logger.h"
#include <QCoreApplication>
#include <QProcessEnvironment>
#include <QCryptographicHash>
#include <QDateTime>

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

QPair<const User*, QString> SessionManager::login(const QString& username)
{
    // ... (попередня логіка ідентифікації користувача до отримання об'єкта 'user')
    // Замість 'const User* user = ...' робимо:
    const User* user = identifyAndLoadUser(username); // Уявна функція, що містить стару логіку

    if (!user) {
        return {nullptr, QString()}; // Повертаємо порожню пару при помилці
    }

    // --- ЛОГІКА СТВОРЕННЯ ТОКЕНА ---
    // 1. Генеруємо унікальний токен
    QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // 2. Створюємо його хеш
    QByteArray tokenHash = QCryptographicHash::hash(token.toUtf8(), QCryptographicHash::Sha256).toHex();

    // 3. Встановлюємо час життя (напр., 7 днів)
    QDateTime expiresAt = QDateTime::currentDateTime().addDays(7);

    // 4. Зберігаємо хеш в базу даних
    if (!DbManager::instance().saveSession(user->id(), tokenHash, expiresAt)) {
        // Якщо не вдалося зберегти сесію - логін неуспішний
        logCritical() << "Could not save session to the database for user" << user->login();
        return {nullptr, QString()};
    }

    logInfo() << "Session created for user" << user->login();

    // Повертаємо пару з об'єктом User та ОРИГІНАЛЬНИМ токеном
    return {user, token};
}

const User* SessionManager::identifyAndLoadUser(const QString& username)
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
