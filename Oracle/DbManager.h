#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QSqlDatabase>
#include <QString>
#include <QJsonObject>
#include <QDateTime>


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

private:
    DbManager(); // Конструктор тепер приватний
    ~DbManager();

    // Забороняємо копіювання
    DbManager(const DbManager&) = delete;
    DbManager& operator=(const DbManager&) = delete;
private:
    QSqlDatabase m_db;
};
#endif // DBMANAGER_H
