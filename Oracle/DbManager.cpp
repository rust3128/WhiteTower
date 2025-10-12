#include "DbManager.h"
#include "ConfigManager.h" // Підключаємо повне оголошення тут
#include "Logger.h"
#include "User.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QVariantMap>
#include <QJsonArray>
#include <QCryptographicHash>

DbManager& DbManager::instance()
{
    static DbManager self;
    return self;
}

DbManager::DbManager()
{
    // QSqlDatabase::addDatabase() створює з'єднання з унікальним іменем
    // Ми будемо використовувати з'єднання за замовчуванням
    if (QSqlDatabase::contains(QSqlDatabase::defaultConnection)) {
        m_db = QSqlDatabase::database(QSqlDatabase::defaultConnection);
    } else {
        m_db = QSqlDatabase::addDatabase("QIBASE");
    }
}

DbManager::~DbManager()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool DbManager::connect(const ConfigManager& config)
{
    m_db.setHostName(config.getDbHost());
    m_db.setPort(config.getDbPort());
    m_db.setDatabaseName(config.getDbPath());
    m_db.setUserName(config.getDbUser());
    m_db.setPassword(config.getDbPassword());

    if (!m_db.open()) {
        logCritical() << "Database connection failed:" << lastError();
        return false;
    }

    logInfo() << "Successfully connected to the database.";
    return true;
}

bool DbManager::isConnected() const
{
    return m_db.isOpen();
}

QString DbManager::lastError() const
{
    return m_db.lastError().text();
}


// Додайте цю функцію в кінець файлу DbManager.cpp
QVariantMap DbManager::loadSettings(const QString& appName)
{
    QVariantMap settings;
    if (!isConnected()) {
        logCritical() << "Cannot load app settings: no database connection.";
        return settings;
    }

    QSqlQuery query(m_db);
    // Запит тепер простий: вибирає параметри ТІЛЬКИ для вказаного appName
    query.prepare("SELECT PARAM_NAME, PARAM_VALUE FROM APP_SETTINGS WHERE APP_NAME = :appName");
    query.bindValue(":appName", appName);

    if (!query.exec()) {
        logCritical() << "Failed to execute settings query for" << appName << ":" << query.lastError().text();
        return settings;
    }

    while (query.next()) {
        QString name = query.value(0).toString();
        QVariant value = query.value(1);
        settings.insert(name, value);
    }
    logDebug() << "Loaded" << settings.count() << "settings for scope:" << appName;
    return settings;
}


int DbManager::getOrCreateUser(const QString& login, bool& ok)
{
    ok = false;
    if (!isConnected()) {
        logCritical() << "Cannot get/create user: no DB connection";
        return -1;
    }

    QSqlQuery query(m_db);
    query.prepare("EXECUTE PROCEDURE GET_OR_CREATE_USER(:login)");
    query.bindValue(":login", login);

    if (!query.exec()) {
        logCritical() << "Failed to execute GET_OR_CREATE_USER:" << query.lastError().text();
        return -1;
    }

    if (query.next()) {
        ok = true;
        return query.value(0).toInt();
    }
    return -1;
}

User* DbManager::loadUser(int userId)
{
    if (!isConnected()) {
        logCritical() << "Cannot load user: no DB connection";
        return nullptr;
    }

    // Запит для отримання основних даних
    QSqlQuery userQuery(m_db);
    userQuery.prepare("SELECT user_login, user_fio, is_active, telegram_id, jira_token "
                      "FROM USERS WHERE user_id = :id");
    userQuery.bindValue(":id", userId);

    if (!userQuery.exec() || !userQuery.next()) { /* ... */ }

    QString login = userQuery.value(0).toString();
    QString fio = userQuery.value(1).toString();
    bool isActive = userQuery.value(2).toBool();
    qint64 telegramId = userQuery.value(3).toLongLong(); // Читаємо нові поля
    QString jiraToken = userQuery.value(4).toString();  // Читаємо нові поля

    // Запит для отримання ролей
    QStringList roles;
    QSqlQuery rolesQuery(m_db);
    rolesQuery.prepare("SELECT r.ROLE_NAME FROM USER_ROLES ur "
                       "JOIN ROLES r ON ur.ROLE_ID = r.ROLE_ID "
                       "WHERE ur.USER_ID = :id");
    rolesQuery.bindValue(":id", userId);
    if (rolesQuery.exec()) {
        while (rolesQuery.next()) {
            roles.append(rolesQuery.value(0).toString());
        }
    }

     return new User(userId, login, fio, isActive, roles, telegramId, jiraToken);
}


