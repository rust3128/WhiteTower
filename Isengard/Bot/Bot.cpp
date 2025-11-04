#include "Bot.h"
#include "TelegramClient.h"
#include "Oracle/ApiClient.h"
#include "Oracle/Logger.h"
#include <QJsonArray>
#include <QJsonObject>

// --- КОНСТРУКТОР ---
Bot::Bot(const QString& botToken, QObject *parent)
    : QObject(parent),
    m_apiClient(ApiClient::instance())
{
    m_telegramClient = new TelegramClient(botToken, this);

    setupCommandHandlers(); // <-- ТЕПЕР МИ ВИКЛИКАЄМО НАЛАШТУВАННЯ МАП
    setupConnections();
}

void Bot::start()
{
    logInfo() << "Bot logic started. Starting to poll for updates...";
    m_telegramClient->startPolling();
}

// --- НАЛАШТУВАННЯ ---

void Bot::setupConnections()
{
    connect(m_telegramClient, &TelegramClient::updatesReceived, this, &Bot::onUpdatesReceived);

    // З'єднання для реєстрації (з qint64 telegramId)
    connect(&m_apiClient, &ApiClient::botUserRegistered, this, &Bot::onUserRegistered);
    connect(&m_apiClient, &ApiClient::botUserRegistrationFailed, this, &Bot::onUserRegistrationFailed);

    // З'єднання для перевірки статусу (з const QJsonObject& message)
    connect(&m_apiClient, &ApiClient::botUserStatusReceived, this, &Bot::onUserStatusReceived);
    connect(&m_apiClient, &ApiClient::botUserStatusCheckFailed, this, &Bot::onUserStatusCheckFailed);

    logInfo() << "Signal-slot connections established.";
}

/**
 * @brief Створює мапи (карти) обробників команд для різних ролей.
 */
void Bot::setupCommandHandlers()
{
    // --- Налаштування для ЗВИЧАЙНОГО КОРИСТУВАЧА ---
    m_userCommandHandlers["/start"] = &Bot::sendUserMenu;
    m_userCommandHandlers["❓ Допомога"] = &Bot::handleUserHelp;
    m_userCommandHandlers["/help"] = &Bot::handleUserHelp;
    m_userCommandHandlers["📋 Мої задачі"] = &Bot::handleMyTasks;
    m_userCommandHandlers["📊 Створити звіт"] = &Bot::handleMyTasks;
    m_userCommandHandlers["👥 Клієнти"] = &Bot::handleClientsCommand;


    // --- Налаштування для АДМІНІСТРАТОРА ---
    // Адмін має всі команди користувача...
    m_adminCommandHandlers = m_userCommandHandlers;

    // ... плюс свої власні (які перевизначають або доповнюють):
    m_adminCommandHandlers["/start"] = &Bot::sendAdminMenu; // Перевизначаємо /start
    m_adminCommandHandlers["❓ Допомога"] = &Bot::handleAdminHelp; // Перевизначаємо довідку
    m_adminCommandHandlers["/help"] = &Bot::handleAdminHelp;
    m_adminCommandHandlers["👑 Адмін: Запити"] = &Bot::handleAdminRequests;
    m_adminCommandHandlers["👑 Адмін: Користувачі"] = &Bot::handleAdminRequests; // Поки заглушка

    logInfo() << "Command handlers registered for user and admin roles.";
}

// --- ГОЛОВНІ СЛОТИ (МАРШРУТИЗАТОРИ) ---

/**
 * @brief Отримує оновлення, НІЧОГО не аналізує, відправляє на перевірку статусу.
 */
void Bot::onUpdatesReceived(const QJsonArray& updates)
{
    for (const QJsonValue& updateVal : updates) {
        QJsonObject update = updateVal.toObject();
        if (update.contains("message")) {
            QJsonObject message = update["message"].toObject();
            // Просто відправляємо будь-яке повідомлення на перевірку
            m_apiClient.checkBotUserStatus(message);
        }
    }
}

/**
 * @brief "МОЗОК". Отримує статус та повідомлення.
 * НЕ обробляє команди, а лише МАРШРУТИЗУЄ за СТАНОМ.
 */
