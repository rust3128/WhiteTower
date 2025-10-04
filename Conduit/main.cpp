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

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // Встановлюємо базову інформацію про додаток
    QCoreApplication::setApplicationName("Conduit");
    QCoreApplication::setApplicationVersion("1.0");

    // Отримуємо ім'я виконуваного файлу для ініціалізації логера
    const QString appName = QFileInfo(QCoreApplication::applicationFilePath()).baseName();
    initLogger(appName);


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

    // --- ПІДКЛЮЧЕННЯ ДО БАЗИ ДАНИХ ---
    DbManager dbManager;
    if (!dbManager.connect(configManager)) {
        logCritical() << "Application will be terminated due to database connection failure.";
        return 1; // Завершуємо роботу з помилкою
    }
    // ------------------------------------

    AppParams& params = AppParams::instance();
    // Тут у майбутньому ми будемо передавати завантажені параметри в AppParams

    logInfo() << "Server is running... (simulation)";

    // Поки що сервер не запускаємо, просто виходимо через 3 секунди
    QTimer::singleShot(3000, &a, &QCoreApplication::quit);
    return a.exec();
}
