#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QSqlDatabase>
#include <QString>

class ConfigManager; // Попереднє оголошення, щоб уникнути циклічної залежності

class DbManager
{
public:
    DbManager();
    ~DbManager();

    // Метод для підключення до БД з використанням параметрів з ConfigManager
    bool connect(const ConfigManager& config);

    // Перевірка статусу підключення
    bool isConnected() const;

    // Метод для отримання останньої помилки
    QString lastError() const;
    // Завантажує налаштування для вказаного додатку та глобальні
    QVariantMap loadSettings(const QString& appName);


private:
    QSqlDatabase m_db;
};

#endif // DBMANAGER_H
