#include <QCoreApplication>
#include <QFileInfo>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QTextStream>
#include <QTimer>
#include <iostream>


#include "Oracle/Logger.h"
#include "Oracle/AppParams.h"
#include "Oracle/ConfigManager.h"
#include "Oracle/DbManager.h"
#include "WebServer.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // Встановлюємо базову інформацію про додаток
    QCoreApplication::setApplicationName("Conduit");
    QCoreApplication::setApplicationVersion("1.0");

    // Отримуємо ім'я виконуваного файлу для ініціалізації логера
    const QString appName = QFileInfo(QCoreApplication::applicationFilePath()).baseName();
    preInitLogger(appName);


    // --- ОБРОБКА АРГУМЕНТІВ КОМАНДНОГО РЯДКА ---
    QCommandLineParser parser;
    parser.setApplicationDescription("Conduit Web Server");
    parser.addHelpOption();
    parser.addVersionOption();

    // Додаємо нашу опцію для режиму налаштування
    QCommandLineOption setupOption("config", "Run in interactive configuration mode.");
    parser.addOption(setupOption);

    // Розбираємо аргументи
    parser.process(a);

    ConfigManager configManager;

    // --- СЦЕНАРІЙ 1: ЗАПУСК В РЕЖИМІ НАЛАШТУВАННЯ ---
    if (parser.isSet(setupOption)) {
        if (configManager.load()) {
            QTextStream output(stdout);
            output << "Configuration file already exists." << Qt::endl;
            configManager.printToConsole();
            output << "Do you want to re-configure? (y/n): ";
            output.flush();

            QTextStream input(stdin);
            QString response = input.readLine().trimmed().toLower();
            if (response == "y" || response == "yes") {
                configManager.createInteractively();
            } else {
                output << "Configuration remains unchanged." << Qt::endl;
            }
        } else {
            QTextStream output(stdout);
            output << "Configuration file not found. Starting setup..." << Qt::endl;
            configManager.createInteractively();
        }
        return 0; // Завершуємо роботу після налаштування
    }

    // --- СЦЕНАРІЙ 2: ЗАПУСК У ЗВИЧАЙНОМУ РЕЖИМІ ---
    logInfo() << "Application starting in normal mode...";

    if (!configManager.load()) {
        logCritical() << "Configuration file not found or is corrupted.";
        // Використовуємо std::cerr для гарантованого виводу, навіть якщо логер не працює
        std::cerr << "\nERROR: Configuration is missing.\n"
                  << "Please run the application with the --config flag to set it up:\n"
                  << qPrintable(appName) << " --config\n" << std::endl;
        return 1; // Завершуємо роботу з кодом помилки
    }

    logInfo() << "Configuration loaded successfully.";
    configManager.printToConsole(); // Виведемо параметри в лог для перевірки

    if (!DbManager::instance().connect(configManager)) {
        logCritical() << "Application will be terminated due to database connection failure.";
        return 1;
    }

    // --- ЗАВАНТАЖЕННЯ НАЛАШТУВАНЬ З БД ---
    AppParams& params = AppParams::instance();

    // 1. Завантажуємо глобальні налаштування
    QVariantMap globalSettings = DbManager::instance().loadSettings("Global");
    for (auto it = globalSettings.constBegin(); it != globalSettings.constEnd(); ++it) {
        params.setParam("Global", it.key(), it.value());
    }

    // 2. Завантажуємо налаштування конкретного додатку
    QVariantMap appSettings = DbManager::instance().loadSettings(appName);
    for (auto it = appSettings.constBegin(); it != appSettings.constEnd(); ++it) {
        params.setParam(appName, it.key(), it.value());
    }

    if (globalSettings.isEmpty() && appSettings.isEmpty()) {
        logWarning() << "No settings found in the database for this application.";
    } else {
        logInfo() << "Successfully loaded" << (globalSettings.count() + appSettings.count()) << "settings from the database.";
    }

    reconfigureLoggerFilters();

    // Отримуємо порт з налаштувань бази даних, з резервним значенням 8080
    quint16 port = params.getParam(appName, "ServerPort", 8080).toUInt();

    // // --- ПОЧАТОК ЗМІН ---
    // // 1. Отримуємо ключ, який ConfigManager тепер вміє читати
    // QString botKey = configManager.getBotApiKey();
    // if (botKey.isEmpty() || botKey == "YOUR_DEFAULT_BOT_KEY_HERE" || botKey.length() < 32) {
    //     logWarning() << "Bot API Key is not configured or is too weak. Bot authentication will fail.";
    //     // Ми не зупиняємо сервер, оскільки Gandalf має працювати,
    //     // але виводимо попередження.
    // }

    // Спроба 1: База Даних (для продакшену найнадійніше)
    QString botKey = params.getParam("Global", "BotApiKey", "").toString();

    // Спроба 2: Файл (резерв, або для тестового середовища)
    if (botKey.isEmpty() || botKey.length() < 10) {
        logInfo() << "Key not found in DB, trying config.json...";
        botKey = configManager.getBotApiKey();
    }

    // Логуємо (безпечно, тільки перші 4 символи), щоб ви БАЧИЛИ, що він прочитав
    logInfo() << "Server using Bot API Key starting with:" << botKey.left(4);

    // 2. Передаємо ключ у новий конструктор WebServer
    WebServer webServer(port, botKey);
    // --- КІНЕЦЬ ЗМІН ---

    if (!webServer.start()) {
        logCritical() << "Не вдалося ініціалізувати та запустити веб-сервер. Завершення роботи.";
        return 1; // Вийти з помилкою
    }

    logInfo() << "Додаток запущено. Натисніть Ctrl+C для виходу.";

    // a.exec() запускає цикл подій Qt, що необхідно для того, щоб сервер
    // обробляв вхідні запити. Додаток буде працювати, поки ви його не закриєте.
    return a.exec();
}
