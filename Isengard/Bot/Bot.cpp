#include "Bot.h"
#include "TelegramClient.h"
#include "Oracle/ApiClient.h"
#include "Oracle/Logger.h"
#include <QJsonArray>
#include <QJsonObject>

Bot::Bot(const QString& botToken, QObject *parent)
    : QObject(parent),
    m_apiClient(ApiClient::instance()) // Отримуємо екземпляр ApiClient
{
    m_telegramClient = new TelegramClient(botToken, this);

    setupCommandHandlers();
    setupConnections();
}

void Bot::setupCommandHandlers()
{
    m_commandHandlers["/start"] = &Bot::handleStartCommand;
    m_commandHandlers["/help"] = &Bot::handleHelpCommand;
    logInfo() << "Command handlers registered.";
}

// Новий метод для налаштування з'єднань "сигнал-слот"
void Bot::setupConnections()
{
    connect(m_telegramClient, &TelegramClient::updatesReceived, this, &Bot::onUpdatesReceived);
    connect(&m_apiClient, &ApiClient::botUserRegistered, this, &Bot::onUserRegistered);
    connect(&m_apiClient, &ApiClient::botUserRegistrationFailed, this, &Bot::onUserRegistrationFailed);
    logInfo() << "Signal-slot connections established.";
}

void Bot::start()
{
    logInfo() << "Bot logic started. Starting to poll for updates...";
    m_telegramClient->startPolling();
}

void Bot::onUpdatesReceived(const QJsonArray& updates)
{
    for (const QJsonValue& updateValue : updates) {
        QJsonObject updateObj = updateValue.toObject();
        if (updateObj.contains("message")) {
            QJsonObject message = updateObj["message"].toObject();
            // Витягуємо текст і розглядаємо перше слово як команду
            QString text = message["text"].toString().trimmed();
            QString command = text.split(' ').first();

            // Шукаємо обробник в нашій карті
            CommandHandler handler = m_commandHandlers.value(command, &Bot::handleUnknownCommand);

            // Викликаємо знайдений метод (або метод для невідомої команди)
            (this->*handler)(message);
        }
    }
}

// Реалізація методів-обробників
    void Bot::handleStartCommand(const QJsonObject& message)
{
    QJsonObject from = message["from"].toObject();
    qint64 userId = from["id"].toVariant().toLongLong();
    QString username = from["username"].toString();
    QString firstName = from["first_name"].toString();

    logInfo() << "User" << username << "(" << userId << ") called /start. Sending registration request...";

    QJsonObject payload;
    payload["telegram_id"] = userId;
    payload["username"] = username;
    payload["first_name"] = firstName;

    m_apiClient.registerBotUser(payload);
}

void Bot::handleHelpCommand(const QJsonObject& message)
{
    logInfo() << "User called /help.";
    // Тут буде логіка для команди /help. Наприклад, відправка повідомлення з інструкціями.
    // m_telegramClient->sendMessage(userId, "Список доступних команд: ...");
}

void Bot::handleUnknownCommand(const QJsonObject& message)
{
    QString text = message["text"].toString();
    logInfo() << "Received unknown command:" << text;
    // Можна відповісти користувачу, що команда не розпізнана
}

void Bot::onUserRegistered(const QJsonObject& response)
{
    logInfo() << "Server response: User successfully registered." << response;
    // В майбутньому тут можна буде надіслати користувачу повідомлення про успіх
}

void Bot::onUserRegistrationFailed(const ApiError& error)
{
    logCritical() << "Server response: User registration failed:" << error.errorString;
}
