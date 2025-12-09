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

// --- ДОДАЄМО ЦЕЙ INCLUDE ---
// Ми маємо підключити ApiClient, щоб викликати setBotApiKey()
#include "Oracle/ApiClient.h"

// Змінна для зберігання ID останнього оновлення
static qint64 g_lastUpdateId = 0;


bool loadGlobalSettingsSync()
{
    logInfo() << "Synchronously loading global settings from API...";

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(5000); // Таймаут 5 секунд

    bool success = false;

    // Підключаємо слоти для отримання відповіді (використовуємо ApiClient::fetchSettings)
    QObject::connect(&ApiClient::instance(), &ApiClient::settingsFetched,
                     QCoreApplication::instance(), // <-- КОНТЕКСТ
                     [&loop, &success](const QVariantMap& settingsMap) {
                         AppParams::instance().setScopedParams("Global", settingsMap);
                         logInfo() << "Successfully synchronized Global settings from server.";
                         success = true;
                         loop.quit();
                     }, Qt::DirectConnection);

    QObject::connect(&ApiClient::instance(), &ApiClient::settingsFetchFailed,
                     QCoreApplication::instance(), // <-- КОНТЕКСТ
                     [&loop](const ApiError& error) {
                         logCritical() << "Failed to load Global settings synchronously:" << error.errorString;
                         loop.quit();
                     }, Qt::DirectConnection);

    // Обробка таймауту
    QObject::connect(&timer, &QTimer::timeout, [&loop](){
        logCritical() << "Synchronous settings loading timed out after 5 seconds.";
        loop.quit();
    });

    // Запускаємо запит
    ApiClient::instance().fetchSettings("Global");
    timer.start();
    loop.exec(); // Блокуємо, доки не отримаємо відповідь або таймаут

    return success;
}


// --- ОНОВЛЕНА ФУНКЦІЯ ---
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
            {"api_server", serverObj},
            // --- ДОДАНО КЛЮЧ ---
            {"bot_api_key", "YOUR_SECRET_KEY_HERE"}
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

        // --- ДОДАНО ЗЧИТУВАННЯ КЛЮЧА ---
        configMap["botApiKey"] = root["bot_api_key"].toString();

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

// --- ОНОВЛЕНА ФУНКЦІЯ ---
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
    // --- ОТРИМУЄМО КЛЮЧ ---
    const QString botApiKey = config["botApiKey"].toString();

    // --- ДОДАНО ПЕРЕВІРКУ КЛЮЧА ---
    if (botToken.isEmpty() || botToken.startsWith("YOUR") || apiUrl.isEmpty() ||
        botApiKey.isEmpty() || botApiKey.startsWith("YOUR"))
    {
        logCritical() << "Bot token, API URL or Bot API Key is not set. Shutting down.";
        return 1;
    }

    // 2. Налаштовуємо глобальні параметри для ApiClient
    //    (ЗАЛИШАЄМО ВАШ ПРАВИЛЬНИЙ КОД)
    AppParams::instance().setParam("Global", "ApiBaseUrl", apiUrl);

    // --- ДОДАЄМО НАЛАШТУВАННЯ КЛЮЧА ---
    // (ApiClient візьме ApiBaseUrl сам, а ми передамо йому ключ)
    ApiClient::instance().setBotApiKey(botApiKey);

    if (!loadGlobalSettingsSync()) {
        logCritical() << "Cannot start bot: Failed to initialize global settings (Redmine URL/Jira URL).";
        return 1;
    }

    // 3. Створюємо і запускаємо "мозок" бота
    Bot bot(botToken);
    bot.start();

    // 4. Запускаємо цикл обробки подій
    return a.exec();
}
