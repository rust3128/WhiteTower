#include "Oracle/Logger.h"
#include "Oracle/AppParams.h"
#include "ApplicationController.h" // Наш новий клас

#include <QApplication>
#include <QFileInfo>
#include <QMessageBox>
#include <QDir>
#include <QTranslator>
#include <QJsonDocument>
#include <QStyleFactory>

// --- ОНОВЛЕНА ФУНКЦІЯ РОБОТИ З КОНФІГУРАЦІЄЮ ---
// Тепер вона перевіряє, створює (якщо потрібно) і завантажує конфігурацію.
QString setupApiConfiguration()
{
    QString configPath = QCoreApplication::applicationDirPath() + "/Config/config.json";
    QFile configFile(configPath);

    // 1. Якщо файл НЕ існує, створюємо його зі значеннями за замовчуванням.
    if (!configFile.exists()) {
        logWarning() << "Config file not found. Creating a default one at" << configPath;

        QDir().mkpath(QFileInfo(configFile).absolutePath()); // Створюємо теку Config, якщо її немає

        if (!configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            logCritical() << "Failed to create default config file!";
            return QString();
        }

        QJsonObject serverObj{{"host", "localhost"}, {"port", 8080}};
        QJsonObject rootObj{{"api_server", serverObj}};
        configFile.write(QJsonDocument(rootObj).toJson(QJsonDocument::Indented));
        configFile.close();
    }

    // 2. Тепер, коли ми впевнені, що файл існує, читаємо його.
    if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        logCritical() << "Failed to read config file at" << configPath;
        return QString();
    }

    QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll());
    if (doc.isObject() && doc.object().contains("api_server")) {
        QJsonObject serverObj = doc.object()["api_server"].toObject();
        QString host = serverObj["host"].toString("localhost");
        int port = serverObj["port"].toInt(8080);
        return QString("http://%1:%2").arg(host).arg(port);
    }

    logCritical() << "Config file is corrupted or has invalid format.";
    return QString();
}


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);


    // --- БЛОК СТИЛІЗАЦІЇ ---
    a.setStyle(QStyleFactory::create("Fusion")); // <--- 2. Вмикаємо стиль Fusion

    QPalette lightPalette;
    lightPalette.setColor(QPalette::Window, QColor(240, 240, 240));
    lightPalette.setColor(QPalette::WindowText, Qt::black);
    lightPalette.setColor(QPalette::Base, Qt::white);
    lightPalette.setColor(QPalette::AlternateBase, QColor(233, 233, 233));
    lightPalette.setColor(QPalette::ToolTipBase, Qt::white);
    lightPalette.setColor(QPalette::ToolTipText, Qt::black);
    lightPalette.setColor(QPalette::Text, Qt::black);
    lightPalette.setColor(QPalette::Button, QColor(240, 240, 240));
    lightPalette.setColor(QPalette::ButtonText, Qt::black);
    lightPalette.setColor(QPalette::BrightText, Qt::red);
    lightPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    lightPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    lightPalette.setColor(QPalette::HighlightedText, Qt::white);

    a.setPalette(lightPalette); // <--- 3. Застосовуємо палітру
    // -----------------------


    const QString appName = QFileInfo(QCoreApplication::applicationFilePath()).baseName();
    preInitLogger(appName);

    logInfo() << "Gandalf application starting...";


    QTranslator qtTranslator;
    if (qtTranslator.load(":/res/Translations/qtbase_uk.qm")) {
        a.installTranslator(&qtTranslator);
    } else {
        logWarning() << "Could not load Qt base translations for Ukrainian.";
    }

    // Налаштовуємо URL API-сервера
    QString apiUrl = setupApiConfiguration();
    if (apiUrl.isEmpty()) {
        QMessageBox::critical(nullptr, "Помилка конфігурації", "...");
        return 1;
    }
    AppParams::instance().setParam("Global", "ApiBaseUrl", apiUrl);
    logInfo() << "API Server URL set to:" << apiUrl;

    // 2. Створюємо і запускаємо контролер
    ApplicationController controller;
    controller.start();

    return a.exec();
}