QList<User*> DbManager::loadAllUsers()
{
    QList<User*> userList;
    if (!isConnected()) return userList;

    // Просто вибираємо ID всіх активних користувачів
    QSqlQuery query(m_db);
    query.prepare("SELECT user_id FROM USERS WHERE is_active = 1 ORDER BY user_fio");
    if (!query.exec()) {
        logCritical() << "Failed to load all users:" << query.lastError().text();
        return userList;
    }

    while (query.next()) {
        int userId = query.value(0).toInt();
        // Використовуємо вже існуючий метод для завантаження повного профілю
        User* user = loadUser(userId);
        if (user) {
            userList.append(user);
        }
    }
    return userList;
}

QList<QVariantMap> DbManager::loadAllRoles()
{
    QList<QVariantMap> roles;
    if (!isConnected()) {
        logCritical() << "Cannot load roles: no database connection.";
        return roles;
    }

    QSqlQuery query(m_db);
    query.prepare("SELECT ROLE_ID, ROLE_NAME, DESCRIPTION FROM ROLES ORDER BY ROLE_ID");

    if (!query.exec()) {
        logCritical() << "Failed to execute load all roles query:" << query.lastError().text();
        return roles;
    }

    while (query.next()) {
        QVariantMap role;
        role["role_id"] = query.value("ROLE_ID");
        role["role_name"] = query.value("ROLE_NAME");
        role["description"] = query.value("DESCRIPTION");
        roles.append(role);
    }
    return roles;
}

