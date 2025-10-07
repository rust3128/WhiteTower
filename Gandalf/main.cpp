#include "MainWindow.h"
#include "Oracle/Logger.h"
#include "Oracle/ApiClient.h"
#include "Oracle/User.h"

#include <QApplication>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcessEnvironment>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    const QString appName = QFileInfo(QCoreApplication::applicationFilePath()).baseName();
    preInitLogger(appName);

    logInfo() << "Gandalf application starting...";

    MainWindow w;
    QObject::connect(&ApiClient::instance(), &ApiClient::loginSuccess, [&](User* user) {
        logInfo() << "Login successful for user:" << user->login();
        // Можна передати вказівник на користувача у головне вікно, якщо потрібно
        // w.setCurrentUser(user);
        w.show();
    });

    QObject::connect(&ApiClient::instance(), &ApiClient::loginFailed, [&](const QString& error) {
        logCritical() << "Login failed:" << error;
        QMessageBox::critical(nullptr, "Помилка входу", "Не вдалося ідентифікувати користувача:\n" + error);
        a.quit();
    });

    // Запускаємо процес логіну
    QString username = QProcessEnvironment::systemEnvironment().value("USERNAME");
    ApiClient::instance().login(username);

    return a.exec();
}
