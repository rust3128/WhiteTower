#include <QCoreApplication>
#include <QFileInfo>
#include <QTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QVariantMap>

#include "Oracle/Logger.h"
#include "Bot/TelegramClient.h"
#include "Oracle/ApiClient.h"
#include "Oracle/AppParams.h"

// Змінна для зберігання ID останнього оновлення
static qint64 g_lastUpdateId = 0;

// --- ОНОВЛЕНА ФУНКЦІЯ ---
// Тепер повертає QVariantMap з усіма налаштуваннями
QVariantMap setupBotConfiguration()
{
    QString configPath = QCoreApplication::applicationDirPath() + "/Config/Isengard.config.json";
    QFile configFile(configPath);
    QVariantMap configMap;

    if (!configFile.exists()) {
        logWarning() << "Config file not found. Creating a default one at" << configPath;
        QDir().mkpath(QFileInfo(configFile).absolutePath());

        if (!configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            logCritical() << "Failed to create default config file!";
            return configMap;
        }
        // Створюємо JSON за замовчуванням
        QJsonObject serverObj{{"host", "localhost"}, {"port", 8080}};
        QJsonObject rootObj{
            {"bot_token", "YOUR_TELEGRAM_BOT_TOKEN_HERE"},
            {"api_server", serverObj}
        };
        configFile.write(QJsonDocument(rootObj).toJson(QJsonDocument::Indented));
        configFile.close();
        logInfo() << "Please edit the new config file.";
    }

    if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        logCritical() << "Failed to open config file for reading!";
        return configMap;
    }

    QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll());
    if (doc.isObject()) {
        QJsonObject root = doc.object();
        // Зчитуємо токен
        configMap["botToken"] = root["bot_token"].toString();

        // Зчитуємо адресу сервера
        QJsonObject serverObj = root["api_server"].toObject();
        QString host = serverObj["host"].toString("localhost");
        int port = serverObj["port"].toInt(8080);
        configMap["apiUrl"] = QString("http://%1:%2").arg(host).arg(port);

        logInfo() << "Successfully loaded configuration from" << configPath;
    } else {
        logCritical() << "Config file is corrupted or has invalid format.";
    }
    return configMap;
}


int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("Isengard");
    preInitLogger(QFileInfo(a.applicationFilePath()).baseName());

    logInfo() << "Isengard bot starting...";

    // 1. Отримуємо налаштування
    QVariantMap config = setupBotConfiguration();
    const QString botToken = config["botToken"].toString();
    const QString apiUrl = config["apiUrl"].toString();

    // 2. Перевіряємо налаштування
    if (botToken.isEmpty() || botToken.startsWith("YOUR")) {
        logCritical() << "Bot token is missing or not set. Shutting down.";
        return 1;
    }
    if (apiUrl.isEmpty()) {
        logCritical() << "API Server URL is missing. Shutting down.";
        return 1;
    }

    // 3. КЛЮЧОВИЙ КРОК: Зберігаємо URL в глобальні параметри ДО створення ApiClient
    AppParams::instance().setParam("Global", "ApiBaseUrl", apiUrl); // <--- ВИРІШЕННЯ ПРОБЛЕМИ

    // 4. Тепер, коли параметр встановлено, отримуємо екземпляр ApiClient
    // Його конструктор тепер успішно знайде потрібний URL в AppParams.
    ApiClient& apiClient = ApiClient::instance();

    // Створюємо клієнт для Telegram
    TelegramClient telegramClient(botToken);

    // 5. Налаштовуємо обробники відповідей від сервера
    QObject::connect(&apiClient, &ApiClient::botUserRegistered, [](const QJsonObject& response) {
        logInfo() << "Server response: User successfully registered." << response;
    });
    QObject::connect(&apiClient, &ApiClient::botUserRegistrationFailed, [](const ApiError& error) {
        logCritical() << "Server response: User registration failed:" << error.errorString;
    });

    // ... решта коду залишається без змін ...

    QObject::connect(&telegramClient, &TelegramClient::updatesReceived, [&](const QJsonArray& updates) {
        if (updates.isEmpty()) {
            telegramClient.getUpdates(g_lastUpdateId + 1);
            return;
        }

        for (const QJsonValue& updateValue : updates) {
            QJsonObject updateObj = updateValue.toObject();
            g_lastUpdateId = updateObj["update_id"].toVariant().toLongLong();

            if (updateObj.contains("message")) {
                QJsonObject message = updateObj["message"].toObject();
                QString text = message["text"].toString();

                if (text == "/start") {
                    QJsonObject from = message["from"].toObject();
                    qint64 userId = from["id"].toVariant().toLongLong();
                    QString username = from["username"].toString();
                    QString firstName = from["first_name"].toString();

                    logInfo() << "User" << username << "(" << userId << ") sent /start. Sending registration request...";

                    QJsonObject payload;
                    payload["telegram_id"] = userId;
                    payload["username"] = username;
                    payload["first_name"] = firstName;

                    apiClient.registerBotUser(payload);
                }
            }
        }
        telegramClient.getUpdates(g_lastUpdateId + 1);
    });

    QObject::connect(&telegramClient, &TelegramClient::errorOccurred, [&](const QString& error){
        logCritical() << "An error occurred with Telegram:" << error;
        QTimer::singleShot(5000, [&](){ telegramClient.getUpdates(g_lastUpdateId + 1); });
    });

    logInfo() << "Starting to poll for updates from Telegram...";
    telegramClient.getUpdates(0);

    return a.exec();
}