bool DbManager::updateUser(int userId, const QJsonObject& userData)
{
    if (!isConnected()) {
        logCritical() << "Cannot update user: no DB connection";
        return false;
    }

    // Відкриваємо транзакцію. Всі запити до commit() або rollback() будуть єдиним цілим.
    if (!m_db.transaction()) {
        logCritical() << "Failed to start transaction:" << m_db.lastError().text();
        return false;
    }

    // 1. Оновлюємо основні дані в таблиці USERS
    QSqlQuery updateQuery(m_db);
    updateQuery.prepare("UPDATE USERS SET "
                        "USER_FIO = :fio, "
                        "IS_ACTIVE = :isActive, "
                        "TELEGRAM_ID = :telegramId, "
                        "JIRA_TOKEN = :jiraToken "
                        "WHERE USER_ID = :id");
    updateQuery.bindValue(":fio", userData["fio"].toString());
    updateQuery.bindValue(":isActive", userData["is_active"].toBool());
    updateQuery.bindValue(":telegramId", userData["telegram_id"].toVariant().toLongLong());
    updateQuery.bindValue(":jiraToken", userData["jira_token"].toString());
    updateQuery.bindValue(":id", userId);

    if (!updateQuery.exec()) {
        logCritical() << "Failed to update USERS table:" << updateQuery.lastError().text();
        m_db.rollback(); // Відкочуємо транзакцію при помилці
        return false;
    }

    // 2. Повністю видаляємо старі ролі користувача
    QSqlQuery deleteRolesQuery(m_db);
    deleteRolesQuery.prepare("DELETE FROM USER_ROLES WHERE USER_ID = :id");
    deleteRolesQuery.bindValue(":id", userId);
    if (!deleteRolesQuery.exec()) {
        logCritical() << "Failed to delete old roles:" << deleteRolesQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    // 3. Додаємо нові ролі
    QJsonArray roles = userData["roles"].toArray();
    for (const QJsonValue &roleValue : roles) {
        QString roleName = roleValue.toString();

        QSqlQuery insertRoleQuery(m_db);
        // Вставляємо зв'язку, отримуючи ROLE_ID за його назвою
        insertRoleQuery.prepare("INSERT INTO USER_ROLES (USER_ID, ROLE_ID) "
                                "VALUES (:userId, (SELECT ROLE_ID FROM ROLES WHERE ROLE_NAME = :roleName))");
        insertRoleQuery.bindValue(":userId", userId);
        insertRoleQuery.bindValue(":roleName", roleName);
        if (!insertRoleQuery.exec()) {
            logCritical() << "Failed to insert new role" << roleName << ":" << insertRoleQuery.lastError().text();
            m_db.rollback();
            return false;
        }
    }

    // Якщо всі кроки пройшли успішно, підтверджуємо транзакцію
    if (!m_db.commit()) {
        logCritical() << "Failed to commit transaction:" << m_db.lastError().text();
        // Firebird може автоматично відкотити транзакцію при невдалому коміті, але для надійності можна викликати rollback()
        m_db.rollback();
        return false;
    }

    return true;
}


bool DbManager::saveSession(int userId, const QByteArray& tokenHash, const QDateTime& expiresAt)
{
    if (!isConnected()) return false;

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO SESSIONS (USER_ID, TOKEN_HASH, EXPIRES_AT) VALUES (:userId, :tokenHash, :expiresAt)");
    query.bindValue(":userId", userId);
    query.bindValue(":tokenHash", QString(tokenHash));
    query.bindValue(":expiresAt", expiresAt);

    if (!query.exec()) {
        logCritical() << "Failed to save session:" << query.lastError().text();
        return false;
    }
    return true;
}


int DbManager::findUserIdByToken(const QByteArray& tokenHash)
{
    if (!isConnected()) return -1;

    QSqlQuery query(m_db);
    // Шукаємо сесію за хешем, перевіряючи, що вона ще не прострочена
    query.prepare("SELECT USER_ID FROM SESSIONS "
                  "WHERE TOKEN_HASH = :tokenHash AND EXPIRES_AT > CURRENT_TIMESTAMP");
    query.bindValue(":tokenHash", QString(tokenHash));

    if (query.exec() && query.next()) {
        return query.value(0).toInt(); // Повертаємо ID користувача, якщо сесія знайдена
    }

    if (query.lastError().isValid()) {
        logCritical() << "Token validation query failed:" << query.lastError().text();
    }

    return -1; // Повертаємо -1, якщо сесія не знайдена або сталася помилка
}

QList<QVariantMap> DbManager::loadAllClients()
{
    QList<QVariantMap> clients;
    if (!isConnected()) return clients;

    QSqlQuery query(m_db);
    // Вибираємо тільки ID та ім'я для списку
    query.prepare("SELECT CLIENT_ID, CLIENT_NAME FROM CLIENTS WHERE IS_ACTIVE = 1 ORDER BY CLIENT_NAME");

    if (!query.exec()) {
        logCritical() << "Failed to load clients list:" << query.lastError().text();
        return clients;
    }

    while (query.next()) {
        QVariantMap client;
        client["client_id"] = query.value("CLIENT_ID");
        client["client_name"] = query.value("CLIENT_NAME");
        clients.append(client);
    }
    return clients;
}

// Повертає ID нового клієнта або -1 в разі помилки
int DbManager::createClient(const QString& clientName)
{
    if (!isConnected()) return -1;

    // Ми не будемо використовувати транзакцію тут, оскільки це один простий запит.
    // Транзакції потрібні для кількох пов'язаних запитів.

    QSqlQuery query(m_db);
    // === ЗМІНЕНО ТУТ: Додаємо SYNC_METHOD в запит ===
    query.prepare("INSERT INTO CLIENTS (CLIENT_NAME, SYNC_METHOD) "
                  "VALUES (:name, :syncMethod) RETURNING CLIENT_ID");
    query.bindValue(":name", clientName);
    query.bindValue(":syncMethod", "DIRECT"); // Встановлюємо 'DIRECT' як метод за замовчуванням

    if (!query.exec() || !query.next()) {
        logCritical() << "Failed to insert into CLIENTS table:" << query.lastError().text();
        // Якщо запит не вдався, база даних автоматично відкотить зміни
        return -1;
    }

    int newClientId = query.value(0).toInt();
    logInfo() << "Created new client '" << clientName << "' with ID:" << newClientId;
    return newClientId;
}
