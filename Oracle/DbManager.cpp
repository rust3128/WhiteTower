#include "DbManager.h"
#include "ConfigManager.h" // Підключаємо повне оголошення тут
#include "Logger.h"
#include "User.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QVariantMap>

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
    userQuery.prepare("SELECT user_login, user_fio, is_active FROM USERS WHERE user_id = :id");
    userQuery.bindValue(":id", userId);
    if (!userQuery.exec() || !userQuery.next()) {
        logCritical() << "Failed to load user profile for ID" << userId << ":" << userQuery.lastError().text();
        return nullptr;
    }
    QString login = userQuery.value(0).toString();
    QString fio = userQuery.value(1).toString();
    bool isActive = userQuery.value(2).toBool();

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

    return new User(userId, login, fio, isActive, roles);
}
