#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QSqlDatabase>
#include <QString>
#include <QJsonObject>
#include <QDateTime>
#include <QJsonArray>
#include <QMutex>


class ConfigManager;
class User;

class DbManager
{
public:
    // Статичний метод для доступу до єдиного екземпляра
    static DbManager& instance();

    bool connect(const ConfigManager& config);
    bool isConnected() const;
    QString lastError() const;
    QVariantMap loadSettings(const QString& appName);
    bool saveSettings(const QString& appName, const QVariantMap& settings);

    int getOrCreateUser(const QString& login, bool& ok);
    User* loadUser(int userId);
    QList<User*> loadAllUsers();
    QList<QVariantMap> loadAllRoles();
    bool updateUser(int userId, const QJsonObject& userData);
    bool saveSession(int userId, const QByteArray& tokenHash, const QDateTime& expiresAt);
    int findUserIdByToken(const QByteArray& tokenHash);
    QList<QVariantMap> loadAllClients();
    int createClient(const QString& clientName);
    QJsonObject loadClientDetails(int clientId);
    QList<QVariantMap> loadAllIpGenMethods();
    static bool testConnection(const QJsonObject& config, QString& error);
    bool updateClient(int clientId, const QJsonObject& clientData);
    QVariantMap syncClientObjects(int clientId);
    QVariantMap getSyncStatus(int clientId);
    QList<QVariantMap> getObjects(const QVariantMap& filters);
    QStringList getUniqueRegionsList();
    QJsonObject registerBotUser(const QJsonObject& userData);
    QJsonArray getPendingBotRequests();
    QJsonArray getActiveBotUsers();

    QJsonArray getStationsForClient(int userId, int clientId);
    QJsonObject getStationDetails(int userId, int clientId, const QString& terminalNo);

    bool rejectBotRequest(int requestId);
    bool approveBotRequest(int requestId, const QString& login);
    bool linkBotRequest(int requestId, int existingUserId);
    QJsonObject getBotUserStatus(qint64 telegramId);
    int findUserIdByTelegramId(qint64 telegramId);


    // --- Методи для управління завданнями експорту (EXPORT_TASKS) ---
    // 1. Завантажити весь список завдань (для табличного відображення)
    QList<QVariantMap> loadAllExportTasks();

    // 2. Завантажити деталі одного завдання (включаючи BLOB SQL)
    QJsonObject loadExportTaskById(int taskId);

    // 3. Створити нове завдання
    int createExportTask(const QJsonObject& taskData);

    // 4. Оновити існуюче завдання
    bool updateExportTask(int taskId, const QJsonObject& taskData);

//    bool processObjectsSync(int clientId, const QJsonArray& objects, QString& errorOut);

    // Новий метод для прямої синхронізації
    QVariantMap syncViaDirect(int clientId, const QJsonObject& clientDetails);

    // Повертає масив JSON зі статусами всіх клієнтів
    QJsonArray getDashboardData();

    // Отримати список РРО для конкретного терміналу (Для Бота та Gandalf)
    QJsonArray getPosDataByTerminal(int clientId, int terminalId);

    // Отримати список резервуарів для терміналу
    QJsonArray getTanksByTerminal(int clientId, int terminalId);

    // Отримати конфігурацію ПРК
    QJsonArray getDispenserConfigByTerminal(int clientId, int terminalId);
    /**
     * @brief Оновлює лише Redmine User ID для існуючого користувача.
     * @param localUserId Локальний ID користувача (з нашої БД).
     * @param redmineId Справжній числовий ID Redmine.
     * @return true у разі успіху.
     */
    bool updateRedmineUserId(int localUserId, int redmineId);

private:
    DbManager(); // Конструктор тепер приватний
    ~DbManager();

    // --- Методи-стратегії для синхронізації ---
    QVariantMap syncViaDirectConnection(int clientId, const QJsonObject& clientDetails);
    QVariantMap syncViaPalantir(int clientId, const QJsonObject& clientDetails);
    QVariantMap syncViaFile(int clientId, const QJsonObject& clientDetails);

    // Отримує назву таблиці та поля для пошуку за іменем файлу
    QPair<QString, QString> getExportTaskInfo(const QString& jsonFileName);

    // Універсальний метод імпорту
    bool processGenericSync(int clientId, const QString& tableName, const QString& matchFields,
                            const QString& deleteStrategy,
                            const QJsonArray& data, QString& errorOut);

    // Забороняємо копіювання
    DbManager(const DbManager&) = delete;
    DbManager& operator=(const DbManager&) = delete;
private:
    QSqlDatabase m_db;
    QString m_lastError;
    QMutex m_dbMutex;
};
#endif // DBMANAGER_H