void Bot::onUserStatusReceived(const QJsonObject& status, const QJsonObject& message)
{
    QJsonObject fromData = message["from"].toObject();
    qint64 userId = fromData["id"].toVariant().toLongLong();
    QString username = fromData["username"].toString();
    QString text = message["text"].toString();

    QString statusString = status["status"].toString();
    logInfo() << "Status for user" << username << "is:" << statusString << ". Text: " << text;

    // --- Етап 1: Маршрутизація за станом ---

    if (statusString == "NEW") {
        if (text == "/start") {
            logInfo() << "User is NEW. Proceeding to registration...";
            QJsonObject payload;
            payload["telegram_id"] = userId;
            payload["username"] = username;
            payload["first_name"] = fromData["first_name"].toString();
            m_apiClient.registerBotUser(payload);
        } else {
            m_telegramClient->sendMessage(userId, "Будь ласка, введіть /start для початку роботи.");
        }

    } else if (statusString == "PENDING") {
        logInfo() << "User is PENDING. Sending 'Wait' message.";
        m_telegramClient->sendMessage(userId, "Ваш запит на доступ знаходиться на розгляді адміністратора. Будь ласка, зачекайте.");

    } else if (statusString == "BLOCKED") {
        logInfo() << "User is BLOCKED. Sending 'Blocked' message.";
        m_telegramClient->sendMessage(userId, "Ваш обліковий запис заблоковано. Для отримання деталей, будь ласка, зверніться до вашого адміністратора.");

    } else if (statusString == "ACTIVE_USER") {
        // Делегуємо обробку мапі користувача
        processActiveUserCommand(message);

    } else if (statusString == "ACTIVE_ADMIN") {
        // Делегуємо обробку мапі адміна
        processActiveAdminCommand(message);
    }
}

// --- СЛОТИ РЕЄСТРАЦІЇ ---

void Bot::onUserRegistered(const QJsonObject& response, qint64 telegramId)
{
    logInfo() << "Server response: User successfully registered." << response;
    m_telegramClient->sendMessage(telegramId,
                                  "Ваш запит на доступ успішно надіслано. Очікуйте на схвалення адміністратором.");
}

void Bot::onUserRegistrationFailed(const ApiError& error, qint64 telegramId)
{
    logCritical() << "Server response: User registration failed:" << error.errorString;
    m_telegramClient->sendMessage(telegramId,
                                  "Під час реєстрації сталася помилка. Спробуйте пізніше. \nПомилка: " + error.errorString);
}

// Слот, якщо сам запит статусу провалився
void Bot::onUserStatusCheckFailed(const ApiError& error)
{
    logCritical() << "Failed to check user status:" << error.errorString;
    // Тут ми не знаємо, кому відповідати, тому просто логуємо
}

// --- МЕТОДИ-МАРШРУТИЗАТОРИ КОМАНД ---

void Bot::processActiveUserCommand(const QJsonObject& message)
{
    QString text = message["text"].toString();
    if (m_userCommandHandlers.contains(text)) {
        // Викликаємо метод (напр., &Bot::handleUserHelp) з мапи
        (this->*m_userCommandHandlers[text])(message);
    } else {
        handleUnknownCommand(message);
    }
}

void Bot::processActiveAdminCommand(const QJsonObject& message)
{
    QString text = message["text"].toString();
    if (m_adminCommandHandlers.contains(text)) {
        // Викликаємо метод (напр., &Bot::handleAdminHelp) з мапи
        (this->*m_adminCommandHandlers[text])(message);
    } else {
        handleUnknownCommand(message);
    }
}

// --- МЕТОДИ-ОБРОБНИКИ КОМАНД ---

void Bot::handleUserHelp(const QJsonObject& message)
{
    qint64 chatId = message["from"].toObject()["id"].toVariant().toLongLong();
    logInfo() << "User (ACTIVE_USER) called /help.";
    QString text = "<b>Допомога:</b>\n\n"
                   "<b>📋 Мої задачі:</b> <i>(в розробці)</i>\n"
                   "<b>📊 Створити звіт:</b> <i>(в розробці)</i>\n";
    m_telegramClient->sendMessage(chatId, text);
}

