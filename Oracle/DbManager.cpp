#include "DbManager.h"
#include "ConfigManager.h" // Підключаємо повне оголошення тут
#include "Logger.h"
#include "User.h"
#include "criptpass.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QVariantMap>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QUuid>


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

    QSqlQuery userQuery(m_db);
    // --- ПОЧАТОК ЗМІН ---
    // Використовуємо LEFT JOIN, щоб отримати TELEGRAM_ID з пов'язаного запиту,
    // не чіпаючи таблицю USERS
    userQuery.prepare("SELECT u.USER_LOGIN, u.USER_FIO, u.IS_ACTIVE, b.TELEGRAM_ID, u.JIRA_TOKEN "
                      "FROM USERS u "
                      "LEFT JOIN BOT_PENDING_REQUESTS b ON u.BOT_REQUEST_ID = b.REQUEST_ID "
                      "WHERE u.USER_ID = :id");
    // --- КІНЕЦЬ ЗМІН ---
    userQuery.bindValue(":id", userId);

    if (!userQuery.exec()) {
        logCritical() << "Failed to load user data:" << userQuery.lastError().text();
        return nullptr;
    }

    if (!userQuery.next()) {
        logWarning() << "No user found with ID:" << userId;
        return nullptr;
    }

    QString login = userQuery.value(0).toString();
    QString fio = userQuery.value(1).toString();
    bool isActive = userQuery.value(2).toBool();
    // Поле 3 (b.TELEGRAM_ID) може бути NULL, toLongLong() коректно це обробить (поверне 0)
    qint64 telegramId = userQuery.value(3).toLongLong();
    QString jiraToken = userQuery.value(4).toString();

    // Запит для отримання ролей (без змін)
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

    if (!m_db.transaction()) {
        logCritical() << "Failed to start transaction:" << m_db.lastError().text();
        return false;
    }

    // --- ПОЧАТОК ЗМІН ---
    // 1. Оновлюємо основні дані в таблиці USERS (БЕЗ TELEGRAM_ID)
    QSqlQuery updateQuery(m_db);
    updateQuery.prepare("UPDATE USERS SET "
                        "USER_FIO = :fio, "
                        "IS_ACTIVE = :isActive, "
                        "JIRA_TOKEN = :jiraToken "
                        "WHERE USER_ID = :id");
    updateQuery.bindValue(":fio", userData["fio"].toString());
    updateQuery.bindValue(":isActive", userData["is_active"].toBool());
    updateQuery.bindValue(":jiraToken", userData["jira_token"].toString());
    updateQuery.bindValue(":id", userId);
    // --- КІНЕЦЬ ЗМІН ---

    if (!updateQuery.exec()) {
        logCritical() << "Failed to update USERS table:" << updateQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    // 2. Оновлення ролей (без змін)
    QSqlQuery deleteRolesQuery(m_db);
    deleteRolesQuery.prepare("DELETE FROM USER_ROLES WHERE USER_ID = :id");
    deleteRolesQuery.bindValue(":id", userId);
    if (!deleteRolesQuery.exec()) {
        logCritical() << "Failed to delete old roles:" << deleteRolesQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    QJsonArray roles = userData["roles"].toArray();
    for (const QJsonValue &roleValue : roles) {
        QString roleName = roleValue.toString();
        QSqlQuery insertRoleQuery(m_db);
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

    if (!m_db.commit()) {
        logCritical() << "Failed to commit transaction:" << m_db.lastError().text();
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

QJsonObject DbManager::loadClientDetails(int clientId)
{
    QJsonObject clientData;
    if (!isConnected()) return clientData;

    QSqlQuery query(m_db);
    // Складний запит, що збирає дані з усіх таблиць, пов'язаних з клієнтом
    query.prepare(
        "SELECT "
        "  c.CLIENT_NAME, c.IS_ACTIVE, c.SYNC_METHOD, c.IP_GEN_METHOD_ID, "
        "  c.TERM_ID_MIN, c.TERM_ID_MAX, c.GAS_STATION_DB_PASSWORD, "
        "  d.DB_HOST, d.DB_PORT, d.DB_PATH, d.DB_USER, d.DB_PASSWORD "
        "FROM CLIENTS c "
        "LEFT JOIN CLIENT_CONFIG_DIRECT d ON c.CLIENT_ID = d.CLIENT_ID "
        "WHERE c.CLIENT_ID = :id"
        );
    query.bindValue(":id", clientId);

    if (!query.exec() || !query.next()) {
        logCritical() << "Failed to load client details for ID" << clientId << ":" << query.lastError().text();
        return clientData;
    }

    // Заповнюємо JSON-об'єкт отриманими даними
    clientData["client_id"] = clientId;
    clientData["client_name"] = query.value("CLIENT_NAME").toString();
    clientData["is_active"] = query.value("IS_ACTIVE").toBool();
    clientData["sync_method"] = query.value("SYNC_METHOD").toString();
    clientData["ip_gen_method_id"] = query.value("IP_GEN_METHOD_ID").toInt();
    clientData["term_id_min"] = query.value("TERM_ID_MIN").toInt();
    clientData["term_id_max"] = query.value("TERM_ID_MAX").toInt();
    clientData["gas_station_db_password"] = CriptPass::instance().decriptPass(query.value("GAS_STATION_DB_PASSWORD").toString());


    // Додаємо дані з дочірніх таблиць, якщо вони є
    if (clientData["sync_method"] == "DIRECT") {
        QJsonObject directConfig;
        directConfig["db_host"] = query.value("DB_HOST").toString();
        directConfig["db_port"] = query.value("DB_PORT").toInt();
        directConfig["db_path"] = query.value("DB_PATH").toString();
        directConfig["db_user"] = query.value("DB_USER").toString();
        directConfig["db_password"] = CriptPass::instance().decriptPass(query.value("DB_PASSWORD").toString()); // Розшифровуємо
        clientData["config_direct"] = directConfig;
    }
    // Тут буде аналогічна логіка для PALANTIR та FILE

    return clientData;
}


QList<QVariantMap> DbManager::loadAllIpGenMethods()
{
    QList<QVariantMap> methods;
    if (!isConnected()) return methods;

    QSqlQuery query("SELECT METHOD_ID, METHOD_NAME FROM IP_GEN_METHODS ORDER BY METHOD_ID", m_db);
    if (!query.exec()) {
        logCritical() << "Failed to load IP Gen Methods:" << query.lastError().text();
        return methods;
    }

    while (query.next()) {
        QVariantMap method;
        method["method_id"] = query.value("METHOD_ID");
        method["method_name"] = query.value("METHOD_NAME");
        methods.append(method);
    }
    return methods;
}

bool DbManager::testConnection(const QJsonObject& config, QString& error)
{
    const QString connName = QUuid::createUuid().toString();
    bool success = false;

    // === ВИПРАВЛЕНО ТУТ: Додаємо локальну область видимості ===
    {
        // 1. Створюємо тимчасове з'єднання
        QSqlDatabase db = QSqlDatabase::addDatabase("QIBASE", connName);

        db.setHostName(config["db_host"].toString());
        db.setPort(config["db_port"].toInt());
        db.setDatabaseName(config["db_path"].toString());
        db.setUserName(config["db_user"].toString());
        // Пароль для перевірки ми отримуємо в чистому вигляді, тому шифрувати його не треба
        // АЛЕ якщо він приходить зашифрованим, то треба розшифрувати
        db.setPassword(config["db_password"].toString());

        success = db.open();
        if (!success) {
            error = db.lastError().text();
        }

        db.close();

    } // <-- В цей момент об'єкт 'db' гарантовано знищується, і посилання на з'єднання звільняється

    // 2. Тепер, коли з'єднання ніхто не використовує, його безпечно видалити
    QSqlDatabase::removeDatabase(connName);

    return success;
}


bool DbManager::updateClient(int clientId, const QJsonObject& clientData)
{
    if (!isConnected()) {
        logCritical() << "Cannot update client: no DB connection";
        return false;
    }

    if (!m_db.transaction()) {
        logCritical() << "Failed to start transaction for updating client.";
        return false;
    }

    // 1. Оновлюємо головну таблицю CLIENTS
    QSqlQuery query(m_db);
    query.prepare("UPDATE CLIENTS SET "
                  "CLIENT_NAME = :name, "
                  "IS_ACTIVE = :isActive, "
                  "SYNC_METHOD = :syncMethod, "
                  "IP_GEN_METHOD_ID = :ipGenMethodId, "
                  "TERM_ID_MIN = :termMin, "
                  "TERM_ID_MAX = :termMax, "
                  "GAS_STATION_DB_PASSWORD = :azsDbPass "
                  "WHERE CLIENT_ID = :id");
    query.bindValue(":name", clientData["client_name"].toString());
    query.bindValue(":isActive", clientData["is_active"].toBool());
    query.bindValue(":syncMethod", clientData["sync_method"].toString());
    query.bindValue(":ipGenMethodId", clientData["ip_gen_method_id"].toInt());
    query.bindValue(":termMin", clientData["term_id_min"].toString().toInt());
    query.bindValue(":termMax", clientData["term_id_max"].toString().toInt());
    query.bindValue(":azsDbPass", CriptPass::instance().criptPass(clientData["gas_station_db_password"].toString()));
    query.bindValue(":id", clientId);

    if (!query.exec()) {
        logCritical() << "Failed to update CLIENTS table:" << query.lastError().text();
        m_db.rollback();
        return false;
    }

    // 2. Оновлюємо конфігурацію для 'DIRECT' (якщо вона є)
    if (clientData.contains("config_direct")) {
        QJsonObject directConfig = clientData["config_direct"].toObject();
        QSqlQuery configQuery(m_db);
        configQuery.prepare("UPDATE OR INSERT INTO CLIENT_CONFIG_DIRECT "
                            "(CLIENT_ID, DB_HOST, DB_PORT, DB_PATH, DB_USER, DB_PASSWORD) "
                            "VALUES (:id, :host, :port, :path, :user, :pass) "
                            "MATCHING (CLIENT_ID)");
        configQuery.bindValue(":id", clientId);
        configQuery.bindValue(":host", directConfig["db_host"].toString());
        configQuery.bindValue(":port", directConfig["db_port"].toInt());
        configQuery.bindValue(":path", directConfig["db_path"].toString());
        configQuery.bindValue(":user", directConfig["db_user"].toString());
        configQuery.bindValue(":pass", CriptPass::instance().criptPass(directConfig["db_password"].toString()));

        if (!configQuery.exec()) {
            logCritical() << "Failed to update CLIENT_CONFIG_DIRECT:" << configQuery.lastError().text();
            m_db.rollback();
            return false;
        }
    }
    // (тут буде аналогічна логіка для config_palantir та config_file)

    if (!m_db.commit()) {
        logCritical() << "Failed to commit client update transaction.";
        m_db.rollback();
        return false;
    }

    return true;
}

bool DbManager::saveSettings(const QString& appName, const QVariantMap& settings)
{
    if (!isConnected()) {
        logCritical() << "Cannot save settings: no DB connection";
        return false;
    }

    // Використовуємо транзакцію для гарантії, що або всі налаштування збережуться, або жодне
    if (!m_db.transaction()) {
        logCritical() << "Failed to start transaction for saving settings.";
        return false;
    }

    QSqlQuery query(m_db);
    // Firebird підтримує UPDATE OR INSERT, що ідеально для нашого випадку
    query.prepare("UPDATE OR INSERT INTO APP_SETTINGS (APP_NAME, PARAM_NAME, PARAM_VALUE) "
                  "VALUES (:appName, :paramName, :paramValue) "
                  "MATCHING (APP_NAME, PARAM_NAME)");

    for (auto it = settings.constBegin(); it != settings.constEnd(); ++it) {
        query.bindValue(":appName", appName);
        query.bindValue(":paramName", it.key());
        query.bindValue(":paramValue", it.value());
        if (!query.exec()) {
            logCritical() << "Failed to save setting" << it.key() << ":" << query.lastError().text();
            m_db.rollback(); // Відкочуємо транзакцію при першій же помилці
            return false;
        }
    }

    if (!m_db.commit()) {
        logCritical() << "Failed to commit settings transaction.";
        m_db.rollback();
        return false;
    }
    return true;
}

// ===================================================================
// РЕАЛІЗАЦІЯ СТРАТЕГІЇ "ПРЯМЕ ПІДКЛЮЧЕННЯ"
// ===================================================================
QVariantMap DbManager::syncViaDirectConnection(int clientId, const QJsonObject& clientDetails)
{
    logInfo() << "Executing DIRECT sync strategy for client" << clientId;

    if (!clientDetails.contains("config_direct")) {
        return {{"error", "Direct connection configuration is missing for the client."}};
    }
    QJsonObject dbConfig = clientDetails["config_direct"].toObject();

    // Імена для тимчасових, потокобезпечних з'єднань
    const QString clientConnName = "client_sync_conn_" + QUuid::createUuid().toString();
    const QString localConnName = "local_sync_conn_" + QUuid::createUuid().toString();

    // Використовуємо блок {}, щоб гарантувати, що об'єкти QSqlDatabase
    // будуть знищені до виклику removeDatabase
    {
        // === 1. Створюємо тимчасове з'єднання до БД клієнта ===
        QSqlDatabase clientDb = QSqlDatabase::addDatabase("QIBASE", clientConnName);
        clientDb.setHostName(dbConfig["db_host"].toString());
        clientDb.setPort(dbConfig["db_port"].toInt());
        clientDb.setDatabaseName(dbConfig["db_path"].toString());
        clientDb.setUserName(dbConfig["db_user"].toString());
        clientDb.setPassword(dbConfig["db_password"].toString()); // Використовуємо вже розшифрований пароль

        if (!clientDb.open()) {
            QString error = clientDb.lastError().text();
            QSqlDatabase::removeDatabase(clientConnName);
            return {{"error", "Failed to connect to client database: " + error}};
        }

        // === 2. Створюємо тимчасове з'єднання до НАШОЇ БД ===
        QSqlDatabase localDb = QSqlDatabase::cloneDatabase(m_db, localConnName);
        if (!localDb.open()) {
            QString error = localDb.lastError().text();
            clientDb.close();
            QSqlDatabase::removeDatabase(clientConnName);
            QSqlDatabase::removeDatabase(localConnName);
            return {{"error", "Failed to open a thread-local connection to the main database: " + error}};
        }

        // === 3. ВИКОНУЄМО ВАШ ФІНАЛЬНИЙ ЗАПИТ ===
        QSqlQuery getObjectsQuery(clientDb);
        getObjectsQuery.prepare("SELECT "
                                "    t.TERMINAL_ID, "
                                "    (select o.name from terminals o where o.terminal_id = t.owner_id) as REGION_NAME, "
                                "    t.NAME, "
                                "    t.ADRESS, " // Використовуємо ADRESS з бази клієнта
                                "    t.PHONE, "
                                "    t.LATITUDE, "
                                "    t.LONGITUDE, "
                                "    t.ISACTIVE, " // Використовуємо ISACTIVE
                                "    t.ISWORK "    // Використовуємо ISWORK
                                "FROM "
                                "    terminals t "
                                "WHERE "
                                "    t.TERMINAL_ID BETWEEN :minTermId AND :maxTermId "
                                "ORDER BY "
                                "    t.TERMINAL_ID");

        getObjectsQuery.bindValue(":minTermId", clientDetails["term_id_min"].toInt());
        getObjectsQuery.bindValue(":maxTermId", clientDetails["term_id_max"].toInt());

        if (!getObjectsQuery.exec()) {
            QString error = getObjectsQuery.lastError().text();
            localDb.close(); clientDb.close();
            QSqlDatabase::removeDatabase(localConnName); QSqlDatabase::removeDatabase(clientConnName);
            return {{"error", "Failed to query objects from client database: " + error}};
        }

        // === 4. ОБРОБЛЯЄМО РЕЗУЛЬТАТИ ===
        if (!localDb.transaction()) {
            localDb.close(); clientDb.close();
            QSqlDatabase::removeDatabase(localConnName); QSqlDatabase::removeDatabase(clientConnName);
            return {{"error", "Failed to start transaction."}};
        }

        QSqlQuery syncQuery(localDb);
        syncQuery.prepare("UPDATE OR INSERT INTO OBJECTS (CLIENT_ID, TERMINAL_ID, NAME, ADDRESS, IS_ACTIVE, IS_WORK, PHONE, REGION_NAME, LATITUDE, LONGITUDE) "
                          "VALUES (:clientId, :termId, :name, :address, :isActive, :isWork, :phone, :region, :lat, :lon) "
                          "MATCHING (CLIENT_ID, TERMINAL_ID)");
        int processedCount = 0;
        while (getObjectsQuery.next()) {
            syncQuery.bindValue(":clientId", clientId);
            syncQuery.bindValue(":termId", getObjectsQuery.value("TERMINAL_ID"));
            syncQuery.bindValue(":name", getObjectsQuery.value("NAME"));
            syncQuery.bindValue(":address", getObjectsQuery.value("ADRESS")); // Читаємо ADRESS
            bool isActive = (getObjectsQuery.value("ISACTIVE").toString().trimmed() == "T");
            bool isWork = (getObjectsQuery.value("ISWORK").toString().trimmed() == "T");
            // Новий діагностичний лог
            logInfo() << "Raw ISACTIVE: '" << getObjectsQuery.value("ISACTIVE").toString()
                      << "', Raw ISWORK: '" << getObjectsQuery.value("ISWORK").toString() << "'";
            syncQuery.bindValue(":isActive", isActive);
            syncQuery.bindValue(":isWork", isWork);
            syncQuery.bindValue(":phone", getObjectsQuery.value("PHONE"));
            syncQuery.bindValue(":region", getObjectsQuery.value("REGION_NAME"));
            syncQuery.bindValue(":lat", getObjectsQuery.value("LATITUDE"));
            syncQuery.bindValue(":lon", getObjectsQuery.value("LONGITUDE"));

            if (!syncQuery.exec()) {
                QString error = syncQuery.lastError().text();
                localDb.rollback();
                localDb.close(); clientDb.close();
                QSqlDatabase::removeDatabase(localConnName); QSqlDatabase::removeDatabase(clientConnName);
                return {{"error", "Failed to sync object: " + error}};
            }
            processedCount++;
        }

// === ЗАМІНЮЄМО БЛОК ОНОВЛЕННЯ СТАТУСУ ===
    QSqlQuery finalStatusQuery(localDb);
    QString finalStatus;
    QString finalMessage;

    // Визначаємо фінальний статус на основі результату коміту
    if (localDb.commit()) {
        finalStatus = "SUCCESS";
        finalMessage = QString("Synchronized %1 objects.").arg(processedCount);
        logInfo() << "Successfully synchronized" << processedCount << "objects for client ID" << clientId;
    } else {
        finalStatus = "FAILED";
        finalMessage = "Failed to commit transaction: " + localDb.lastError().text();
        localDb.rollback();
        logCritical() << finalMessage;
    }

    finalStatusQuery.prepare("UPDATE SYNC_STATUS SET LAST_SYNC_DATE = CURRENT_TIMESTAMP, "
                             "LAST_SYNC_STATUS = :status, LAST_SYNC_MESSAGE = :message "
                             "WHERE CLIENT_ID = :clientId");
    finalStatusQuery.bindValue(":status", finalStatus);
    finalStatusQuery.bindValue(":message", finalMessage);
    finalStatusQuery.bindValue(":clientId", clientId);

    if (!finalStatusQuery.exec()) {
        logCritical() << "FATAL: Could not write final sync status for client" << clientId << ":" << finalStatusQuery.lastError().text();
    }

    // === КОРЕКТНЕ ЗАВЕРШЕННЯ ===
    clientDb.close();
    localDb.close();
    QSqlDatabase::removeDatabase(clientConnName);
    QSqlDatabase::removeDatabase(localConnName);

    if (finalStatus == "SUCCESS") {
         return {{"status", "success"}, {"processed_count", processedCount}};
    } else {
        return {{"error", finalMessage}};
    }
}
}

// ===================================================================
// ГОЛОВНИЙ МЕТОД-"ДИСПЕТЧЕР"
// ===================================================================
QVariantMap DbManager::syncClientObjects(int clientId)
{
    // --- 1. Оновлюємо статус на "PENDING" (Виконується) ---
    // Це відбувається в основному потоці, ще до запуску фонового завдання
    QSqlQuery statusQuery(m_db);
    statusQuery.prepare("UPDATE OR INSERT INTO SYNC_STATUS (CLIENT_ID, LAST_SYNC_STATUS, LAST_SYNC_MESSAGE) "
                        "VALUES (:clientId, 'PENDING', 'Synchronization has been queued.') "
                        "MATCHING (CLIENT_ID)");
    statusQuery.bindValue(":clientId", clientId);
    if (!statusQuery.exec()) {
        logCritical() << "Could not set PENDING status for client sync:" << statusQuery.lastError().text();
        return {{"error", "Failed to initialize sync status."}};
    }

    // --- 2. Завантажуємо деталі клієнта ---
    // (використовуємо з'єднання за замовчуванням, оскільки ми ще в головному потоці)
    QJsonObject clientDetails = loadClientDetails(clientId);
    if (clientDetails.isEmpty()) {
        // Якщо клієнта не знайдено, оновлюємо статус на "FAILED"
        statusQuery.prepare("UPDATE SYNC_STATUS SET LAST_SYNC_STATUS = 'FAILED', "
                            "LAST_SYNC_MESSAGE = 'Client not found.' WHERE CLIENT_ID = :clientId");
        statusQuery.bindValue(":clientId", clientId);
        statusQuery.exec();
        return {{"error", QString("Client with ID %1 not found.").arg(clientId)}};
    }

    QString syncMethod = clientDetails["sync_method"].toString().toUpper();
    logInfo() << QString("Initiating synchronization for client %1 with method '%2'")
                     .arg(clientId).arg(syncMethod);

    // --- 3. Викликаємо відповідну стратегію ---
    // Результат тепер буде записаний у базу даних самою стратегією
    if (syncMethod == "DIRECT") {
        return syncViaDirectConnection(clientId, clientDetails);
    } else if (syncMethod == "PALANTIR") {
        return syncViaPalantir(clientId, clientDetails);
    } else if (syncMethod == "FILE") {
        return syncViaFile(clientId, clientDetails);
    } else {
        QString errorMsg = QString("Unknown synchronization method '%1'").arg(syncMethod);
        statusQuery.prepare("UPDATE SYNC_STATUS SET LAST_SYNC_STATUS = 'FAILED', "
                            "LAST_SYNC_MESSAGE = :msg WHERE CLIENT_ID = :clientId");
        statusQuery.bindValue(":msg", errorMsg);
        statusQuery.bindValue(":clientId", clientId);
        statusQuery.exec();
        return {{"error", errorMsg}};
    }
}

// 2. Заглушка для "PALANTIR"
QVariantMap DbManager::syncViaPalantir(int clientId, const QJsonObject& clientDetails)
{
    logInfo() << "Attempted to sync via PALANTIR for client" << clientId << "(not implemented yet).";
    Q_UNUSED(clientDetails); // Поки що не використовуємо
    return {{"error", "Synchronization via Palantir is not yet implemented."}};
}

// 3. Заглушка для "FILE"
QVariantMap DbManager::syncViaFile(int clientId, const QJsonObject& clientDetails)
{
    logInfo() << "Attempted to sync via FILE for client" << clientId << "(not implemented yet).";
    Q_UNUSED(clientDetails); // Поки що не використовуємо
    return {{"error", "Synchronization via File is not yet implemented."}};
}

QVariantMap DbManager::getSyncStatus(int clientId)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT LAST_SYNC_DATE, LAST_SYNC_STATUS, LAST_SYNC_MESSAGE "
                  "FROM SYNC_STATUS WHERE CLIENT_ID = :clientId");
    query.bindValue(":clientId", clientId);

    if (query.exec() && query.next()) {
        return {
            {"last_sync_date", query.value("LAST_SYNC_DATE")},
            {"status", query.value("LAST_SYNC_STATUS")},
            {"message", query.value("LAST_SYNC_MESSAGE")}
        };
    }
    // Якщо запису немає, повертаємо статус "Невідомий"
    return {{"status", "UNKNOWN"}};
}

QList<QVariantMap> DbManager::getObjects(const QVariantMap &filters)
{
    QList<QVariantMap> objects;
    QString queryString = "SELECT o.*, c.CLIENT_NAME "
                          "FROM OBJECTS o "
                          "JOIN CLIENTS c ON o.CLIENT_ID = c.CLIENT_ID";

    QStringList whereConditions;
    QVariantMap bindValues;

    if (filters.contains("clientId")) {
        whereConditions.append("o.CLIENT_ID = :clientId");
        bindValues[":clientId"] = filters["clientId"];
    }
    if (filters.contains("region")) {
        whereConditions.append("o.REGION_NAME = :region");
        bindValues[":region"] = filters["region"];
    }
    if (filters.contains("isActive")) {
        whereConditions.append("o.IS_ACTIVE = :isActive");
        bindValues[":isActive"] = filters["isActive"];
    }
    if (filters.contains("isWork")) {
        whereConditions.append("o.IS_WORK = :isWork");
        bindValues[":isWork"] = filters["isWork"];
    }

    // 6. Фільтр за ID терміналу
    if (filters.contains("terminalId")) {
        whereConditions.append("o.TERMINAL_ID = :terminalId");
        bindValues[":terminalId"] = filters["terminalId"];
    }

    // --- ЗМІНЕНО ТУТ ---
    if (filters.contains("search") && !filters["search"].toString().isEmpty()) {
        whereConditions.append("(o.NAME CONTAINING :search OR o.ADDRESS CONTAINING :search)");
        bindValues[":search"] = filters["search"];
    }
    // -------------------

    if (!whereConditions.isEmpty()) {
        queryString += " WHERE " + whereConditions.join(" AND ");
    }
    queryString += " ORDER BY o.CLIENT_ID, o.TERMINAL_ID";

    logDebug() << "Executing SQL:" << queryString;
    logDebug() << "With BIND values:" << bindValues;

    QSqlQuery query(m_db);
    query.prepare(queryString);

    for (auto it = bindValues.constBegin(); it != bindValues.constEnd(); ++it) {
        query.bindValue(it.key(), it.value());
    }

    if (!query.exec()) {
        logCritical() << "Failed to fetch filtered objects:" << query.lastError().text();
        return objects;
    }

    while (query.next()) {
        QVariantMap object;
        object["object_id"] = query.value("OBJECT_ID");
        object["client_id"] = query.value("CLIENT_ID");
        object["client_name"] = query.value("CLIENT_NAME");
        object["terminal_id"] = query.value("TERMINAL_ID");
        object["name"] = query.value("NAME");
        object["address"] = query.value("ADDRESS");
        object["region_name"] = query.value("REGION_NAME");
        object["is_active"] = query.value("IS_ACTIVE");
        object["is_work"] = query.value("IS_WORK");
        objects.append(object);
    }
    return objects;
}


QStringList DbManager::getUniqueRegionsList()
{
    QStringList regions;
    QSqlQuery query("SELECT DISTINCT REGION_NAME FROM OBJECTS WHERE REGION_NAME IS NOT NULL AND REGION_NAME <> '' ORDER BY REGION_NAME", m_db);
    if (!query.exec()) {
        logCritical() << "Failed to fetch unique regions list:" << query.lastError().text();
        return regions;
    }
    while (query.next()) {
        regions.append(query.value(0).toString());
    }
    return regions;
}


/**
 * @brief Створює запит на доступ для нового користувача Telegram.
 * Створює НЕАКТИВНИЙ запис в основній таблиці USERS.
 * @param userData JSON-об'єкт, що містить 'telegram_id', 'username', 'first_name'.
 * @return JSON-об'єкт зі статусом операції.
 */
QJsonObject DbManager::registerBotUser(const QJsonObject &userData)
{
    // 1. Отримуємо дані з JSON
    qint64 telegramId = userData["telegram_id"].toVariant().toLongLong();
    if (telegramId == 0) {
        qCritical() << "Telegram ID is missing in registration data.";
        return {{"status", "error"}, {"message", "Telegram ID is missing"}};
    }
    QString username = userData["username"].toString();
    QString firstName = userData["first_name"].toString();

    QSqlQuery query(m_db);

    // 2. Перевіряємо, чи не існує вже запиту від цього користувача
    query.prepare("SELECT REQUEST_ID FROM BOT_PENDING_REQUESTS WHERE TELEGRAM_ID = :telegram_id");
    query.bindValue(":telegram_id", telegramId);
    if (!query.exec()) {
        qCritical() << "Failed to check for existing bot request:" << query.lastError().text();
        return {{"status", "error"}, {"message", "Database check failed."}};
    }

    if (query.next()) {
        qInfo() << "Registration request for telegram_id" << telegramId << "already exists.";
        return {{"status", "exists"}, {"message", "Request for this user has already been sent."}};
    }

    // 3. Створюємо новий запит в таблиці BOT_PENDING_REQUESTS
    query.prepare("INSERT INTO BOT_PENDING_REQUESTS (TELEGRAM_ID, TELEGRAM_USERNAME, TELEGRAM_FIO) "
                  "VALUES (:telegram_id, :username, :fio)");
    query.bindValue(":telegram_id", telegramId);
    query.bindValue(":username", username);
    query.bindValue(":fio", firstName);

    if (!query.exec()) {
        qCritical() << "Failed to insert new bot request:" << query.lastError().text();
        return {{"status", "error"}, {"message", "Failed to create user request record."}};
    }

    // 4. Успішна відповідь
    qInfo() << "Successfully created a pending access request for user" << username;
    return {{"status", "success"}, {"message", "Your access request has been sent for review."}};
}

/**
 * @brief Повертає список неактивних користувачів, які надіслали запит через бота.
 * @return QJsonArray масив об'єктів, кожен з яких містить user_id, login та fio.
 */
QJsonArray DbManager::getPendingBotRequests()
{
    QJsonArray requestsArray;
    QSqlQuery query(m_db);

    // --- ОСНОВНА ЗМІНА ТУТ ---
    // Тепер ми вибираємо дані з BOT_PENDING_REQUESTS зі статусом PENDING
    query.prepare("SELECT REQUEST_ID, TELEGRAM_USERNAME, TELEGRAM_FIO FROM BOT_PENDING_REQUESTS "
                  "WHERE STATUS = 'PENDING' "
                  "ORDER BY REQUEST_DATE DESC"); // Сортуємо, щоб новіші були зверху

    if (!query.exec()) {
        qCritical() << "Failed to fetch pending bot requests:" << query.lastError().text();
        return requestsArray;
    }

    while (query.next()) {
        QJsonObject requestObject;
        // Зверніть увагу на імена полів, які ми повертаємо в JSON.
        // Ми використовуємо "request_id", "login" та "fio", щоб UI в Gandalf,
        // який ми вже написали, продовжував працювати без змін.
        requestObject["request_id"] = query.value("REQUEST_ID").toInt();
        requestObject["login"] = query.value("TELEGRAM_USERNAME").toString();
        requestObject["fio"] = query.value("TELEGRAM_FIO").toString();
        requestsArray.append(requestObject);
    }

    qInfo() << "Fetched" << requestsArray.count() << "pending bot requests.";
    return requestsArray;
}

/**
 * @brief Відхиляє запит на доступ від бота.
 * @param requestId ID запиту з таблиці BOT_PENDING_REQUESTS.
 * @return true, якщо статус успішно оновлено на 'REJECTED', інакше false.
 */
bool DbManager::rejectBotRequest(int requestId)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE BOT_PENDING_REQUESTS SET STATUS = 'REJECTED' "
                  "WHERE REQUEST_ID = :request_id AND STATUS = 'PENDING'");
    query.bindValue(":request_id", requestId);

    if (!query.exec()) {
        qCritical() << "Failed to reject bot request for ID" << requestId << ":" << query.lastError().text();
        return false;
    }

    if (query.numRowsAffected() == 0) {
        qWarning() << "Could not reject bot request for ID" << requestId << ". Request not found or status was not 'PENDING'.";
        return false;
    }

    qInfo() << "Bot request ID" << requestId << "has been rejected.";
    return true;
}

