#include <QCoreApplication>
#include <QFileInfo>
#include <QTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QVariantMap>
#include <QCoreApplication>
#include <QFileInfo>
#include "Oracle/Logger.h"
#include "Oracle/AppParams.h"
#include "Bot/Bot.h" // Наш новий головний клас

#include "Oracle/Logger.h"
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




// ... функція setupBotConfiguration() залишається без змін ...

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("Isengard");
    preInitLogger(QFileInfo(a.applicationFilePath()).baseName());

    logInfo() << "Isengard application starting...";

    // 1. Отримуємо налаштування
    QVariantMap config = setupBotConfiguration();
    const QString botToken = config["botToken"].toString();
    const QString apiUrl = config["apiUrl"].toString();

    if (botToken.isEmpty() || botToken.startsWith("YOUR") || apiUrl.isEmpty()) {
        logCritical() << "Bot token or API URL is not set. Shutting down.";
        return 1;
    }

    // 2. Налаштовуємо глобальні параметри для ApiClient
    AppParams::instance().setParam("Global", "ApiBaseUrl", apiUrl);

    // 3. Створюємо і запускаємо "мозок" бота
    Bot bot(botToken);
    bot.start();

    // 4. Запускаємо цикл обробки подій
    return a.exec();
}
