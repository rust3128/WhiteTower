#ifndef BOT_H
#define BOT_H

#include <QObject>
#include "Oracle/ApiClient.h" // Включаємо для ApiError

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
    void onUserRegistered(const QJsonObject& response);
    void onUserRegistrationFailed(const ApiError& error);


private:
    // Нові приватні методи для налаштувань
    void setupCommandHandlers();
    void setupConnections();

    // Метод для обробки конкретної команди
    using CommandHandler = void (Bot::*)(const QJsonObject& message);

    // Методи-обробники команд
    void handleStartCommand(const QJsonObject& message);
    void handleHelpCommand(const QJsonObject& message);
    void handleUnknownCommand(const QJsonObject& message);
private:
    // 3. Карта для зберігання команд
    QMap<QString, CommandHandler> m_commandHandlers;
    TelegramClient* m_telegramClient;
    ApiClient& m_apiClient;


};

#endif // BOT_H
