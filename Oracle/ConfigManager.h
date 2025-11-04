#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QString>

class ConfigManager
{
public:
    ConfigManager();

    // Завантажує конфігурацію з файлу. Повертає true, якщо успішно.
    bool load();

    // Зберігає поточну конфігурацію у файл. Повертає true, якщо успішно.
    bool save();

    // Запускає інтерактивний режим для створення конфігурації з консолі.
    void createInteractively();

    // Виводить поточні параметри в консоль.
    void printToConsole() const;

    // Методи доступу до параметрів
    QString getDbHost() const;
    QString getDbPath() const;
    int getDbPort() const;
    QString getDbUser() const;
    QString getDbPassword() const;
    QString getBotApiKey() const;

private:
    // Повертає повний шлях до файлу конфігурації.
    QString getConfigFilePath() const;

    // Внутрішні змінні для зберігання параметрів
    QString m_dbHost;
    QString m_dbPath;
    int m_dbPort;
    QString m_dbUser;
    QString m_dbPassword;
    QString m_botApiKey;
};

#endif // CONFIGMANAGER_H