/**
 * @brief Схвалює запит, створює нового користувача (або знаходить існуючого за логіном)
 * і прив'язує до нього запит від бота.
 * @param requestId ID запиту з BOT_PENDING_REQUESTS.
 * @param login Новий корпоративний логін для користувача (або існуючий).
 * @return true, якщо всі операції пройшли успішно, інакше false.
 */
bool DbManager::approveBotRequest(int requestId, const QString& login)
{
    if (!isConnected()) {
        logCritical() << "Cannot approve bot request: no DB connection";
        return false;
    }

    // --- Починаємо транзакцію ---
    if (!m_db.transaction()) {
        logCritical() << "Failed to start transaction for approving bot request.";
        return false;
    }

    // Крок 1: Створюємо (або отримуємо) користувача за допомогою вашої процедури
    bool ok = false;
    int userId = getOrCreateUser(login, ok);

    if (!ok || userId <= 0) {
        logCritical() << "GET_OR_CREATE_USER failed for login:" << login;
        m_db.rollback();
        return false;
    }

    // Крок 2: Прив'язуємо BOT_REQUEST_ID до цього користувача
    QSqlQuery updateUsersQuery(m_db);
    updateUsersQuery.prepare("UPDATE USERS SET BOT_REQUEST_ID = :request_id "
                             "WHERE USER_ID = :user_id");
    updateUsersQuery.bindValue(":request_id", requestId);
    updateUsersQuery.bindValue(":user_id", userId);

    if (!updateUsersQuery.exec()) {
        logCritical() << "Failed to link bot request ID" << requestId << "to user ID" << userId << ":" << updateUsersQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    // Крок 3: Оновлюємо статус самого запиту на 'APPROVED'
    QSqlQuery updateRequestsQuery(m_db);
    updateRequestsQuery.prepare("UPDATE BOT_PENDING_REQUESTS SET STATUS = 'APPROVED' "
                                "WHERE REQUEST_ID = :request_id AND STATUS = 'PENDING'");
    updateRequestsQuery.bindValue(":request_id", requestId);

    if (!updateRequestsQuery.exec()) {
        logCritical() << "Failed to set request status to 'APPROVED' for ID" << requestId << ":" << updateRequestsQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    // Якщо все пройшло добре, підтверджуємо транзакцію
    if (!m_db.commit()) {
        logCritical() << "Failed to commit approve transaction:" << m_db.lastError().text();
        m_db.rollback(); // На випадок, якщо коміт не вдався
        return false;
    }

    qInfo() << "Successfully approved bot request ID" << requestId << "and linked to user" << login << "(ID:" << userId << ")";
    return true;
}


/**
 * @brief Прив'язує запит від бота до існуючого облікового запису користувача.
 * @param requestId ID запиту з BOT_PENDING_REQUESTS.
 * @param existingUserId ID існуючого користувача з таблиці USERS.
 * @return true, якщо всі операції пройшли успішно, інакше false.
 */
bool DbManager::linkBotRequest(int requestId, int existingUserId)
{
    if (!isConnected()) {
        logCritical() << "Cannot link bot request: no DB connection";
        return false;
    }

    // --- Починаємо транзакцію ---
    if (!m_db.transaction()) {
        logCritical() << "Failed to start transaction for linking bot request.";
        return false;
    }

    // Крок 1: Прив'язуємо BOT_REQUEST_ID до існуючого користувача
    QSqlQuery updateUsersQuery(m_db);
    updateUsersQuery.prepare("UPDATE USERS SET BOT_REQUEST_ID = :request_id "
                             "WHERE USER_ID = :user_id");
    updateUsersQuery.bindValue(":request_id", requestId);
    updateUsersQuery.bindValue(":user_id", existingUserId);

    if (!updateUsersQuery.exec()) {
        logCritical() << "Failed to link bot request ID" << requestId << "to user ID" << existingUserId << ":" << updateUsersQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    // Крок 2: Оновлюємо статус самого запиту на 'LINKED'
    QSqlQuery updateRequestsQuery(m_db);
    updateRequestsQuery.prepare("UPDATE BOT_PENDING_REQUESTS SET STATUS = 'LINKED' "
                                "WHERE REQUEST_ID = :request_id AND STATUS = 'PENDING'");
    updateRequestsQuery.bindValue(":request_id", requestId);

    if (!updateRequestsQuery.exec()) {
        logCritical() << "Failed to set request status to 'LINKED' for ID" << requestId << ":" << updateRequestsQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    // Якщо все пройшло добре, підтверджуємо транзакцію
    if (!m_db.commit()) {
        logCritical() << "Failed to commit link transaction:" << m_db.lastError().text();
        m_db.rollback();
        return false;
    }

    qInfo() << "Successfully linked bot request ID" << requestId << "to existing user ID" << existingUserId;
    return true;
}

/**
 * @brief Виконує повну перевірку статусу користувача бота за його Telegram ID.
 * @param telegramId Унікальний Telegram ID користувача.
 * @return QJsonObject, що описує статус:
 * - {"status": "NEW"} - Користувач невідомий.
 * - {"status": "PENDING"} - Запит на розгляді.
 * - {"status": "BLOCKED"} - Запит відхилено або користувача заблоковано.
 * - {"status": "ACTIVE_USER", "fio": "..."} - Активний користувач.
 * - {"status": "ACTIVE_ADMIN", "fio": "..."} - Активний адміністратор.
 */
QJsonObject DbManager::getBotUserStatus(qint64 telegramId)
{
    if (!isConnected()) {
        logCritical() << "Cannot get bot user status: no DB connection";
        return {{"status", "ERROR"}, {"message", "Database connection failed"}};
    }

    QSqlQuery statusQuery(m_db);
    // Крок 1: Шукаємо користувача в таблиці запитів
    statusQuery.prepare("SELECT REQUEST_ID, STATUS FROM BOT_PENDING_REQUESTS WHERE TELEGRAM_ID = :id");
    statusQuery.bindValue(":id", telegramId);

    if (!statusQuery.exec()) {
        logCritical() << "Failed to check bot pending requests:" << statusQuery.lastError().text();
        return {{"status", "ERROR"}, {"message", "Database query failed"}};
    }

    // --- Стан 1: "Новий" ---
    if (!statusQuery.next()) {
        // Користувача немає в таблиці запитів
        return {{"status", "NEW"}};
    }

    // --- Користувач знайдений, аналізуємо його статус ---
    int requestId = statusQuery.value(0).toInt();
    QString requestStatus = statusQuery.value(1).toString();

    // --- Стан 2: "Очікуючий" ---
    if (requestStatus == "PENDING") {
        return {{"status", "PENDING"}};
    }

    // --- Стан 3: "Заблокований" (на рівні запиту) ---
    if (requestStatus == "REJECTED") {
        return {{"status", "BLOCKED"}};
    }

    // --- Стан 4/5: "Активний" (APPROVED або LINKED) ---
    if (requestStatus == "APPROVED" || requestStatus == "LINKED") {

        // Тепер нам потрібно знайти, до якого USER_ID прив'язаний цей запит
        QSqlQuery userQuery(m_db);
        userQuery.prepare("SELECT USER_ID FROM USERS WHERE BOT_REQUEST_ID = :request_id");
        userQuery.bindValue(":request_id", requestId);

        if (!userQuery.exec() || !userQuery.next()) {
            // Це аномалія: запит схвалено, але не прив'язано до користувача
            logCritical() << "Data inconsistency: Bot request" << requestId << "is approved/linked but not attached to any user.";
            return {{"status", "BLOCKED"}}; // Блокуємо, оскільки це помилка конфігурації
        }

        int userId = userQuery.value(0).toInt();

        // У нас є ID користувача, завантажуємо повний профіль, щоб перевірити IS_ACTIVE та ролі
        // Використовуємо наш існуючий надійний метод loadUser
        User* user = loadUser(userId);

        if (!user) {
            logCritical() << "Failed to load user profile for USER_ID" << userId;
            return {{"status", "BLOCKED"}}; // Не вдалося завантажити профіль
        }

        if (!user->isActive()) {
            delete user;
            return {{"status", "BLOCKED"}}; // Користувач заблокований в Gandalf
        }

        // Визначаємо рівень доступу
        QJsonObject response;
        response["fio"] = user->fio();

        if (user->hasRole("Адміністратор")) {
            response["status"] = "ACTIVE_ADMIN";
        } else {
            response["status"] = "ACTIVE_USER";
        }

        delete user;
        return response;
    }

    // На випадок, якщо з'явиться невідомий статус
    return {{"status", "BLOCKED"}};
}

//

/**
 * @brief (ВИПРАВЛЕНО 3.0) Знаходить ID активного користувача за його Telegram ID.
 * Використовує JOIN для зв'язку USERS та BOT_PENDING_REQUESTS.
 */
int DbManager::findUserIdByTelegramId(qint64 telegramId)
{
    if (!isConnected() || telegramId == 0) return -1;

    QSqlQuery query(m_db);

    // --- ОСНОВНА ЗМІНА: ПРАВИЛЬНИЙ ЗАПИТ ДО СХЕМИ ---
    // Ми шукаємо активного користувача (IS_ACTIVE = 1),
    // який прив'язаний до запиту з цим telegram_id
    query.prepare(
        "SELECT u.USER_ID "
        "FROM USERS u "
        "JOIN BOT_PENDING_REQUESTS b ON u.BOT_REQUEST_ID = b.REQUEST_ID "
        "WHERE b.TELEGRAM_ID = ? AND u.IS_ACTIVE = 1"
        );
    // Використовуємо addBindValue() для позиційного параметра
    query.addBindValue(telegramId);
    // --- КІНЕЦЬ ЗМІН ---

    if (query.exec() && query.next()) {
        return query.value(0).toInt(); // Повертаємо USER_ID
    }

    if (query.lastError().isValid()) {
        logCritical() << "Telegram ID validation (JOIN) query failed:"
                      << query.lastError().driverText();
    }

    // Якщо не знайдено (або користувач неактивний), повертаємо -1
    return -1;
}

//

/**
 * @brief (НОВИЙ) Повертає список активних користувачів, які прив'язані до бота.
 * @return QJsonArray масив об'єктів (user_id, login, fio).
 */
QJsonArray DbManager::getActiveBotUsers()
{
    QJsonArray usersArray;
    if (!isConnected()) return usersArray;

    QSqlQuery query(m_db);

    // Шукаємо користувачів, які активні (IS_ACTIVE = 1)
    // і мають прив'язку до запиту бота (BOT_REQUEST_ID > 0)
    // Також приєднуємо таблицю запитів, щоб отримати Telegram ПІБ
    query.prepare(
        "SELECT u.USER_ID, u.USER_LOGIN, u.USER_FIO, b.TELEGRAM_FIO "
        "FROM USERS u "
        "JOIN BOT_PENDING_REQUESTS b ON u.BOT_REQUEST_ID = b.REQUEST_ID "
        "WHERE u.IS_ACTIVE = 1 "
        "ORDER BY u.USER_FIO"
        );

    if (!query.exec()) {
        logCritical() << "Failed to fetch active bot users:" << query.lastError().driverText();
        return usersArray;
    }

    while (query.next()) {
        QJsonObject userObject;
        userObject["user_id"] = query.value("USER_ID").toInt();
        userObject["login"] = query.value("USER_LOGIN").toString();
        // Пріоритет - корпоративне ПІБ, якщо ні - телеграм ПІБ
        QString fio = query.value("USER_FIO").toString();
        if (fio.isEmpty()) {
            fio = query.value("TELEGRAM_FIO").toString();
        }
        userObject["fio"] = fio;

        usersArray.append(userObject);
    }

    logInfo() << "Fetched" << usersArray.count() << "active bot users.";
    return usersArray;
}


//

/**
 * @brief (НОВИЙ/ВИПРАВЛЕНИЙ) Повертає список АЗС (OBJECTS) для клієнта.
 * Використовує коректні імена таблиць.
 */
QJsonArray DbManager::getStationsForClient(int userId, int clientId)
{
    QJsonArray stationsArray;
    if (!isConnected()) return stationsArray;

    QSqlQuery query(m_db);

    // (Примітка: ми все ще перевіряємо userId, щоб переконатися,
    // що цей користувач аутентифікований, але сам SQL-запит
    // може не потребувати userId, якщо логіка прав інша.
    // Для початку ми просто перевіряємо, що АЗС належать клієнту.)

    // ВИПРАВЛЕНИЙ ЗАПИТ
    query.prepare(
        "SELECT o.TERMINAL_ID, o.NAME, o.IS_ACTIVE, o.IS_WORK "
        "FROM OBJECTS o "
        "WHERE o.CLIENT_ID = :client_id "
        "ORDER BY o.TERMINAL_ID"
        );
    // query.bindValue(":user_id", userId); // Цей рядок, ймовірно, не потрібен
    query.bindValue(":client_id", clientId);

    if (!query.exec()) {
        logCritical() << "Failed to fetch stations (OBJECTS) for client" << clientId
                      << "for user" << userId << ":" << query.lastError().driverText();
        return stationsArray;
    }

    while (query.next()) {
        QJsonObject station;
        // Використовуємо імена полів з вашої таблиці OBJECTS
        station["terminal_no"] = query.value("TERMINAL_ID").toString(); // Або toInt()
        station["name"] = query.value("NAME").toString();
        station["is_active"] = query.value("IS_ACTIVE").toBool();
        station["is_working"] = query.value("IS_WORK").toBool();
        stationsArray.append(station);
    }

    return stationsArray;
}

//

/**
 * @brief (НОВИЙ/ВИПРАВЛЕНИЙ) Повертає деталі однієї АЗС (OBJECT) за її номером.
 */
QJsonObject DbManager::getStationDetails(int userId, int clientId, const QString& terminalNo)
{
    if (!isConnected()) return {{"error", "Database not connected"}};

    QSqlQuery query(m_db);

    // ВИПРАВЛЕНИЙ ЗАПИТ
    query.prepare(
        "SELECT o.OBJECT_ID, o.TERMINAL_ID, o.NAME, o.IS_ACTIVE, o.IS_WORK " // (o.LAST_DATA - я прибрав, бо не бачу її у вашій схемі)
        "FROM OBJECTS o "
        "WHERE o.CLIENT_ID = :client_id AND o.TERMINAL_ID = :terminal_no"
        );
    // query.bindValue(":user_id", userId); // Також, ймовірно, не потрібен
    query.bindValue(":client_id", clientId);
    query.bindValue(":terminal_no", terminalNo.toInt()); // TERMINAL_ID - це INTEGER

    if (!query.exec()) {
        logCritical() << "Failed to fetch station details:" << query.lastError().driverText();
        return {{"error", "Query failed"}};
    }

    if (query.next()) {
        QJsonObject station;
        station["station_id"] = query.value("OBJECT_ID").toInt();
        station["terminal_no"] = query.value("TERMINAL_ID").toString();
        station["name"] = query.value("NAME").toString();
        station["is_active"] = query.value("IS_ACTIVE").toBool();
        station["is_working"] = query.value("IS_WORK").toBool();
        // station["last_data"] = ... (якщо є)
        return station;
    }

    // Якщо query.next() не спрацював
    return {{"error", "Station not found or access denied"}};
}
