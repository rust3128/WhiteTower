#include "Bot.h"
#include "TelegramClient.h"
#include "Oracle/ApiClient.h"
#include "Oracle/Logger.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

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

    // З'єднання для отримання списку клієнтів
    connect(&m_apiClient, &ApiClient::botClientsFetched, this, &Bot::onBotClientsReceived);
    connect(&m_apiClient, &ApiClient::botClientsFetchFailed, this, &Bot::onBotClientsFailed);

    connect(&m_apiClient, &ApiClient::botAdminRequestsFetched, this, &Bot::onAdminRequestsReceived);
    connect(&m_apiClient, &ApiClient::botAdminRequestsFetchFailed, this, &Bot::onAdminRequestsFailed);

    connect(&m_apiClient, &ApiClient::botActiveUsersFetched, this, &Bot::onActiveUsersReceived);
    connect(&m_apiClient, &ApiClient::botActiveUsersFetchFailed, this, &Bot::onActiveUsersFailed);

    connect(&m_apiClient, &ApiClient::stationsFetched, this, &Bot::onStationsReceived);
    connect(&m_apiClient, &ApiClient::stationsFetchFailed, this, &Bot::onStationsFailed);
    connect(&m_apiClient, &ApiClient::stationDetailsFetched, this, &Bot::onStationDetailsReceived);
    connect(&m_apiClient, &ApiClient::stationDetailsFetchFailed, this, &Bot::onStationDetailsFailed);

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
    m_adminCommandHandlers["👑 Адмін: Користувачі"] = &Bot::handleAdminUsers; // Поки заглушка

    logInfo() << "Command handlers registered for user and admin roles.";
}

//
/**
 * @brief (ОНОВЛЕНО) Головний маршрутизатор вхідних оновлень.
 * Тепер перевіряє стан (введення номера АЗС) та inline-кнопки
 * ПЕРЕД обробкою звичайних команд.
 */
