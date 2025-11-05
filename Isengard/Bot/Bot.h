#ifndef BOT_H
#define BOT_H

#include <QObject>
#include "Oracle/ApiClient.h" // Включаємо для ApiError
#include <QMap>               // <-- ДОДАНО (для QMap)

class TelegramClient;
class ApiClient;
class QJsonObject;

class Bot : public QObject
{
    Q_OBJECT
public:
    explicit Bot(const QString& botToken, QObject *parent = nullptr);

    void start();

private slots:
    // Слот для обробки оновлень від Telegram
    void onUpdatesReceived(const QJsonArray& updates);

    // Слоти для обробки відповідей від нашого сервера
    void onUserRegistered(const QJsonObject& response, qint64 telegramId);
    void onUserRegistrationFailed(const ApiError& error, qint64 telegramId);

    void onUserStatusReceived(const QJsonObject& status, const QJsonObject& message);
    void onUserStatusCheckFailed(const ApiError& error);

    // --- СЛОТИ ДЛЯ КЛІЄНТІВ ---
    void onBotClientsReceived(const QJsonArray& clients, qint64 telegramId);
    void onBotClientsFailed(const ApiError& error, qint64 telegramId);

    void onAdminRequestsReceived(const QJsonArray& requests, qint64 telegramId);
    void onAdminRequestsFailed(const ApiError& error, qint64 telegramId);

    // --- ДОДАЙТЕ ЦІ ДВА РЯДКИ ---
    void onActiveUsersReceived(const QJsonArray& users, qint64 telegramId);
    void onActiveUsersFailed(const ApiError& error, qint64 telegramId);

    void onStationsReceived(const QJsonArray& stations, qint64 telegramId, int clientId);
    void onStationsFailed(const ApiError& error, qint64 telegramId, int clientId);
    void onStationDetailsReceived(const QJsonObject& station, qint64 telegramId, int clientId);
    void onStationDetailsFailed(const ApiError& error, qint64 telegramId, int clientId);
private:
    // Тип-вказівник на метод-обробник
    using CommandHandler = void (Bot::*)(const QJsonObject& message);

    // --- Методи налаштування ---
    void setupCommandHandlers(); // <-- ПЕРЕМІЩЕНО
    void setupConnections();

    // --- Методи-маршрутизатори ---
    void processActiveUserCommand(const QJsonObject& message);
    void processActiveAdminCommand(const QJsonObject& message);

    // --- Обробники команд ---
    void handleUserHelp(const QJsonObject& message);
    void handleAdminHelp(const QJsonObject& message);
    void handleMyTasks(const QJsonObject& message);
    void handleAdminRequests(const QJsonObject& message);
    void handleClientsCommand(const QJsonObject& message);

    void handleUnknownCommand(const QJsonObject& message);

    // --- Методи для меню (ЗМІНЕНО ПІДПИС) ---
    void sendUserMenu(const QJsonObject& message);    // <-- ЗМІНЕНО
    void sendAdminMenu(const QJsonObject& message);   // <-- ЗМІНЕНО
    void handleAdminUsers(const QJsonObject& message);

    void handleCallbackQuery(const QJsonObject& callbackQuery);
    void handleStationNumberInput(const QJsonObject& message);
    void sendPaginatedStations(qint64 telegramId, int clientId, int page, int messageId);

private:
    enum class UserState { None, WaitingForStationNumber };
    QMap<qint64, UserState> m_userState; // <telegramId, State>
    QMap<qint64, int> m_userClientContext; // <telegramId, clientId>
    QMap<qint64, QJsonArray> m_userStationCache; // <telegramId, Full_Stations_Array>
    QMap<QString, CommandHandler> m_userCommandHandlers;
    QMap<QString, CommandHandler> m_adminCommandHandlers;
    TelegramClient* m_telegramClient;
    ApiClient& m_apiClient;
};

#endif // BOT_H
