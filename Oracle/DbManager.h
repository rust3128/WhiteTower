#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QSqlDatabase>
#include <QString>

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

    int getOrCreateUser(const QString& login, bool& ok);
    User* loadUser(int userId);
    QList<User*> loadAllUsers();
    QList<QVariantMap> loadAllRoles();

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
