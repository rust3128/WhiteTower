#include "MainWindow.h"
#include "Oracle/Logger.h"
#include "Oracle/ApiClient.h"
#include "Oracle/User.h"
#include "Oracle/AppParams.h"
#include "Oracle/SessionManager.h"

#include <QApplication>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QTranslator>

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
    const QString appName = QFileInfo(QCoreApplication::applicationFilePath()).baseName();
    preInitLogger(appName);

    logInfo() << "Gandalf application starting...";

    // --- БЛОК ЗАВАНТАЖЕННЯ ПЕРЕКЛАДІВ ---
    QTranslator qtTranslator;
    if (qtTranslator.load(":/res/Translations/qtbase_uk.qm")) {
        a.installTranslator(&qtTranslator);
    } else {
        logWarning() << "Could not load Qt base translations for Ukrainian.";
    }
    // ------------------------------------

    // 1. Налаштовуємо та завантажуємо URL API-сервера
    QString apiUrl = setupApiConfiguration();
    if (apiUrl.isEmpty()) {
        QMessageBox::critical(nullptr, "Помилка конфігурації", "Не вдалося прочитати або створити файл конфігурації Config/config.json");
        return 1;
    }

    // 2. Зберігаємо URL в AppParams, щоб ApiClient міг його знайти
    AppParams::instance().setParam("Global", "ApiBaseUrl", apiUrl);
    logInfo() << "API Server URL set to:" << apiUrl;

    // 3. Запускаємо асинхронний логін (цей код вже використовує ApiClient, який тепер знає адресу)
    MainWindow w;
    QObject::connect(&ApiClient::instance(), &ApiClient::loginSuccess, [&](User* user) {
        logInfo() << "Login successful for user:" << user->fio();
        SessionManager::instance().setCurrentUser(user);
        w.show();
    });

    QObject::connect(&ApiClient::instance(), &ApiClient::loginFailed, [&](const ApiError& error) {
        logCritical() << "Login failed:" << error.errorString << error.httpStatusCode << error.requestUrl;

        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText("Помилка входу");
        msgBox.setInformativeText("Не вдалося ідентифікувати користувача.\n" + error.errorString);
        msgBox.setDetailedText(QString("URL: %1\nHTTP Status: %2")
                                   .arg(error.requestUrl)
                                   .arg(error.httpStatusCode));
        msgBox.exec();

        a.quit();
    });

    QString username = QProcessEnvironment::systemEnvironment().value("USERNAME");
    ApiClient::instance().login(username);

    return a.exec();
}