void Bot::onUpdatesReceived(const QJsonArray& updates)
{
    for (const QJsonValue& updateVal : updates) {
        QJsonObject update = updateVal.toObject();

        // --- 1. ПЕРЕВІРКА INLINE-КНОПКИ ---
        if (update.contains("callback_query")) {
            QJsonObject callbackQuery = update["callback_query"].toObject();
            handleCallbackQuery(callbackQuery);
            continue; // Обробка завершена
        }

        // --- 2. ПЕРЕВІРКА СТАНУ (ОЧІКУВАННЯ ВВЕДЕННЯ) ---
        if (update.contains("message")) {
            QJsonObject message = update["message"].toObject();
            qint64 telegramId = message["from"].toObject()["id"].toVariant().toLongLong();

            if (m_userState.value(telegramId) == UserState::WaitingForStationNumber) {
                handleStationNumberInput(message);
                continue; // Обробка завершена
            }

            // --- 3. ЗВИЧАЙНА ОБРОБКА КОМАНДИ ---
            // (Якщо не кнопка і не стан, перевіряємо статус)
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

//
/**
 * @brief (ОНОВЛЕНО) Обробляє команду "👑 Адмін: Запити".
 */
void Bot::handleAdminRequests(const QJsonObject& message)
{
    qint64 chatId = message["from"].toObject()["id"].toVariant().toLongLong();
    logInfo() << "Admin" << chatId << "called 'Admin: Requests'.";

    // 1. Повідомляємо адміну, що ми почали
    m_telegramClient->sendChatAction(chatId, "typing");

    // 2. Викликаємо новий метод ApiClient
    m_apiClient.fetchBotRequestsForAdmin(chatId);
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

/**
 * @brief (ОНОВЛЕНО) Обробляє команду "👥 Клієнти".
 * Запитує список клієнтів у ApiClient.
 */
void Bot::handleClientsCommand(const QJsonObject& message)
{
    qint64 chatId = message["from"].toObject()["id"].toVariant().toLongLong();
    logInfo() << "User" << chatId << "called 'Clients'.";

    // 1. Повідомляємо користувачу, що ми почали
    m_telegramClient->sendChatAction(chatId, "typing");

    // 2. Викликаємо ApiClient.
    // Ми передаємо chatId, щоб ApiClient додав його в заголовок X-Telegram-ID
    m_apiClient.fetchBotClients(chatId);
}

//
/**
 * @brief (ПОВНІСТЮ ПЕРЕПИСАНО) Отримано список клієнтів.
 * Тепер надсилає їх у вигляді inline-кнопок.
 */
void Bot::onBotClientsReceived(const QJsonArray& clients, qint64 telegramId)
{
    logInfo() << "Successfully fetched" << clients.count() << "clients for user" << telegramId;

    if (clients.isEmpty()) {
        m_telegramClient->sendMessage(telegramId, "Список клієнтів порожній.");
        return;
    }

    // --- Формуємо Inline-Клавіатуру ---
    QJsonObject keyboard;
    QJsonArray rows;

    for (const QJsonValue& val : clients) {
        QJsonObject client = val.toObject();
        QString name = client["client_name"].toString();
        int id = client["client_id"].toInt();

        // Створюємо кнопку
        QJsonObject button;
        button["text"] = name;
        // "Зашиваємо" ID клієнта в callback_data
        button["callback_data"] = QString("client:select:%1").arg(id);

        // Кожна кнопка в окремому ряду
        QJsonArray row;
        row.append(button);
        rows.append(row);
    }
    keyboard["inline_keyboard"] = rows;
    // --- Кінець формування ---

    m_telegramClient->sendMessageWithInlineKeyboard(telegramId,
                                                    "<b>Оберіть клієнта:</b>",
                                                    keyboard);
}

/**
 * @brief (НОВИЙ СЛОТ) Не вдалося отримати список клієнтів.
 */
void Bot::onBotClientsFailed(const ApiError& error, qint64 telegramId)
{
    logCritical() << "Failed to fetch clients for user" << telegramId << ":" << error.errorString;
    m_telegramClient->sendMessage(telegramId,
                                  "Помилка завантаження клієнтів: " + error.errorString);
}


//
/**
 * @brief (ВІДКОЧЕНО) Успішно отримано список запитів (тільки текст).
 */
void Bot::onAdminRequestsReceived(const QJsonArray& requests, qint64 telegramId)
{
    logInfo() << "Successfully fetched" << requests.count() << "pending requests for admin" << telegramId;

    if (requests.isEmpty()) {
        m_telegramClient->sendMessage(telegramId, "Нових запитів на реєстрацію немає.");
        return;
    }

    QStringList requestList;
    requestList.append(QString("<b>Нові запити на реєстрацію (%1):</b>\n")
                           .arg(requests.count()));

    for (const QJsonValue& val : requests) {
        QJsonObject req = val.toObject();

        // Читаємо "request_id" (виправлено)
        int requestId = req["request_id"].toInt();
        QString login = req["login"].toString(); // (telegram username)
        QString fio = req["fio"].toString();     // (telegram FIO)

        requestList.append(QString("👤 <b>%1</b> (%2) [ID: %3]")
                               .arg(fio, login, QString::number(requestId)));
    }

    // Додаємо примітку, що керування відбувається в Gandalf
    requestList.append("\n\n<i>Для схвалення або відхилення, будь ласка, "
                       "використовуйте десктопний додаток (Gandalf).</i>");

    m_telegramClient->sendMessage(telegramId, requestList.join("\n"));
}

/**
 * @brief (НОВИЙ СЛОТ) Не вдалося отримати список запитів.
 */
void Bot::onAdminRequestsFailed(const ApiError& error, qint64 telegramId)
{
    logCritical() << "Failed to fetch admin requests for" << telegramId << ":" << error.errorString;
    m_telegramClient->sendMessage(telegramId,
                                  "Помилка завантаження запитів: " + error.errorString);
}


//

// --- Адмін: Користувачі ---

/**
 * @brief (НОВИЙ) Обробляє команду "👑 Адмін: Користувачі".
 */
void Bot::handleAdminUsers(const QJsonObject& message)
{
    qint64 chatId = message["from"].toObject()["id"].toVariant().toLongLong();
    logInfo() << "Admin" << chatId << "called 'Admin: Users'.";

    // Використовуємо "typing" action
    m_telegramClient->sendChatAction(chatId, "typing");

    // Викликаємо новий метод ApiClient
    m_apiClient.fetchBotActiveUsers(chatId);
}

/**
 * @brief (НОВИЙ СЛОТ) Успішно отримано список активних користувачів.
 */
void Bot::onActiveUsersReceived(const QJsonArray& users, qint64 telegramId)
{
    logInfo() << "Successfully fetched" << users.count() << "active users for admin" << telegramId;

    if (users.isEmpty()) {
        m_telegramClient->sendMessage(telegramId, "Не знайдено жодного активного користувача бота.");
        return;
    }

    QStringList userList;
    userList.append(QString("<b>Активні користувачі бота (%1):</b>\n")
                        .arg(users.count()));

    for (const QJsonValue& val : users) {
        QJsonObject user = val.toObject();

        int userId = user["user_id"].toInt();
        QString login = user["login"].toString(); // (корпоративний логін)
        QString fio = user["fio"].toString();     // (ПІБ)

        userList.append(QString("👤 <b>%1</b> (%2) [ID: %3]")
                            .arg(fio, login, QString::number(userId)));
    }

    m_telegramClient->sendMessage(telegramId, userList.join("\n"));
}

/**
 * @brief (НОВИЙ СЛОТ) Не вдалося отримати список активних користувачів.
 */
void Bot::onActiveUsersFailed(const ApiError& error, qint64 telegramId)
{
    logCritical() << "Failed to fetch active users for" << telegramId << ":" << error.errorString;
    m_telegramClient->sendMessage(telegramId,
                                  "Помилка завантаження списку користувачів: " + error.errorString);
}


//

/**
 * @brief (НОВИЙ "МОЗОК") Обробляє всі натискання inline-кнопок.
 */
void Bot::handleCallbackQuery(const QJsonObject& callbackQuery)
{
    QString data = callbackQuery["data"].toString();
    QJsonObject message = callbackQuery["message"].toObject();
    qint64 chatId = message["chat"].toObject()["id"].toVariant().toLongLong();
    int messageId = message["message_id"].toInt();
    QString callbackQueryId = callbackQuery["id"].toString();

    logInfo() << "Callback query received from" << chatId << "with data:" << data;

    // 1. --- Навігація по меню Клієнта ---
    if (data.startsWith("client:select:")) {
        int clientId = data.split(":").last().toInt();

        // Створюємо нове меню (Список АЗС / Ввести номер)
        QJsonObject keyboard;
        QJsonArray rows;
        QJsonArray row1;
        row1.append(QJsonObject{
            {"text", "📋 Список АЗС"},
            {"callback_data", QString("stations:list:%1").arg(clientId)}
        });
        row1.append(QJsonObject{
            {"text", "⌨️ Ввести номер АЗС"},
            {"callback_data", QString("stations:enter:%1").arg(clientId)}
        });
        rows.append(row1);
        QJsonArray row2;
        row2.append(QJsonObject{
            {"text", "⬅️ Назад (до клієнтів)"},
            {"callback_data", "clients:main"}
        });
        rows.append(row2);
        keyboard["inline_keyboard"] = rows;

        // Редагуємо повідомлення "на місці"
        m_telegramClient->editMessageText(chatId, messageId, "<b>Оберіть дію:</b>", keyboard);
        m_telegramClient->answerCallbackQuery(callbackQueryId); // Знімаємо "годинник"

    }
    // 2. --- Повернення до списку клієнтів ---
    else if (data == "clients:main") {
        // Ми не можемо просто викликати onBotClientsReceived,
        // бо нам потрібен messageId для редагування.
        // Простіше попросити користувача викликати команду знову.
        m_telegramClient->sendMessage(chatId, "Будь ласка, натисніть /start або 👥 Клієнти, щоб оновити список.");
        m_telegramClient->answerCallbackQuery(callbackQueryId);
    }
    // 3. --- Запит на "Ввести номер АЗС" ---
    else if (data.startsWith("stations:enter:")) {
        int clientId = data.split(":").last().toInt();

        // Встановлюємо стан
        m_userState[chatId] = UserState::WaitingForStationNumber;
        m_userClientContext[chatId] = clientId; // Зберігаємо контекст

        logInfo() << "User" << chatId << "is now WaitingForStationNumber for client" << clientId;
        m_telegramClient->answerCallbackQuery(callbackQueryId, "Введіть номер терміналу");
        m_telegramClient->sendMessage(chatId, "<b>Введіть номер терміналу (АЗС):</b>");
    }
    // 4. --- Запит на "Список АЗС" ---
    else if (data.startsWith("stations:list:")) {
        int clientId = data.split(":").last().toInt();
        m_telegramClient->answerCallbackQuery(callbackQueryId, "Завантажую список...");
        m_apiClient.fetchStationsForClient(chatId, clientId);
    }
    // 5. --- (Майбутнє) Обробка кнопок меню АЗС ---
    else if (data.startsWith("station:")) {
        // (напр., "station:reboot:555:101")
        m_telegramClient->answerCallbackQuery(callbackQueryId, "Функція в розробці...");
    }
    else if (data.startsWith("stations:page:")) {
        // data = "stations:page:<clientId>:<page>"
        QStringList parts = data.split(":");
        int clientId = parts[2].toInt();
        int page = parts[3].toInt();

        // Редагуємо повідомлення, показуючи нову сторінку
        sendPaginatedStations(chatId, clientId, page, messageId);
        m_telegramClient->answerCallbackQuery(callbackQueryId); // Просто знімаємо "годинник"
    }

    // 6. --- (НОВЕ) Закриття списку АЗС ---
    else if (data == "stations:close") {
        // Просто видаляємо повідомлення зі списком
        // (Або можемо його відредагувати на "Список закрито")
        // m_telegramClient->deleteMessage(chatId, messageId); // Потребує нового методу в TelegramClient
        m_telegramClient->editMessageText(chatId, messageId, "<i>Список АЗС закрито.</i>", QJsonObject());
        m_userStationCache.remove(chatId); // Чистимо кеш
        m_telegramClient->answerCallbackQuery(callbackQueryId);
    }
    // Інші кнопки
    else {
        m_telegramClient->answerCallbackQuery(callbackQueryId);
    }
}

/**
 * @brief (НОВИЙ) Обробляє текстове повідомлення, коли
 * користувач перебуває у стані WaitingForStationNumber.
 */
void Bot::handleStationNumberInput(const QJsonObject& message)
{
    qint64 chatId = message["from"].toObject()["id"].toVariant().toLongLong();
    QString terminalNo = message["text"].toString().trimmed();

    // 1. Отримуємо контекст (для якого клієнта шукаємо)
    int clientId = m_userClientContext.value(chatId, 0);

    // 2. Скидаємо стан (ВАЖЛИВО!)
    m_userState.remove(chatId);
    m_userClientContext.remove(chatId);

    if (clientId == 0) {
        logWarning() << "User" << chatId << "sent number, but context was lost.";
        m_telegramClient->sendMessage(chatId, "Виникла помилка. Оберіть клієнта знову.");
        return;
    }

    logInfo() << "User" << chatId << "sent terminal number" << terminalNo << "for client" << clientId;
    m_telegramClient->sendChatAction(chatId);
    m_apiClient.fetchStationDetails(chatId, clientId, terminalNo);
}


// --- НОВІ СЛОТИ (АЗС) ---
//
/**
 * @brief (ОНОВЛЕНО) Успішно отримано список АЗС.
 * Тепер зберігає список в кеш і викликає пагінацію.
 */
void Bot::onStationsReceived(const QJsonArray& stations, qint64 telegramId, int clientId)
{
    logInfo() << "Fetched" << stations.count() << "stations for user" << telegramId;
    if (stations.isEmpty()) {
        m_telegramClient->sendMessage(telegramId, "Для цього клієнта не знайдено АЗС, до яких ви маєте доступ.");
        return;
    }

    // 1. Зберігаємо повний список в кеш
    m_userStationCache[telegramId] = stations;

    // 2. Надсилаємо ПЕРШУ сторінку (page = 1)
    sendPaginatedStations(telegramId, clientId, 1, 0);
}

void Bot::onStationsFailed(const ApiError& error, qint64 telegramId, int clientId)
{
    logCritical() << "Failed to fetch stations:" << error.errorString;
    m_telegramClient->sendMessage(telegramId, "❌ Помилка завантаження списку АЗС.");
}

void Bot::onStationDetailsReceived(const QJsonObject& station, qint64 telegramId, int clientId)
{
    logInfo() << "Fetched details for station:" << station["terminal_no"].toString();

    // Формуємо повідомлення (як на скріншоті)
    QString text = QString("<b>АЗС: %1</b> (ID: %2)\n"
                           "Клієнт ID: %3\n"
                           "Статус: %4, %5")
                       .arg(station["name"].toString())
                       .arg(station["terminal_no"].toString())
                       .arg(QString::number(clientId))
                       .arg(station["is_active"].toBool() ? "Активна" : "Неактивна")
                       .arg(station["is_working"].toBool() ? "В роботі" : "Не в роботі");

    // Формуємо кнопки (як на скріншоті)
    QJsonObject keyboard;
    QJsonArray rows;
    QJsonArray row1;
    // Ми "зашиваємо" всю інфу в кнопку: "station:<action>:<clientId>:<termNo>"
    QString baseData = QString("station:%1:%2").arg(clientId).arg(station["terminal_no"].toString());

    row1.append(QJsonObject{{"text", "ℹ️ Інфо"}, {"callback_data", baseData.arg("info")}});
    row1.append(QJsonObject{{"text", "🔄 Перезавантажити"}, {"callback_data", baseData.arg("reboot")}});
    rows.append(row1);

    // ... (додайте інші кнопки за потреби) ...

    keyboard["inline_keyboard"] = rows;
    m_telegramClient->sendMessageWithInlineKeyboard(telegramId, text, keyboard);
}

void Bot::onStationDetailsFailed(const ApiError& error, qint64 telegramId, int clientId)
{
    logCritical() << "Failed to fetch station details:" << error.errorString;
    // Перевіряємо, чи це помилка "не знайдено"
    QJsonDocument doc = QJsonDocument::fromJson(error.responseBody);
    if (doc.isObject() && doc.object().contains("error")) {
        m_telegramClient->sendMessage(telegramId, "❌ " + doc.object()["error"].toString());
    } else {
        m_telegramClient->sendMessage(telegramId, "❌ Помилка пошуку АЗС.");
    }
}

//

/**
 * @brief (НОВИЙ) "Рендерить" і надсилає конкретну сторінку списку АЗС.
 * @param telegramId ID чату.
 * @param clientId ID клієнта (для формування кнопок).
 * @param page Номер сторінки (починаючи з 1).
 * @param messageId (Опціонально) ID повідомлення для редагування.
 */
void Bot::sendPaginatedStations(qint64 telegramId, int clientId, int page, int messageId = 0)
{
    // 1. Отримуємо повний список з кешу
    if (!m_userStationCache.contains(telegramId)) {
        logWarning() << "Station cache for user" << telegramId << "is empty.";
        m_telegramClient->sendMessage(telegramId, "❌ Помилка кешу. Спробуйте обрати клієнта знову.");
        return;
    }
    QJsonArray allStations = m_userStationCache.value(telegramId);

    // 2. Налаштування пагінації
    const int itemsPerPage = 20; // Скільки АЗС на одній сторінці
    const int totalItems = allStations.count();
    const int totalPages = (totalItems + itemsPerPage - 1) / itemsPerPage;

    if (page < 1) page = 1;
    if (page > totalPages) page = totalPages;

    // 3. "Нарізаємо" масив для поточної сторінки
    QJsonArray pageStations;
    int startIndex = (page - 1) * itemsPerPage;
    int endIndex = qMin(startIndex + itemsPerPage, totalItems);

    for (int i = startIndex; i < endIndex; ++i) {
        pageStations.append(allStations.at(i));
    }

    // 4. Формуємо "псевдо-таблицю" (ваш код)
    QString messageTitle = QString("<b>Доступні АЗС (Сторінка %1 / %2):</b>")
                               .arg(page).arg(totalPages);
    QStringList tableRows;
    const int termWidth = 6;
    const int nameWidth = 32;
    const int statusWidth = 3;

    tableRows.append(QString("%1 | %2 | %3 | %4")
                         .arg("ID", -termWidth)
                         .arg("Назва АЗС", -nameWidth)
                         .arg("Акт.", -statusWidth)
                         .arg("Роб.", -statusWidth));
    tableRows.append(QString(53, '-')); // Ваша виправлена довжина

    for (const QJsonValue& val : pageStations) {
        QJsonObject s = val.toObject();
        QString termNo = s["terminal_no"].toString();
        QString name = s["name"].toString();
        if (name.length() > nameWidth) {
            name = name.left(nameWidth - 1) + ".";
        }
        QString active = s["is_active"].toBool() ? " ✅" : " ❌";
        QString working = s["is_working"].toBool() ? " ✅" : " ❌";

        tableRows.append(QString("%1 | %2 | %3 | %4")
                             .arg(termNo, -termWidth)
                             .arg(name, -nameWidth)
                             .arg(active, -statusWidth)
                             .arg(working, -statusWidth));
    }
    QString messageBody = messageTitle + "\n<pre>" + tableRows.join("\n") + "</pre>";

    // 5. Формуємо кнопки пагінації
    QJsonObject keyboard;
    QJsonArray rows;
    QJsonArray navRow; // Ряд кнопок

    // Кнопка "<" (Назад)
    if (page > 1) {
        navRow.append(QJsonObject{
            {"text", "⬅️ Назад"},
            {"callback_data", QString("stations:page:%1:%2").arg(clientId).arg(page - 1)}
        });
    }

    // Кнопка "Сторінка X/Y" (просто текст)
    navRow.append(QJsonObject{
        {"text", QString("Сторінка %1/%2").arg(page).arg(totalPages)},
        {"callback_data", "noop"} // "No Operation"
    });

    // Кнопка ">" (Вперед)
    if (page < totalPages) {
        navRow.append(QJsonObject{
            {"text", "Вперед ➡️"},
            {"callback_data", QString("stations:page:%1:%2").arg(clientId).arg(page + 1)}
        });
    }
    rows.append(navRow);

    // Кнопка "Закрити"
    QJsonArray closeRow;
    closeRow.append(QJsonObject{
        {"text", "❌ Закрити список"},
        {"callback_data", QString("stations:close")}
    });
    rows.append(closeRow);

    keyboard["inline_keyboard"] = rows;

    // 6. Надсилаємо або Редагуємо повідомлення
    if (messageId == 0) {
        // Якщо messageId 0 - це перший раз, надсилаємо нове
        m_telegramClient->sendMessageWithInlineKeyboard(telegramId, messageBody, keyboard);
    } else {
        // Якщо messageId є - редагуємо існуюче
        m_telegramClient->editMessageText(telegramId, messageId, messageBody, keyboard);
    }
}