void Bot::handleAdminHelp(const QJsonObject& message)
{
    qint64 chatId = message["from"].toObject()["id"].toVariant().toLongLong();
    logInfo() << "User (ACTIVE_ADMIN) called /help.";
    QString text = "<b>Допомога (Адміністратор):</b>\n\n"
                   "<b>📋 Мої задачі:</b> <i>(в розробці)</i>\n"
                   "<b>📊 Створити звіт:</b> <i>(в розробці)</i>\n"
                   "<b>👑 Адмін: Запити:</b> <i>(в розробці)</i>\n"
                   "<b>👑 Адмін: Користувачі:</b> <i>(в розробці)</i>\n";
    m_telegramClient->sendMessage(chatId, text);
}

void Bot::handleMyTasks(const QJsonObject& message)
{
    qint64 chatId = message["from"].toObject()["id"].toVariant().toLongLong();
    logInfo() << "User called 'My Tasks'.";
    m_telegramClient->sendMessage(chatId, "Функція 'Мої задачі' наразі в розробці.");
}

void Bot::handleAdminRequests(const QJsonObject& message)
{
    qint64 chatId = message["from"].toObject()["id"].toVariant().toLongLong();
    logInfo() << "Admin called 'Admin Requests'.";
    m_telegramClient->sendMessage(chatId, "Зараз я завантажу список запитів...");

    // TODO: Викликати m_apiClient.fetchBotRequests()
    // (Ми зробимо це на наступному кроці)
}

void Bot::handleUnknownCommand(const QJsonObject& message)
{
    qint64 chatId = message["from"].toObject()["id"].toVariant().toLongLong();
    logInfo() << "Unknown command for active user:" << message["text"].toString();
    m_telegramClient->sendMessage(chatId, "Невідома команда. Оберіть дію з меню або введіть /help.");
}

// --- МЕТОДИ ВІДОБРАЖЕННЯ МЕНЮ (з новим підписом) ---

void Bot::sendUserMenu(const QJsonObject& message)
{
    qint64 chatId = message["from"].toObject()["id"].toVariant().toLongLong();

    QJsonObject keyboard;
    QJsonArray rows;
    QJsonArray row1;
    row1.append(QJsonObject{{"text", "📋 Мої задачі"}});
    row1.append(QJsonObject{{"text", "📊 Створити звіт"}});
    rows.append(row1);

    QJsonArray row2;
    row2.append(QJsonObject{{"text", "👥 Клієнти"}}); // <-- ДОДАНО
    rows.append(row2);

    QJsonArray row3; // <-- СТАВ РЯДКОМ 3
    row3.append(QJsonObject{{"text", "? Допомога"}});
    rows.append(row3);

    keyboard["keyboard"] = rows;
    keyboard["resize_keyboard"] = true;
    keyboard["one_time_keyboard"] = false;

    // Ми не знаємо FIO тут, тому шлемо загальне вітання
    m_telegramClient->sendMessage(chatId, "Оберіть дію:", keyboard);
}

void Bot::sendAdminMenu(const QJsonObject& message)
{
    qint64 chatId = message["from"].toObject()["id"].toVariant().toLongLong();

    QJsonObject keyboard;
    QJsonArray rows;
    QJsonArray row1;
    row1.append(QJsonObject{{"text", "📋 Мої задачі"}});
    row1.append(QJsonObject{{"text", "📊 Створити звіт"}});
    row1.append(QJsonObject{{"text", "👥 Клієнти"}});
    rows.append(row1);
    QJsonArray row2;
    row2.append(QJsonObject{{"text", "👑 Адмін: Запити"}});
    row2.append(QJsonObject{{"text", "👑 Адмін: Користувачі"}});
    rows.append(row2);
    QJsonArray row3;
    row3.append(QJsonObject{{"text", "❓ Допомога"}});
    rows.append(row3);

    keyboard["keyboard"] = rows;
    keyboard["resize_keyboard"] = true;
    keyboard["one_time_keyboard"] = false;

    m_telegramClient->sendMessage(chatId, "Меню адміністратора:", keyboard);
}

void Bot::handleClientsCommand(const QJsonObject& message)
{
    qint64 chatId = message["from"].toObject()["id"].toVariant().toLongLong();
    logInfo() << "User called 'Clients'.";
    m_telegramClient->sendMessage(chatId, "Функція 'Клієнти' наразі в розробці.");
}
