// Gandalf/ApplicationController.cpp
#include "ApplicationController.h"
#include "MainWindow.h" // А тут вже підключаємо повний .h
#include "Oracle/Logger.h"
#include "Oracle/SessionManager.h"
#include "Oracle/AppParams.h"
#include "Oracle/User.h"
#include <QApplication>
#include <QMessageBox>
#include <QProcessEnvironment>

ApplicationController::ApplicationController(QObject *parent)
    : QObject{parent}
{
    m_mainWindow = new MainWindow();
    createConnections();
}

void ApplicationController::createConnections()
{
    connect(&ApiClient::instance(), &ApiClient::loginSuccess, this, &ApplicationController::onLoginSuccess);
    connect(&ApiClient::instance(), &ApiClient::loginFailed, this, &ApplicationController::onLoginFailed);
    connect(&ApiClient::instance(), &ApiClient::settingsFetched, this, &ApplicationController::onSettingsFetched);
    connect(&ApiClient::instance(), &ApiClient::settingsFetchFailed, this, &ApplicationController::onSettingsFetchFailed);
}

void ApplicationController::start()
{
    // Запускаємо логін
    QString username = QProcessEnvironment::systemEnvironment().value("USERNAME");
    ApiClient::instance().login(username);
}

void ApplicationController::onLoginSuccess(User *user)
{
    logInfo() << "Login successful for user:" << user->fio();
    SessionManager::instance().setCurrentUser(user);

    // Запускаємо завантаження налаштувань
    ApiClient::instance().fetchSettings("Gandalf");

    m_mainWindow->show();
}

void ApplicationController::onLoginFailed(const ApiError &error)
{
    logCritical() << "Login failed:" << error.errorString << error.httpStatusCode << error.requestUrl;

    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setText("Помилка входу");
    msgBox.setInformativeText("Не вдалося ідентифікувати користувача.\n" + error.errorString);
    msgBox.setDetailedText(QString("URL: %1\nHTTP Status: %2")
                               .arg(error.requestUrl)
                               .arg(error.httpStatusCode));
    msgBox.exec();

    qApp->quit(); // Закриваємо додаток
}

void ApplicationController::onSettingsFetched(const QVariantMap &settings)
{
    logInfo() << "Successfully loaded" << settings.count() << "settings for Gandalf.";
    for (auto it = settings.constBegin(); it != settings.constEnd(); ++it) {
        AppParams::instance().setParam("Gandalf", it.key(), it.value());
    }
}

void ApplicationController::onSettingsFetchFailed(const ApiError &error)
{
    logCritical() << "Failed to fetch application settings:" << error.errorString;
    // Тут можна теж показати QMessageBox, якщо налаштування критичні
}
