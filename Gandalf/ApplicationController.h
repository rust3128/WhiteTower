// Gandalf/ApplicationController.h
#ifndef APPLICATIONCONTROLLER_H
#define APPLICATIONCONTROLLER_H

#include <QObject>
#include "Oracle/ApiClient.h" // Потрібен для ApiError

class MainWindow; // Використовуємо попереднє оголошення, щоб не підключати .h

class ApplicationController : public QObject
{
    Q_OBJECT
public:
    explicit ApplicationController(QObject *parent = nullptr);
    void start(); // Метод, який запускає всю логіку

private slots:
    // Слоти для обробки сигналів від ApiClient
    void onLoginSuccess(User* user);
    void onLoginFailed(const ApiError& error);
    void onSettingsFetched(const QVariantMap& settings);
    void onSettingsFetchFailed(const ApiError& error);

private:
    void createConnections();

private:
    MainWindow* m_mainWindow;
};

#endif // APPLICATIONCONTROLLER_H
