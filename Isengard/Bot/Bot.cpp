#include "Bot.h"
#include "TelegramClient.h"
#include "Oracle/ApiClient.h"
#include "Oracle/Logger.h"
#include "Oracle/AppParams.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>


// Допоміжна функція для екранування HTML-символів у тексті
QString escapeHtml(const QString& text)
{
    QString escaped = text;
    // & має бути першим!
    escaped.replace(QLatin1Char('&'), QLatin1String("&amp;"));
    escaped.replace(QLatin1Char('<'), QLatin1String("&lt;"));
    escaped.replace(QLatin1Char('>'), QLatin1String("&gt;"));
    return escaped;
}

// --- КОНСТРУКТОР ---
Bot::Bot(const QString& botToken, QObject *parent)
    : QObject(parent),
    m_apiClient(ApiClient::instance())
{
    m_telegramClient = new TelegramClient(botToken, this);

    m_attachmentManager = new AttachmentManager(this);
    setupCommandHandlers();
    setupCallbackHandlers();
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

    // --- РРО (POS) ---
    connect(&m_apiClient, &ApiClient::stationPosDataReceived, this, &Bot::onStationPosDataReceived);
    connect(&m_apiClient, &ApiClient::stationPosDataFailed, this, &Bot::onStationPosDataFailed);

    // --- TANKS ------
    connect(&m_apiClient, &ApiClient::stationTanksReceived, this, &Bot::onStationTanksReceived);
    connect(&m_apiClient, &ApiClient::stationTanksFailed, this, &Bot::onStationTanksFailed);

    // З'єднання для конфігурації ТРК
    connect(&m_apiClient, &ApiClient::dispenserConfigReceived,
            this, &Bot::onDispenserConfigReceived);
    connect(&m_apiClient, &ApiClient::dispenserConfigFailed,
            this, &Bot::onDispenserConfigFailed);

    connect(&ApiClient::instance(), &ApiClient::redmineTasksFetched,
            this, &Bot::onRedmineTasksFetched);
    connect(&ApiClient::instance(), &ApiClient::redmineTasksFetchFailed,
            this, &Bot::onRedmineTasksFetchFailed);


    connect(&ApiClient::instance(), &ApiClient::jiraTasksFetched,
            this, &Bot::onJiraTasksFetched);
    connect(&ApiClient::instance(), &ApiClient::jiraTasksFetchFailed,
            this, &Bot::onJiraTasksFetchFailed);


    connect(&m_apiClient, &ApiClient::taskDetailsFetched,
            this, &Bot::onTaskDetailsFetched);
    connect(&m_apiClient, &ApiClient::taskDetailsFetchFailed,
            this, &Bot::onTaskDetailsFetchFailed);

    connect(&m_apiClient, &ApiClient::assignTaskSuccess,
            this, &Bot::onAssignTaskSuccess);
    connect(&m_apiClient, &ApiClient::assignTaskFailed,
            this, &Bot::onAssignTaskFailed);

    connect(&m_apiClient, &ApiClient::reportTaskSuccess, this, &Bot::onReportTaskSuccess);
    connect(&m_apiClient, &ApiClient::reportTaskFailed, this, &Bot::onReportTaskFailed);

    connect(m_attachmentManager, &AttachmentManager::fileDownloaded, this, [this](const QString &path) {
        logInfo() << "File successfully saved to archive:" << path;
        // ТУТ МИ БУДЕМО ВИКЛИКАТИ ApiClient::uploadFileToConduit(path)
    });

    connect(m_attachmentManager, &AttachmentManager::downloadError, this, [this](const QString &err) {
        logCritical() << "Attachment download error:" << err;
    });

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
    m_userCommandHandlers["📊 Створити звіт"] = &Bot::handleCreateReport;
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
 * @brief (НОВИЙ) Налаштовує мапи (карти) обробників для inline-кнопок.
 */
void Bot::setupCallbackHandlers()
{
    // Префікс "clients:" (дії зі списком клієнтів)
    m_clientsHandlers["main"] = &Bot::handleCallbackClientsMain;

    // Префікс "client:" (дії для конкретного клієнта)
    m_clientHandlers["select"] = &Bot::handleCallbackClientSelect;

    // Префікс "stations:" (дії зі списком АЗС)
    m_stationsHandlers["list"]  = &Bot::handleCallbackStationsList;
    m_stationsHandlers["enter"] = &Bot::handleCallbackStationsEnter;
    m_stationsHandlers["page"]  = &Bot::handleCallbackStationsPage;
    m_stationsHandlers["close"] = &Bot::handleCallbackStationsClose;

    // Префікс "station:" (дії на картці АЗС)
    m_stationHandlers["stub"] = &Bot::handleCallbackStationStub;
    m_stationHandlers["map"]  = &Bot::handleCallbackStationMap;
    m_stationHandlers["pos"] = &Bot::handleCallbackStationPos;
    m_stationHandlers["tanks"] = &Bot::handleCallbackStationTanks;
    m_stationHandlers["disp"] = &Bot::handleCallbackStationDisp;

    // Оброблятиме: tasks:show
    m_tasksHandlers["show"] = &Bot::handleTaskTrackerSelection;
    m_reportHandlers["select"] = &Bot::handleReportTrackerSelection;

    // !!! НОВІ РЕЄСТРАЦІЇ ДЛЯ ФАЗИ ВИБОРУ ЗАДАЧІ !!!
    m_reportHandlers["select_task"] = &Bot::handleCallbackReportSelectTask;
    m_reportHandlers["manual_id"] = &Bot::handleCallbackReportManualId;

    m_reportHandlers["type"] = &Bot::handleCallbackReportSelectType;

    m_reportHandlers["search"] = &Bot::handleCallbackReportSearch;

    m_reportHandlers["action"] = &Bot::handleCallbackReportAction;


    logInfo() << "Callback query handlers registered.";
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

        // 1. Обробка кнопок (inline_keyboard)
        if (update.contains("callback_query")) {
            handleCallbackQuery(update["callback_query"].toObject());
            continue;
        }

        // 2. Обробка повідомлень (текст або фото)
        if (update.contains("message")) {
            QJsonObject message = update["message"].toObject();
            qint64 telegramId = message["from"].toObject()["id"].toVariant().toLongLong();
            UserState currentState = m_userState.value(telegramId);

            // --- КЛЮЧОВА ЗМІНА ТУТ ---
            // Якщо повідомлення містить фото АБО користувач у стані очікування фото
            if (message.contains("photo") || currentState == UserState::WaitingForJiraPhoto) {
                handleReportInput(message);
                continue; // Це не дозволить викликати checkBotUserStatus для фото
            }
            // -------------------------

            // Обробка інших станів (АЗС, коментарі тощо)
            if (currentState == UserState::WaitingForStationNumber ||
                currentState == UserState::WaitingForJiraTerminalID ||
                currentState == UserState::WaitingForJiraTaskId ||
                currentState == UserState::WaitingForManualTaskId ||
                currentState == UserState::WaitingForComment)
            {
                handleReportInput(message);
                continue;
            }

            // Якщо це не стан і не фото — перевіряємо як звичайну команду
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
    if (status.contains("user")) {
        logInfo() << "✅ SUCCESS: User data received for ID:" << userId; // Додайте цей лог
        if (m_users.contains(userId)) delete m_users.take(userId);
        m_users[userId] = User::fromJson(status["user"].toObject());
    } else {
        // Якщо ви побачите це в консолі — значить сервер не дає дані профілю
        logWarning() << "⚠️ WARNING: Server returned status, but NO 'user' object in JSON!";
    }

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
                   "<b>📋 Мої задачі:</b> <i>Відкриті призначені задачі</i>\n"
                   "<b>📊 Створити звіт:</b> <i>(в розробці)</i>\n";
    m_telegramClient->sendMessage(chatId, text);
}

void Bot::handleAdminHelp(const QJsonObject& message)
{
    qint64 chatId = message["from"].toObject()["id"].toVariant().toLongLong();
    logInfo() << "User (ACTIVE_ADMIN) called /help.";
    QString text = "<b>Допомога (Адміністратор):</b>\n\n"
                   "<b>📋 Мої задачі:</b> <i>Відкриті призначені задачі</i>\n"
                   "<b>📊 Створити звіт:</b> <i>(в розробці)</i>\n"
                   "<b>👑 Адмін: Запити:</b> <i>Запити на регістрацію в боті</i>\n"
                   "<b>👑 Адмін: Користувачі:</b> <i>Зареестровані користувачі</i>\n";
    m_telegramClient->sendMessage(chatId, text);
}

// void Bot::handleMyTasks(const QJsonObject& message)
// {
//     qint64 telegramId = message["from"].toObject()["id"].toVariant().toLongLong();
//     logInfo() << "Bot: User called 'Мої задачі' (" << telegramId << ").";

//     // 1. Надсилаємо повідомлення про очікування
//     m_telegramClient->sendMessage(telegramId, "Завантажую ваші відкриті задачі Redmine...");

//     // 2. Викликаємо метод для ініціації запиту до нашого Вебсервера
//     // Використовуємо ApiClient::instance(), оскільки ApiClient був правильно доданий до контексту Bot::Bot
//     ApiClient::instance().fetchRedmineTasks(telegramId);
// }


void Bot::handleMyTasks(const QJsonObject& message)
{
    qint64 chatId = message["from"].toObject()["id"].toVariant().toLongLong();
    logInfo() << "User called 'Мої задачі' (" << chatId << "). Launching task hub menu.";

    // --- 1. Створення Inline-клавіатури ---
    QJsonObject keyboard;
    QJsonArray rows;
    QJsonArray row1;

    // Кнопка 1: Redmine (Callback: tasks:show:redmine)
    row1.append(QJsonObject{
        {"text", "🔴 Redmine"},
        {"callback_data", "tasks:show:redmine"}
    });

    // Кнопка 2: Jira (Заглушка, Callback: tasks:show:jira)
    row1.append(QJsonObject{
        {"text", "🔵 Jira"},
        {"callback_data", "tasks:show:jira"}
    });

    rows.append(row1);
    keyboard["inline_keyboard"] = rows;

    // --- 2. Відправка повідомлення ---
    QString messageText = "Оберіть систему управління задачами, з якої бажаєте отримати список завдань:";

    // Використовуємо sendMessageWithInlineKeyboard (замість sendMessage та fetchRedmineTasks)
    m_telegramClient->sendMessageWithInlineKeyboard(chatId, messageText, keyboard);
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

    m_userClientCache[telegramId] = clients; // Зберігаємо список у кеш

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
 * @brief (ОНОВЛЕНО/РЕФАКТОРИНГ) Головний "мозок" (маршрутизатор).
 * Тепер використовує QMap'и для виклику правильного обробника.
 */
void Bot::handleCallbackQuery(const QJsonObject& callbackQuery)
{
    QString data = callbackQuery["data"].toString();
    QStringList parts = data.split(":"); // "prefix:action:arg1:arg2"

    if (parts.isEmpty()) {
        logWarning() << "Received callback query with empty data.";
        m_telegramClient->answerCallbackQuery(callbackQuery["id"].toString());
        return;
    }

    QString prefix = parts.at(0); // "clients", "client", "stations", "station"
    QString action = (parts.count() > 1) ? parts.at(1) : ""; // "main", "select", "list", "map"

    // Оголошуємо змінну для обробника
    CallbackHandler handler = nullptr;

    // 1. Шукаємо обробник у відповідній мапі
    if (prefix == "clients") {
        handler = m_clientsHandlers.value(action, &Bot::handleCallbackUnknown);
    } else if (prefix == "client") {
        handler = m_clientHandlers.value(action, &Bot::handleCallbackUnknown);
    } else if (prefix == "stations") {
        handler = m_stationsHandlers.value(action, &Bot::handleCallbackUnknown);
    } else if (prefix == "station") {
        handler = m_stationHandlers.value(action, &Bot::handleCallbackUnknown);
    } else if (prefix == "tasks") {
        handler = m_tasksHandlers.value(action, &Bot::handleCallbackUnknown);
    } else if (prefix == "report") {
        handler = m_reportHandlers.value(action, &Bot::handleCallbackUnknown);
    } else {
        // Якщо префікс невідомий (напр., "noop")
        handler = &Bot::handleCallbackUnknown;
    }

    // 2. Викликаємо знайдений обробник
    if (handler) {
        (this->*handler)(callbackQuery, parts);
    } else {
        // Цей 'else' не мав би спрацювати, але для безпеки
        logWarning() << "No handler found for callback data:" << data;
        m_telegramClient->answerCallbackQuery(callbackQuery["id"].toString());
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

//

/**
 * @brief (ОНОВЛЕНО) Отримано деталі АЗС.
 * Форматує повідомлення з назвою клієнта, адресою та телефоном.
 */
void Bot::onStationDetailsReceived(const QJsonObject& station, qint64 telegramId, int clientId)
{
    logInfo() << "Fetched details for station:" << station["terminal_no"].toString();

    // --- 1. Отримуємо назву клієнта з нашого нового кешу ---
    QString clientName = "<i>N/A</i>";
    if (m_userClientCache.contains(telegramId)) {
        for (const QJsonValue& val : m_userClientCache.value(telegramId)) {
            if (val.toObject()["client_id"].toInt() == clientId) {
                clientName = val.toObject()["client_name"].toString();
                break;
            }
        }
    }
    // --- КІНЕЦЬ ---

    // --- 2. Отримуємо всі дані про АЗС з JSON ---
    QString stationName = station["name"].toString();
    QString termNo = station["terminal_no"].toString();
    QString address = station["address"].toString();
    QString phone = station["phone"].toString();
//    int stationId = station["station_id"].toInt();
    double latitude = station["latitude"].toDouble(0.0);
    double longitude = station["longitude"].toDouble(0.0);

    if (address.isEmpty()) address = "<i>N/A</i>";
    if (phone.isEmpty()) phone = "<i>N/A</i>";

    // --- 3. Формуємо текст (з оновленим заголовком) ---
    QStringList textLines;

    // --- ОНОВЛЕНО ТУТ ---
    textLines.append(QString("<b>🏪 %1 %2</b>").arg(clientName, stationName));
    // --- КІНЕЦЬ ОНОВЛЕННЯ ---

    textLines.append(QString("<b>⛽ Термінал:</b> %1").arg(termNo));
    textLines.append(QString("<b>📍 Адреса:</b> %1").arg(address));
    textLines.append(QString("<b>📞 Телефон:</b> %1").arg(phone));

    QString statusActive = station["is_active"].toBool() ? "Активна" : "Неактивна";
    QString statusWork = station["is_working"].toBool() ? "В роботі" : "Не в роботі";
    textLines.append(QString("<b>ℹ️ Статус:</b> %1, %2").arg(statusActive, statusWork));

    QString text = textLines.join("\n");

    // --- 4. Формуємо нові кнопки (ОНОВЛЕНИЙ МАКЕТ) ---
    QJsonObject keyboard;
    QJsonArray rows;

    // --- Ряд 1: Заглушки (3 кнопки в ряд) ---
    QJsonArray row1;
    // !!! [ЗМІНА ТУТ] !!!
    // Було: row1.append(QJsonObject{{"text", "РРО"}, {"callback_data", "station:stub"}});

    // Стало: Формуємо реальний callback для РРО
    // termNo - це змінна, яку ви отримали вище: QString termNo = station["terminal_no"].toString();
    QString posCallback = QString("station:pos:%1:%2").arg(clientId).arg(termNo);

    // Додаємо кнопку (використовуємо той самий стиль QJsonObject, що й у вас в коді)
    row1.append(QJsonObject{{"text", "📠 РРО"}, {"callback_data", posCallback}});
    // !!! [КІНЕЦЬ ЗМІНИ] !!!



    // !!! ВИПРАВЛЕННЯ: ФОРМУЄМО КНОПКУ ТРК У ТОМУ Ж СТИЛІ !!!
    QString dispCallback = QString("station:disp:%1:%2").arg(clientId).arg(termNo);

    // Додаємо кнопку "Конфігурація ТРК"
    row1.append(QJsonObject{{"text", "⛽ ПРК"}, {"callback_data", dispCallback}});



    // Формуємо callback: station:tanks:clientId:termNo
    QString tanksCallback = QString("station:tanks:%1:%2").arg(clientId).arg(termNo);
    // Додаємо кнопку
    row1.append(QJsonObject{{"text", "🛢 Резервуари"}, {"callback_data", tanksCallback}});
    rows.append(row1);

    // --- Ряд 2: Мапа ---
    QJsonArray row2;
    QString mapCallbackData;

    if (latitude != 0.0 && longitude != 0.0) {
        mapCallbackData = QString("station:map:%1:%2").arg(latitude).arg(longitude);
    } else {
        mapCallbackData = "station:map:null";
    }
    row2.append(QJsonObject{{"text", "🗺️ Показати на мапі"}, {"callback_data", mapCallbackData}});
    rows.append(row2);

    // --- Ряд 3: Назад ---
    QJsonArray row3;
    row3.append(QJsonObject{{"text", "⬅️ Назад"}, {"callback_data", QString("client:select:%1").arg(clientId)}});
    rows.append(row3);

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
    const int termWidth = 5;
    const int nameWidth = 24;
    const int statusWidth = 3;

    tableRows.append(QString("%1|%2 |%3|%4")
                         .arg("ID", -termWidth)
                         .arg("Назва АЗС", -nameWidth)
                         .arg("Акт.", -statusWidth)
                         .arg("Роб.", -statusWidth));
    tableRows.append(QString(45, '-')); // Ваша виправлена довжина

    for (const QJsonValue& val : pageStations) {
        QJsonObject s = val.toObject();
        QString termNo = s["terminal_no"].toString();
        QString name = s["name"].toString();
        if (name.length() > nameWidth) {
            name = name.left(nameWidth - 1) + ".";
        }
        QString active = s["is_active"].toBool() ? " ✅" : " ❌";
        QString working = s["is_working"].toBool() ? " ✅" : " ❌";

        tableRows.append(QString("%1|%2 |%3|%4")
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
        m_telegramClient->editMessageText(telegramId, messageId, messageBody, keyboard, false);
    }
}


//

// --- (НОВІ ОБРОБНИКИ ДЛЯ INLINE-КНОПОК) ---
/**
 * @brief Обробник для "clients:main" (Повернутися до списку клієнтів)
 */
void Bot::handleCallbackClientsMain(const QJsonObject& query, const QStringList& parts)
{
    qint64 telegramId = query["message"].toObject()["chat"].toObject()["id"].toVariant().toLongLong();
    QString callbackQueryId = query["id"].toString();

    // 1. Прибираємо "годинник" та показуємо "друкує"
    m_telegramClient->answerCallbackQuery(callbackQueryId);
    m_telegramClient->sendChatAction(telegramId, "typing");

    // 2. Викликаємо API для завантаження списку клієнтів.
    // Відповідь обробляється onBotClientsReceived, який відображає список.
    m_apiClient.fetchBotClients(telegramId);
}

/**
 * @brief (НОВИЙ) Обробник для "client:select:<clientId>" (Меню АЗС)
 */
void Bot::handleCallbackClientSelect(const QJsonObject& query, const QStringList& parts)
{
    qint64 chatId = query["message"].toObject()["chat"].toObject()["id"].toVariant().toLongLong();
    int messageId = query["message"].toObject()["message_id"].toInt();
    QString callbackQueryId = query["id"].toString();

    if (parts.count() < 3) return; // Захист
    int clientId = parts.at(2).toInt(); // "client:select:10"

    QJsonObject keyboard;
    QJsonArray rows;
    QJsonArray row1;
    row1.append(QJsonObject{
        {"text", "⌨️ Ввести номер АЗС"},
        {"callback_data", QString("stations:enter:%1").arg(clientId)}
    });
    row1.append(QJsonObject{
        {"text", "📋 Список АЗС"},
        {"callback_data", QString("stations:list:%1").arg(clientId)}
    });

    rows.append(row1);
    QJsonArray row2;
    row2.append(QJsonObject{
        {"text", "⬅️ Назад (до клієнтів)"},
        {"callback_data", "clients:main"}
    });
    rows.append(row2);
    keyboard["inline_keyboard"] = rows;

    m_telegramClient->editMessageText(chatId, messageId, "<b>Оберіть дію:</b>", keyboard, false);
    m_telegramClient->answerCallbackQuery(callbackQueryId);
}

/**
 * @brief (НОВИЙ) Обробник для "stations:list:<clientId>"
 */
void Bot::handleCallbackStationsList(const QJsonObject& query, const QStringList& parts)
{
    qint64 chatId = query["message"].toObject()["chat"].toObject()["id"].toVariant().toLongLong();
    QString callbackQueryId = query["id"].toString();

    if (parts.count() < 3) return; // Захист
    int clientId = parts.at(2).toInt(); // "stations:list:10"

    m_telegramClient->answerCallbackQuery(callbackQueryId, "Завантажую список...");
    m_apiClient.fetchStationsForClient(chatId, clientId);
}

/**
 * @brief (НОВИЙ) Обробник для "stations:enter:<clientId>"
 */
void Bot::handleCallbackStationsEnter(const QJsonObject& query, const QStringList& parts)
{
    qint64 telegramId = query["from"].toObject()["id"].toVariant().toLongLong();
    QString callbackId = query["id"].toString();

    // 1. Сценарій кнопки "Назад" (stations:enter:CLIENT_ID:TERMINAL_ID)
    // parts: ["stations", "enter", "clientId", "terminalId"]
    if (parts.count() >= 4) {
        int clientId = parts.at(2).toInt();
        QString terminalNo = parts.at(3);

        // Зберігаємо контекст про всяк випадок
        m_userClientContext[telegramId] = clientId;

        m_telegramClient->answerCallbackQuery(callbackId, "Завантаження картки АЗС...");

        // Відразу викликаємо API
        m_apiClient.fetchStationDetails(telegramId, clientId, terminalNo);
        return;
    }

    // 2. Сценарій ручного вводу (stations:enter:CLIENT_ID)
    // parts: ["stations", "enter", "clientId"]
    if (parts.count() >= 3) {
        int clientId = parts.at(2).toInt();

        // !!! ВИПРАВЛЕННЯ: ЗБЕРІГАЄМО КОНТЕКСТ КЛІЄНТА !!!
        m_userClientContext[telegramId] = clientId;
        // ------------------------------------------------

        m_userState[telegramId] = UserState::WaitingForStationNumber;

        m_telegramClient->sendMessage(telegramId, "🔢 <b>Введіть номер терміналу (АЗС):</b>");
        m_telegramClient->answerCallbackQuery(callbackId);
    } else {
        // Якщо раптом прийшло щось без ID клієнта
        logWarning() << "Invalid stations:enter callback:" << parts;
        m_telegramClient->answerCallbackQuery(callbackId, "? Помилка навігації. Спробуйте ще раз.");
    }
}

/**
 * @brief (НОВИЙ) Обробник для "stations:page:<clientId>:<page>"
 */
void Bot::handleCallbackStationsPage(const QJsonObject& query, const QStringList& parts)
{
    qint64 chatId = query["message"].toObject()["chat"].toObject()["id"].toVariant().toLongLong();
    int messageId = query["message"].toObject()["message_id"].toInt();
    QString callbackQueryId = query["id"].toString();

    if (parts.count() < 4) return; // Захист
    int clientId = parts.at(2).toInt();
    int page = parts.at(3).toInt();

    sendPaginatedStations(chatId, clientId, page, messageId);
    m_telegramClient->answerCallbackQuery(callbackQueryId);
}

/**
 * @brief (НОВИЙ) Обробник для "stations:close"
 */
void Bot::handleCallbackStationsClose(const QJsonObject& query, const QStringList& parts)
{
    qint64 chatId = query["message"].toObject()["chat"].toObject()["id"].toVariant().toLongLong();
    int messageId = query["message"].toObject()["message_id"].toInt();
    QString callbackQueryId = query["id"].toString();

    m_telegramClient->editMessageText(chatId, messageId, "<i>Список АЗС закрито.</i>", QJsonObject(), false);
    m_userStationCache.remove(chatId); // Чистимо кеш
    m_telegramClient->answerCallbackQuery(callbackQueryId);
}

/**
 * @brief (НОВИЙ) Обробник для "station:stub"
 */
void Bot::handleCallbackStationStub(const QJsonObject& query, const QStringList& parts)
{
    QString callbackQueryId = query["id"].toString();
    m_telegramClient->answerCallbackQuery(callbackQueryId, "Функція в розробці...");
}

/**
 * @brief (НОВИЙ) Обробник для "station:map:<lat>:<lon>" або "station:map:null"
 */
void Bot::handleCallbackStationMap(const QJsonObject& query, const QStringList& parts)
{
    qint64 chatId = query["message"].toObject()["chat"].toObject()["id"].toVariant().toLongLong();
    QString callbackQueryId = query["id"].toString();

    if (parts.count() < 3) return; // Захист

    // parts = ["station", "map", ...]
    if (parts.at(2) == "null") {
        m_telegramClient->answerCallbackQuery(callbackQueryId, "❌ Координати для цієї АЗС відсутні.");
    } else if (parts.count() == 4) {
        double lat = parts.at(2).toDouble();
        double lon = parts.at(3).toDouble();
        m_telegramClient->sendLocation(chatId, lat, lon);
        m_telegramClient->answerCallbackQuery(callbackQueryId);
    }
}

/**
 * @brief (НОВИЙ) Обробник для невідомих колбеків
 */
void Bot::handleCallbackUnknown(const QJsonObject& query, const QStringList& parts)
{
    QString callbackQueryId = query["id"].toString();
    logWarning() << "Received unknown callback query:" << parts.join(":");
    m_telegramClient->answerCallbackQuery(callbackQueryId);
}

// --- (КІНЕЦЬ НОВИХ ОБРОБНИКІВ) ---

void Bot::handleCallbackStationPos(const QJsonObject& query, const QStringList& parts)
{
    // Формат callback: station:pos:<clientId>:<terminalId>
    // parts[0]="station", parts[1]="pos", parts[2]=clientId, parts[3]=terminalId

    if (parts.count() < 4) {
        logWarning() << "Invalid POS callback format:" << parts;
        return;
    }

    int clientId = parts.at(2).toInt();
    int terminalId = parts.at(3).toInt();

    // Отримуємо telegramId того, хто натиснув
    qint64 telegramId = query["from"].toObject()["id"].toVariant().toLongLong();
    QString callbackId = query["id"].toString();

    // Показуємо користувачеві "годинник" (що запит пішов)
    m_telegramClient->answerCallbackQuery(callbackId, "Завантаження даних РРО...");

    // Викликаємо API
    m_apiClient.fetchStationPosData(clientId, terminalId, telegramId);
}

void Bot::onStationPosDataReceived(const QJsonArray& data, int clientId, int terminalId, qint64 telegramId)
{
    if (data.isEmpty()) {
        m_telegramClient->sendMessage(telegramId, QString("ℹ️ <b>Інформація про РРО відсутня</b> для АЗС %1.").arg(terminalId));
        return;
    }

    QString message = QString("📠 <b>РРО на АЗС %1</b>\n\n").arg(terminalId);

    for (const QJsonValue& val : data) {
        QJsonObject pos = val.toObject();

        // Читаємо дані
        int posId = pos["pos_id"].toInt();
        QString manufacturer = pos["manufacturer"].toString();
        QString model = pos["model"].toString();
        QString factoryNum = pos["factory_number"].toString();
        QString taxNum = pos["tax_number"].toString();
        QString dateReg = pos["reg_date"].toString();

        // --- ЧИТАЄМО НОВІ ПОЛЯ ---
        QString ver = pos["version"].toString();
        QString muk = pos["muk_version"].toString();
        // -------------------------

        // 1. Заголовок
        message += QString("🔹 <b>Каса №%1</b>").arg(posId);
        if (!manufacturer.isEmpty() || !model.isEmpty()) {
            message += QString(" %1").arg(model);
        }
        message += "\n";

        // 2. Основні номери
        if (!factoryNum.isEmpty()) message += QString("   ЗН: <code>%1</code>\n").arg(factoryNum);
        if (!taxNum.isEmpty())     message += QString("   ФН: <code>%1</code>\n").arg(taxNum);

        // 3. --- ВИВЕДЕННЯ ВЕРСІЙ ---
        if (!ver.isEmpty() || !muk.isEmpty()) {
            QString vStr;
            if (!ver.isEmpty()) vStr += QString("ПО: %1").arg(ver);
            if (!ver.isEmpty() && !muk.isEmpty()) vStr += " | "; // Розділювач, якщо є обидві
            if (!muk.isEmpty()) vStr += QString("МУК: %1").arg(muk);

            message += QString("   🛠 %1\n").arg(vStr);
        }
        // ---------------------------

        if (!dateReg.isEmpty())    message += QString("   📅 Рєєстрація %1\n").arg(dateReg);

        message += "\n";
    }
    // --- 2. КНОПКА "НАЗАД" ---
    QJsonObject keyboard;
    QJsonArray rows;
    QJsonArray rowBack;

    // ГОЛОВНИЙ МОМЕНТ:
    // Ми формуємо команду "stations:enter" і додаємо туди ID.
    // Це змушує бота думати: "О, користувач вибрав АЗС №1001 клієнта №3".
    // І бот одразу покаже картку станції (ту, що на скріншоті).
    QString backCallback = QString("stations:enter:%1:%2").arg(clientId).arg(terminalId);

    rowBack.append(QJsonObject{
        {"text", "⬅️ Назад до АЗС"},
        {"callback_data", backCallback}
    });

    rows.append(rowBack);
    keyboard["inline_keyboard"] = rows;

    // 3. Відправляємо
    m_telegramClient->sendMessageWithInlineKeyboard(telegramId, message, keyboard);
}


void Bot::onStationPosDataFailed(const ApiError& error, qint64 telegramId)
{
    m_telegramClient->sendMessage(telegramId, "❌ Не вдалося отримати дані РРО.\n" + error.errorString);
}


void Bot::handleCallbackStationTanks(const QJsonObject& query, const QStringList& parts)
{
    // parts: station:tanks:clientId:terminalId
    if (parts.count() < 4) return;

    int clientId = parts.at(2).toInt();
    int terminalId = parts.at(3).toInt();

    qint64 telegramId = query["from"].toObject()["id"].toVariant().toLongLong();
    QString callbackId = query["id"].toString();

    m_telegramClient->answerCallbackQuery(callbackId, "Завантаження резервуарів...");

    m_apiClient.fetchStationTanks(clientId, terminalId, telegramId);
}

void Bot::onStationTanksReceived(const QJsonArray& data, int clientId, int terminalId, qint64 telegramId)
{
    QString message;

    if (data.isEmpty()) {
        message = QString("ℹ️ <b>Резервуари не знайдені</b> для АЗС %1.").arg(terminalId);
    } else {
        // Заголовок
        message = QString("🏭 <b>АЗС №%1: Паспорт резервуарів</b>\n").arg(terminalId);
        message += "➖➖➖➖➖➖➖➖➖➖\n\n";

        for (const QJsonValue& val : data) {
            QJsonObject t = val.toObject();

            // Читаємо ВСІ необхідні поля з DbManager (snake_case)
            int tankId = t["tank_id"].toInt();
            QString fuelName = t["fuel_name"].toString();
            QString fuelShortname = t["fuel_shortname"].toString();

            int maxVol = t["max_vol"].toInt();
            int minVol = t["min_vol"].toInt();
            int deadMax = t["dead_max"].toInt();
            int deadMin = t["dead_min"].toInt();
            int tubeVol = t["tube_vol"].toInt();

            // --- ФОРМАТУВАННЯ ЗГІДНО СПЕЦИФІКАЦІЇ ---

            // Line 1 (Header)
            message += QString("🔹 <b>Резервуар %1</b> – %2, %3:\n")
                           .arg(tankId)
                           .arg(fuelName.isEmpty() ? fuelShortname : fuelName)
                           .arg(fuelShortname);

            // Line 2 (Volume Limits)
            message += QString("   🔽 Min: %1 | 🔼 Max: %2\n")
                           .arg(minVol)
                           .arg(maxVol);

            // Line 3 (Dead/Unusable Limits)
            message += QString("   📏 Рівномір: %1 - %2\n")
                           .arg(deadMin)
                           .arg(deadMax);

            // Line 4 (Pipe Volume)
            message += QString("   🏭 Трубопровід: %1\n").arg(tubeVol);

            message += "\n"; // Розділювач
        }
    }

    // --- КНОПКА "НАЗАД" ---
    QJsonObject keyboard;
    QJsonArray rows;
    QJsonArray rowBack;

    QString backCallback = QString("stations:enter:%1:%2").arg(clientId).arg(terminalId);

    rowBack.append(QJsonObject{
        {"text", "⬅️ Назад до АЗС"},
        {"callback_data", backCallback}
    });

    rows.append(rowBack);
    keyboard["inline_keyboard"] = rows;

    m_telegramClient->sendMessageWithInlineKeyboard(telegramId, message, keyboard);
}

void Bot::onStationTanksFailed(const ApiError& error, qint64 telegramId)
{
    m_telegramClient->sendMessage(telegramId, "❌ Не вдалося отримати дані резервуарів.\n" + error.errorString);
}


/**
 * @brief Обробник для callback-запиту "station:disp:<clientId>:<terminalId>"
 */
void Bot::handleCallbackStationDisp(const QJsonObject& query, const QStringList& parts)
{
    QString callbackQueryId = query["id"].toString();
    qint64 telegramId = query["message"].toObject()["chat"].toObject()["id"].toVariant().toLongLong();

    // Перевірка формату callback: [station, disp, clientId, terminalId]
    if (parts.count() != 4) {
        m_telegramClient->answerCallbackQuery(callbackQueryId, "❌ Некоректний формат запиту.");
        return;
    }

    int clientId = parts.at(2).toInt();
    int terminalId = parts.at(3).toInt();

    m_telegramClient->answerCallbackQuery(callbackQueryId); // Прибираємо "годинник"
    m_telegramClient->sendChatAction(telegramId, "typing"); // Показуємо "друкує"

    // Виклик API для отримання конфігурації ТРК
    m_apiClient.fetchDispenserConfig(clientId, terminalId, telegramId);
}

/**
 * @brief Перетворює масив ТРК/Пістолетів у формат Telegram (деревоподібний звіт з HTML).
 */
void Bot::onDispenserConfigReceived(const QJsonArray& config, int clientId, int terminalId, qint64 telegramId)
{
    logInfo() << "Call Bot::onDispenserConfigReceived. Final HTML rendering.";

    if (config.isEmpty()) {
        // Тут залишаємо HTML, оскільки sendMessageWithInlineKeyboard зазвичай використовує HTML-парсинг
        m_telegramClient->sendMessage(telegramId, QString("ℹ️ <b>Конфігурація ТРК відсутня</b> або всі вони неактивні."));
        return;
    }

    QString message = QString("⛽ <b>Конфігурація ТРК</b> на АЗС <code>%1</code>:\n\n").arg(terminalId);

    for (const QJsonValue& dispValue : config) {
        QJsonObject dispenser = dispValue.toObject();

        int dispId = dispenser["dispenser_id"].toInt();
        QString protocol = dispenser["protocol_name"].toString().trimmed();
        int port = dispenser["channel_port"].toInt();
        int speed = dispenser["channel_speed"].toInt();
        int address = dispenser["net_address"].toInt();
        int rs485Type = dispenser["rs485_type"].toInt();
        bool emulCounters = dispenser["emul_counters"].toInt() == 1;

        // Форматуємо TYPERS485 (2 або 4)
        QString rs485Str = (rs485Type == 2 || rs485Type == 4)
                               ? QString("%1-провідний").arg(rs485Type)
                               : "Невідомий тип";

        // Заголовок ТРК
        message += QString("🔹 <b>ПРК %1</b>: <i>%2</i>\n")
                       .arg(dispId)
                       .arg(protocol);

        // Технічні параметри
        message += QString("   → Порт: <code>%1</code>, Шв: <code>%2</code>, Адр: <code>%3</code>\n")
                       .arg(port)
                       .arg(speed)
                       .arg(address);

        message += QString("   → RS485: %1\n").arg(rs485Str);

        if (emulCounters) {
            // !!! ВИПРАВЛЕННЯ КОНФЛІКТУ: ВИКОРИСТОВУЄМО ТІЛЬКИ HTML <b> !!!
            message += QString("   → ⚠️ <b>Емуляція лічильників УВІМКНЕНА</b>\n");
        }


        // Обробка пістолетів (вкладений масив)
        QJsonArray nozzles = dispenser["nozzles"].toArray();
        if (nozzles.isEmpty()) {
            message += "  └ 🛠 <i>Пістолети відсутні</i>\n";
        } else {
            for (int i = 0; i < nozzles.count(); ++i) {
                QJsonObject nozzle = nozzles.at(i).toObject();

                int nozzleId = nozzle["nozzle_id"].toInt();
                int tankId = nozzle["tank_id"].toInt();
                QString fuelName = nozzle["fuel_shortname"].toString().trimmed();

                QString prefix = (i == nozzles.count() - 1) ? "  └ 🛠 " : "  ├ 🛠 ";

                message += QString("%1 Пістолет %2 (Резервуар %3) – <b>%4</b>\n")
                               .arg(prefix)
                               .arg(nozzleId)
                               .arg(tankId)
                               .arg(fuelName);
            }
        }
        message += "\n"; // Розділювач між ТРК
    }

    // --- 2. КНОПКА "НАЗАД" ---
    QJsonObject keyboard;
    QJsonArray rows;
    QJsonArray rowBack;

    QString backCallback = QString("stations:enter:%1:%2").arg(clientId).arg(terminalId);

    rowBack.append(QJsonObject{
        {"text", "⬅️ Назад до АЗС"},
        {"callback_data", backCallback}
    });

    rows.append(rowBack);
    keyboard["inline_keyboard"] = rows;

    // --- 3. Відправка ---
    m_telegramClient->sendMessageWithInlineKeyboard(telegramId, message, keyboard);
}

/**
 * @brief Обробляє помилки запиту конфігурації ПРК.
 */
void Bot::onDispenserConfigFailed(const ApiError& error, qint64 telegramId)
{
    logCritical() << "Failed to fetch dispenser config:" << error.errorString;
    QString errMsg = QString("❌ Помилка під час запиту конфігурації ТРК.\nСервер: <code>%1</code>")
                         .arg(error.errorString);
    m_telegramClient->sendMessage(telegramId, errMsg);
}


// Isengard/Bot/Bot.cpp

// ... (після існуючих слотів)

/**
 * @brief Форматує список задач у текстове повідомлення та надсилає користувачу Telegram.
 * @param tasks Масив JSON із задачами Redmine.
 * @param telegramId ID користувача, який ініціював запит.
 */
void Bot::onRedmineTasksFetched(const QJsonArray& tasks, qint64 telegramId, int /*userId*/)
{
    if (telegramId == 0) return;

    // !!! НОВА ЛОГІКА: МАРШРУТИЗАЦІЯ ЗА СТАНОМ !!!
    if (m_userState.value(telegramId) == UserState::WaitingForTaskSelection) {
        // Якщо користувач очікує вибору задачі для звіту, викликаємо спеціальний обробник меню
        handleRedmineTaskSelectionForReport(tasks, telegramId);
        // !!! ДУЖЕ ВАЖЛИВО: СКИДАЄМО СТАН ПІСЛЯ ВИКОНАННЯ !!!
        m_userState.remove(telegramId);
        return;
    }

    QString message;

    // Отримуємо URL Redmine (він вже гарантовано завантажений)
    const QString redmineUrl = AppParams::instance().getParam("Global", "RedmineBaseUrl").toString();

    if (tasks.isEmpty()) {
        message = "✅ <b>У вас немає відкритих задач Redmine, призначених вам.</b>";
    } else {
        // --- 1. ГРУПУВАННЯ ЗАДАЧ ЗА ПРОЄКТАМИ ---
        // QMap<Назва Проєкту, Список Задач>
        QMap<QString, QJsonArray> tasksByProject;

        for (const QJsonValue& val : tasks) {
            QJsonObject issue = val.toObject();
            // Зчитуємо назву проєкту
            QString projectName = issue["project"].toObject()["name"].toString();

            // Якщо мапа вже містить проєкт, додаємо задачу до існуючого масиву
            // Інакше, створюємо новий масив і додаємо задачу
            tasksByProject[projectName].append(val);
        }

        // Заголовок загального повідомлення
        message = QString("📝 <b>Ваші відкриті задачі Redmine (%1):</b>\n\n").arg(tasks.count());

        // --- 2. ФОРМУВАННЯ ВИВОДУ (Ітерація по проєктах) ---

        // Ітеруємо по проєктах (QMap автоматично сортує за ключем)
        QMapIterator<QString, QJsonArray> i(tasksByProject);
        while (i.hasNext()) {
            i.next();
            const QString projectName = i.key();
            const QJsonArray projectTasks = i.value();

            // Заголовок Проєкту
            message += QString("📁 <b>Проєкт: %1 (%2)</b>\n")
                           .arg(projectName)
                           .arg(projectTasks.count());

            // Ітеруємо по задачах у поточному проєкті
            for (const QJsonValue& val : projectTasks) {
                QJsonObject issue = val.toObject();

                int id = issue["id"].toInt();
                QString subject = issue["subject"].toString();
                QString status = issue["status"].toObject()["name"].toString();
                int statusId = issue["status"].toObject()["id"].toInt();

                const QString issueUrl = redmineUrl + "/issues/" + QString::number(id);

                // Вибір емодзі
                QString statusEmoji;
                if (statusId == 1) { // Новий
                    statusEmoji = "🟢";
                } else if (statusId == 2) { // В розробці
                    statusEmoji = "🛠️";
                } else if (statusId == 7) { // Відкладена
                    statusEmoji = "🟡";
                } else {
                    statusEmoji = "🔵";
                }

                // Екрануємо тему
                const QString escapedSubject = escapeHtml(subject.simplified());

                // Формуємо рядок задачі
                message += QString("  %1 <b>[#%2] [%3]</b> %4\n  <a href=\"%5\">➡️ Перейти до задачі</a>\n")
                               .arg(statusEmoji)      // %1
                               .arg(id)               // %2
                               .arg(status)           // %3
                               .arg(escapedSubject)   // %4
                               .arg(issueUrl);         // %5
            }

            // Додаємо додатковий пробіл після проєкту для розділення
            message += "\n";
        }
    }

    m_telegramClient->sendMessage(telegramId, message, true);
    m_userState.remove(telegramId);
}




/**
 * @brief Обробляє помилку завантаження задач.
 */
void Bot::onRedmineTasksFetchFailed(const ApiError& error, qint64 telegramId, int /*userId*/)
{
    if (telegramId == 0) return; // Забезпечення, що це був запит від бота

    QString errorMessage = QString("❌ Помилка завантаження задач Redmine: %1\n"
                                   "HTTP Status: %2. Можливо, не налаштовано ключ API Redmine або URL.")
                               .arg(error.errorString)
                               .arg(error.httpStatusCode);

    m_telegramClient->sendMessage(telegramId, errorMessage);
}


void Bot::handleZaglushka(const QJsonObject& message)

{

    qint64 chatId = message["from"].toObject()["id"].toVariant().toLongLong();

    logInfo() << "User called 'Zaglushka'.";

    m_telegramClient->sendMessage(chatId, "Функція наразі в розробці.");

}


/**
 * @brief Обробляє вибір трекера (Redmine/Jira) з Inline-меню.
 * Callback-формат: tasks:show:<tracker>
 */
void Bot::handleTaskTrackerSelection(const QJsonObject& query, const QStringList& parts)
{
    qint64 chatId = query["message"].toObject()["chat"].toObject()["id"].toVariant().toLongLong();
    // Використовуємо .toLongLong(), оскільки message_id може бути великим числом
    qint64 messageId = query["message"].toObject()["message_id"].toVariant().toLongLong();
    QString queryId = query["id"].toString();

    if (parts.size() < 3) {
        m_telegramClient->answerCallbackQuery(queryId, "Некоректний запит.");
        return;
    }

    QString tracker = parts.at(2); // "redmine" або "jira"

    // 1. Початкове повідомлення про завантаження (редагуємо оригінальне меню)
    QString loadingMessage = QString("Завантажую задачі з %1...").arg(tracker == "redmine" ? "Redmine" : "Jira");

    // [Виправлено: Передаємо 4 аргументи, 4-й - порожня клавіатура]
    m_telegramClient->editMessageText(chatId, messageId, loadingMessage, QJsonObject(), false);


    if (tracker == "redmine") {
        // --- Redmine: Запуск існуючого, робочого функціоналу ---
        logInfo() << "Bot: Starting Redmine tasks fetch for user" << chatId;
        m_apiClient.fetchRedmineTasks(chatId);

    } else if (tracker == "jira") {
        logInfo() << "Bot: Starting Jira tasks fetch for user" << chatId;
        m_apiClient.fetchJiraTasks(chatId); // Тільки цей рядок
        return;
    }

    // Відповідь Telegram (для Redmine, щоб зникло "годинник")
    // [Виправлено: Тільки 2 аргументи]
    m_telegramClient->answerCallbackQuery(queryId, "Завантаження розпочато...");
}



/**
 * @brief (НОВИЙ) Обробляє успішне отримання задач Jira.
 * Реалізує групування за проєктами, але БЕЗ АКТИВНИХ ПОСИЛАНЬ.
 */
void Bot::onJiraTasksFetched(const QJsonArray& tasks, qint64 telegramId)
{
    if (telegramId == 0) return;

    // --- 1. ПЕРЕВІРКА СТАНУ: ЧИ ОЧІКУЄ КОРИСТУВАЧ ВИБОРУ ЗАДАЧІ ДЛЯ ЗВІТУ? ---
    if (m_userState.value(telegramId) == UserState::WaitingForTaskSelection) {

        QJsonObject keyboard;
        QJsonArray rows;
        QString messageText;

        if (tasks.isEmpty()) {
            messageText = "📭 <b>Призначених задач Jira не знайдено.</b>\nСкористайтеся пошуком нижче:";
        } else {
            messageText = QString("✅ Знайдено %1 задач. Оберіть задачу Jira для звіту або скористайтеся пошуком:").arg(tasks.size());

            // Створюємо кнопки для кожної призначеної задачі
            for (const QJsonValue& value : tasks) {
                QJsonObject issue = value.toObject();
                QString key = issue["key"].toString();
                QString summary = issue["fields"].toObject()["summary"].toString();

                // Обмежуємо довжину тексту на кнопці
                if (summary.length() > 30) summary = summary.left(27) + "...";

                QJsonArray row;
                row.append(QJsonObject{
                    {"text", QString("[%1] %2").arg(key, summary)},
                    {"callback_data", QString("report:select_task:jira:%1").arg(key)}
                });
                rows.append(row);
            }
        }

        // --- ДОДАВАННЯ КНОПОК ПОШУКУ (згідно з планом) ---
        QJsonArray searchRow;
        searchRow.append(QJsonObject{
            {"text", "🔍 Пошук по АЗС"},
            {"callback_data", "report:search:jira:terminal"}
        });
        searchRow.append(QJsonObject{
            {"text", "🔢 Пошук по номеру"},
            {"callback_data", "report:search:jira:id"}
        });
        rows.append(searchRow);

        keyboard["inline_keyboard"] = rows;
        m_telegramClient->sendMessageWithInlineKeyboard(telegramId, messageText, keyboard);

        // Скидаємо стан, щоб наступні повідомлення не вважалися вибором задачі
        m_userState.remove(telegramId);
        return;
    }

    // --- 2. ВАШ ІСНУЮЧИЙ КОД ДЛЯ ТЕКСТОВОГО ВИВОДУ (/Мої задачі) ---

    const QString jiraUrl = AppParams::instance().getParam("Global", "JiraBaseUrl").toString();
    const bool urlIsAvailable = !jiraUrl.isEmpty();

    QString message;

    if (tasks.isEmpty()) {
        message = "✅ <b>У вас немає відкритих задач Jira, призначених вам.</b>";
    } else {
        // Групування за проєктами
        QMap<QString, QJsonArray> tasksByProject;

        for (const QJsonValue& val : tasks) {
            QJsonObject issue = val.toObject();
            QString projectName = issue["fields"].toObject()["project"].toObject()["name"].toString();
            tasksByProject[projectName].append(val);
        }

        message = QString("📝 <b>Ваші відкриті задачі Jira (%1):</b>\n\n").arg(tasks.count());

        QMapIterator<QString, QJsonArray> i(tasksByProject);
        while (i.hasNext()) {
            i.next();
            const QString projectName = i.key();
            const QJsonArray projectTasks = i.value();

            message += QString("📁 <b>Проєкт: %1 (%2)</b>\n")
                           .arg(projectName)
                           .arg(projectTasks.count());

            for (const QJsonValue& val : projectTasks) {
                QJsonObject issue = val.toObject();
                QJsonObject fields = issue["fields"].toObject();

                QString key = issue["key"].toString();
                QString summary = fields["summary"].toString();
                QString status = fields["status"].toObject()["name"].toString();

                QString statusEmoji;
                if (status.contains("Open", Qt::CaseInsensitive)) {
                    statusEmoji = "🟢";
                } else if (status.contains("Progress", Qt::CaseInsensitive)) {
                    statusEmoji = "🛠️";
                } else {
                    statusEmoji = "🔵";
                }

                const QString escapedSummary = escapeHtml(summary.simplified());

                if (urlIsAvailable) {
                    const QString issueUrl = jiraUrl + "/browse/" + key;
                    message += QString("  %1 <b><a href=\"%2\">[%3]</a> [%4]</b> %5\n")
                                   .arg(statusEmoji, issueUrl, key, status, escapedSummary);
                } else {
                    message += QString("  %1 <b>[%2] [%3]</b> %4\n")
                    .arg(statusEmoji, key, status, escapedSummary);
                }
            }
            message += "\n";
        }
    }

    m_telegramClient->sendMessage(telegramId, message);
}

/**
 * @brief (НОВИЙ) Обробляє помилку завантаження задач Jira.
 */
void Bot::onJiraTasksFetchFailed(const ApiError& error, qint64 telegramId)
{
    if (telegramId == 0) return;

    QString errorMessage = QString("❌ Помилка завантаження задач Jira: %1\n"
                                   "HTTP Status: %2. Можливо, не налаштовано ключ API Jira.")
                               .arg(error.errorString)
                               .arg(error.httpStatusCode);

    m_telegramClient->sendMessage(telegramId, errorMessage);
}


/**
 * @brief (НОВИЙ) Обробляє команду "📊 Створити звіт".
 * Запускає Inline-меню для вибору трекера.
 */
void Bot::handleCreateReport(const QJsonObject& message)
{
    qint64 chatId = message["from"].toObject()["id"].toVariant().toLongLong();
    logInfo() << "User called 'Створити звіт' (" << chatId << "). Launching report hub menu.";

    // --- 1. Створення Inline-клавіатури ---
    QJsonObject keyboard;
    QJsonArray rows;
    QJsonArray row1;

    // Кнопка 1: Redmine (Callback: report:select:redmine)
    row1.append(QJsonObject{
        {"text", "🔴 Redmine"},
        {"callback_data", "report:select:redmine"}
    });

    // Кнопка 2: Jira (Callback: report:select:jira)
    row1.append(QJsonObject{
        {"text", "🔵 Jira"},
        {"callback_data", "report:select:jira"}
    });

    rows.append(row1);
    keyboard["inline_keyboard"] = rows;

    // --- 2. Відправка повідомлення ---
    QString messageText = "Оберіть систему управління задачами, для якої бажаєте створити звіт:";

    m_telegramClient->sendMessageWithInlineKeyboard(chatId, messageText, keyboard);

    // !!! ТУТ БУДЕ ДОДАНО ЗБЕРЕЖЕННЯ СТАНУ: WAITING_FOR_REPORT_INIT !!!
    // m_userState[chatId] = UserState::WaitingForReportInit;
    // Але поки ми не визначили новий стан у Bot.h, ми це пропускаємо.
}

/**
 * @brief (НОВИЙ) Обробляє вибір трекера (Redmine/Jira) для створення звіту.
 * Callback-формат: report:select:<tracker>
 */
void Bot::handleReportTrackerSelection(const QJsonObject& query, const QStringList& parts)
{
    // Отримуємо ID чату та ID callback-запиту
    qint64 chatId = query["message"].toObject()["chat"].toObject()["id"].toVariant().toLongLong();
    QString queryId = query["id"].toString();

    // Перевірка наявності параметрів у callback_data (очікуємо report:select:trackerName)
    if (parts.size() < 3) {
        m_telegramClient->answerCallbackQuery(queryId, "Некоректний запит.");
        return;
    }

    QString tracker = parts.at(2); // "redmine" або "jira"

    // 1. ФІКСУЄМО КОНТЕКСТ ТА ВСТАНОВЛЮЄМО СТАН ОЧІКУВАННЯ
    // Цей стан дозволить обробникам onRedmineTasksFetched / onJiraTasksFetched
    // зрозуміти, що потрібно малювати кнопки, а не текстовий список.
    m_userState[chatId] = UserState::WaitingForTaskSelection;

    // Зберігаємо тип трекера (1 - Redmine, 2 - Jira) для подальшої валідації
    m_userClientContext[chatId] = (tracker == "redmine") ? 1 : 2;

    // Також ініціалізуємо контекст звіту, щоб пізніше знати, який трекер обрано
    m_reportContext[chatId]["tracker"] = tracker;

    // 2. ІНФОРМУЄМО КОРИСТУВАЧА ТА ВИКЛИКАЄМО API
    QString loadingMessage = QString("⌛ Завантажую ваші призначені задачі <b>%1</b>...").arg(tracker.toUpper());

    // Надсилаємо нове повідомлення про процес завантаження
    m_telegramClient->sendMessage(chatId, loadingMessage);

    if (tracker == "redmine") {
        logInfo() << "Bot: Initiating Redmine tasks fetch for reporting flow. User:" << chatId;
        m_apiClient.fetchRedmineTasks(chatId); // Виклик існуючого методу
    } else if (tracker == "jira") {
        logInfo() << "Bot: Initiating Jira tasks fetch for reporting flow. User:" << chatId;
        m_apiClient.fetchJiraTasks(chatId);    // Виклик існуючого методу
    } else {
        logWarning() << "Bot: Unknown tracker selected:" << tracker;
        m_userState.remove(chatId); // Скидаємо стан, якщо трекер невідомий
        m_telegramClient->answerCallbackQuery(queryId, "Помилка: невідомий трекер.");
        return;
    }

    // Прибираємо стан завантаження ("годинник") на кнопці в Telegram
    m_telegramClient->answerCallbackQuery(queryId);
}

// Bot.cpp (handleRedmineTaskSelectionForReport)

/**
 * @brief (НОВИЙ ПРИВАТНИЙ МЕТОД) Формує меню вибору задачі Redmine, додаючи опцію ручного вводу.
 * Викликається, коли стан користувача WaitingForTaskSelection.
 */
void Bot::handleRedmineTaskSelectionForReport(const QJsonArray& tasks, qint64 telegramId)
{
    // Поточний клієнт, збережений у контексті: 1=Redmine, 2=Jira
    int trackerType = m_userClientContext.value(telegramId);
    QString trackerPrefix = (trackerType == 1) ? "redmine" : "jira";

    QJsonObject keyboard;
    QJsonArray rows;

    QString messageText;

    // --- 1. Формування кнопок із призначеними задачами (якщо вони є) ---
    if (!tasks.isEmpty()) {
        messageText = QString("✅ Знайдено %1 задач. Оберіть одну зі списку або натисніть 'Ввести ID вручну':\n\n")
                          .arg(tasks.count());

        // --- НОВИЙ БЛОК: ФОРМУВАННЯ КНОПОК ОДНИМ СТОВПЦЕМ ---
        const int maxButtons = qMin(tasks.count(), 10); // Обмежимо, наприклад, першими 10
        const int maxSummaryLength = 40; // Максимальна довжина теми

        for (int k = 0; k < maxButtons; ++k) {
            QJsonObject task = tasks.at(k).toObject();
            int taskId = task["id"].toInt();
            QString summary = task["subject"].toString().trimmed();

            // Форматування тексту кнопки: [#ID] Тема...
            QString buttonText = QString("[#%1] %2").arg(taskId).arg(summary);

            if (summary.length() > maxSummaryLength) {
                summary = summary.left(maxSummaryLength) + "...";
            }
            buttonText += " " + summary;

            // Створення кнопки в окремому ряду
            QJsonArray taskRow;
            taskRow.append(QJsonObject{
                {"text", buttonText},
                // Callback: report:select_task:redmine:12345
                {"callback_data", QString("report:select_task:%1:%2").arg(trackerPrefix).arg(taskId)}
            });
            rows.append(taskRow);
        }
        // -----------------------------------------------------------------

        m_userState[telegramId] = UserState::WaitingForTaskSelection;
    }

    // --- 2. ДОДАВАННЯ КНОПКИ "ВВЕСТИ ВРУЧНУ" ---
    QJsonArray manualRow;
    manualRow.append(QJsonObject{
        {"text", "⌨️ Ввести ID вручну"},
        // Callback: report:manual_id:redmine
        {"callback_data", QString("report:manual_id:%1").arg(trackerPrefix)}
    });
    rows.append(manualRow);

    keyboard["inline_keyboard"] = rows;

    // --- 3. ВІДПРАВКА ЄДИНОГО ПОВІДОМЛЕННЯ З КЛАВІАТУРОЮ ---
    m_telegramClient->sendMessageWithInlineKeyboard(telegramId, messageText, keyboard);
}

/**
 * @brief Обробляє вибір задачі за Inline-кнопкою зі списку призначених.
 * Callback-формат: report:select_task:<tracker>:<taskId>
 */
void Bot::handleCallbackReportSelectTask(const QJsonObject& query, const QStringList& parts)
{
    qint64 chatId = query["message"].toObject()["chat"].toObject()["id"].toVariant().toLongLong();
    qint64 messageId = query["message"].toObject()["message_id"].toVariant().toLongLong();
    QString queryId = query["id"].toString();

    if (parts.size() < 4) return;

    QString tracker = parts.at(2); // "jira" або "redmine"
    QString taskId = parts.at(3);  // Наприклад, "AZS-46937"

    // 1. Обов'язково фіксуємо трекер та ID у контексті звіту
    m_reportContext[chatId]["tracker"] = tracker;
    m_reportContext[chatId]["taskId"] = taskId;

    // 2. ВАЖЛИВО: Оновлюємо m_userClientContext, щоб onTaskDetailsFetched
    // правильно розпізнав систему (1 - Redmine, 2 - Jira)
    m_userClientContext[chatId] = (tracker == "redmine") ? 1 : 2;

    m_telegramClient->answerCallbackQuery(queryId, "⌛ Завантажую деталі задачі...");

    if (tracker == "jira") {
        // Для Jira: запитуємо деталі, щоб показати універсальну картку (Етап 3 плану)
        // Після отримання даних спрацює onTaskDetailsFetched
        m_apiClient.fetchTaskDetails(tracker, taskId, chatId);
    } else {
        // Для Redmine: залишаємо старий флоу (перехід до вибору типу звіту)
        showReportMenu(chatId, taskId, tracker, true, messageId);
    }
}
/**
 * @brief (НОВИЙ) Обробляє натискання кнопки "Ввести ID вручну".
 * Переводить користувача у стан очікування тексту.
 * Callback-формат: report:manual_id:<tracker>
 */
void Bot::handleCallbackReportManualId(const QJsonObject& query, const QStringList& parts)
{
    qint64 chatId = query["message"].toObject()["chat"].toObject()["id"].toVariant().toLongLong();
    qint64 messageId = query["message"].toObject()["message_id"].toVariant().toLongLong();
    QString queryId = query["id"].toString();

    if (parts.size() < 3) {
        m_telegramClient->answerCallbackQuery(queryId, "Некоректний запит.");
        return;
    }

    QString tracker = parts.at(2); // redmine або jira

    // 1. Встановлюємо стан очікування ручного введення
    m_userState[chatId] = UserState::WaitingForManualTaskId;
    startSessionTimeout(chatId);

    // 2. Редагуємо повідомлення, щоб прибрати кнопки і попросити ID
    QString messageText = QString("🔢 <b>Введіть, будь ласка, повний ID задачі (%1):</b>")
                              .arg(tracker == "redmine" ? "#ID" : "KEY-XXXX");

    // Редагуємо повідомлення, щоб прибрати кнопки
    m_telegramClient->editMessageText(chatId, messageId, messageText, QJsonObject(), false);
    m_telegramClient->answerCallbackQuery(queryId, "Режим ручного вводу активовано.");

    logInfo() << "Report: Manual ID mode enabled for" << tracker;
}


/**
 * @brief (НОВИЙ) Єдиний обробник для текстового введення та файлів у режимі звіту.
 */
void Bot::handleReportInput(const QJsonObject& message)
{
    qint64 chatId = message["from"].toObject()["id"].toVariant().toLongLong();
    stopSessionTimeout(chatId);

    // Отримуємо текст, якщо він є (у повідомленні з фото тексту може не бути)
    QString text = message["text"].toString().trimmed();
    UserState currentState = m_userState.value(chatId);

    // --- 0. НОВИЙ БЛОК: ОБРОБКА ФОТО (ДЛЯ JIRA) ---
    // Перевіряємо цей стан першим, щоб не пропустити медіа-повідомлення
    if (currentState == UserState::WaitingForJiraPhoto) {
        if (message.contains("photo")) {
            QJsonArray photos = message["photo"].toArray();
            QString fileId = photos.last().toObject()["file_id"].toString();

            // Використовуємо існуючу мапу m_users
            User* user = m_users.value(chatId, nullptr);
            QString taskId = m_reportContext[chatId]["taskId"].toString();

            // ПРАВИЛЬНИЙ ПАРАМЕТР З ВАШОЇ БД
            QString root = AppParams::instance().getParam("Global", "StorageRootPath").toString();

            if (!user || root.isEmpty()) {
                if (!user) logCritical() << "IDENTIFICATION ERROR: User object is NULL in m_users for chatId:" << chatId;
                if (root.isEmpty()) logCritical() << "CONFIG ERROR: StorageRootPath is EMPTY from AppParams (Global/StorageRootPath)";
                m_telegramClient->sendMessage(chatId, "❌ Помилка ідентифікації або налаштувань сховища.");
                return;
            }

            QString saveDir = m_attachmentManager->prepareStoragePath(root, user, taskId);
            if (saveDir.isEmpty()) {
                m_telegramClient->sendMessage(chatId, "❌ Помилка створення папок на диску.");
                return;
            }

            QString fileName = QString("photo_%1.jpg").arg(QDateTime::currentMSecsSinceEpoch());
            QString fullPath = QDir(saveDir).absoluteFilePath(fileName);

            m_telegramClient->sendMessage(chatId, "⌛ Завантажую фото у локальний архів...");

            // ВИПРАВЛЕНО: Використовуємо метод getFile, який ми додали в TelegramClient
            m_telegramClient->getFile(fileId, [this, chatId, fullPath](const QString& tgFilePath) {
                // Беремо токен безпосередньо з клієнта, він там уже є з конструктора
                QString token = m_telegramClient->token();

                QUrl downloadUrl(QString("https://api.telegram.org/file/bot%1/%2").arg(token, tgFilePath));
                m_attachmentManager->downloadFile(downloadUrl, fullPath);
            });
            return;
        }
    }

    // --- УНІВЕРСАЛЬНА ПЕРЕВІРКА І СКИДАННЯ СТАНУ ПРИ ВВЕДЕННІ КОМАНД (БЕЗ ЗМІН) ---
    if (currentState == UserState::WaitingForManualTaskId ||
        currentState == UserState::WaitingForComment ||
        currentState == UserState::WaitingForJiraPhoto) // Додано перевірку для нового стану
    {
        bool isCommand = text.startsWith("/") ||
                         text.contains("Допомога") ||
                         text.contains("Мої задачі") ||
                         text.contains("Створити звіт") ||
                         text.contains("Клієнти") ||
                         text.contains("Адмін:");

        if (isCommand) {
            logWarning() << "Report: Command detected (" << text << ") while waiting for input. Resetting dialogue.";
            m_userState.remove(chatId);
            m_reportContext.remove(chatId);
            m_telegramClient->sendMessage(chatId, "⚠️ Діалог звітування скасовано.");
            m_apiClient.checkBotUserStatus(message);
            return;
        }
    }

    // --- 1. ОБРОБКА ВВЕДЕННЯ ID ЗАДАЧІ REDMINE/JIRA (БЕЗ ЗМІН) ---
    if (currentState == UserState::WaitingForManualTaskId) {
        logInfo() << "Report: Received manual Task ID input:" << text;
        if (text.isEmpty()) {
            m_telegramClient->sendMessage(chatId, "? ID задачі не може бути порожнім. Введіть, будь ласка, ID.");
            return;
        }
        m_reportContext[chatId]["taskId"] = text;
        m_userState[chatId] = UserState::ValidatingTask;
        int trackerType = m_userClientContext.value(chatId);
        QString selectedTracker = (trackerType == 1) ? "redmine" : "jira";
        m_apiClient.fetchTaskDetails(selectedTracker, text, chatId);

        QString response = QString("Проводиться валідація задачі <b>%1</b> (%2)...").arg(text).arg(selectedTracker);
        m_telegramClient->sendMessage(chatId, response);
        return;
    }

    // --- 2. ОБРОБКА ВВЕДЕННЯ КОМЕНТАРЯ REDMINE (БЕЗ ЗМІН) ---
    if (currentState == UserState::WaitingForComment) {
        logInfo() << "Report: Received comment text, finalizing report.";
        if (text.isEmpty()) {
            m_telegramClient->sendMessage(chatId, "❌ Текст коментаря/рішення не може бути порожнім.");
            return;
        }
        m_reportContext[chatId]["commentText"] = text;
        QVariantMap reportData = m_reportContext[chatId];
        QString taskId = reportData["taskId"].toString();
        QString tracker = reportData["tracker"].toString();
        QString reportType = reportData["reportType"].toString();

        QJsonObject payload;
        payload["tracker"] = tracker;
        payload["taskId"] = taskId;
        payload["action"] = reportType;
        payload["comment"] = text;

        QString actionName = (reportType == "close") ? "закриття" : "коментар";
        m_telegramClient->sendMessage(chatId, QString("🚀 Відправляю звіт (%1) по задачі <b>%2</b>...").arg(actionName, taskId));

        m_apiClient.reportTask(payload, chatId);
        m_userState[chatId] = UserState::ValidatingTask;
        return;
    }

    // --- 3. ОБРОБКА ПОШУКУ ЗА НОМЕРОМ JIRA (БЕЗ ЗМІН) ---
    if (currentState == UserState::WaitingForJiraTaskId) {
        QString cleanId = text.trimmed().toUpper();
        if (!cleanId.startsWith("AZS-")) {
            bool isNumeric;
            cleanId.toInt(&isNumeric);
            if (isNumeric) cleanId = "AZS-" + cleanId;
        }
        m_telegramClient->sendMessage(chatId, QString("🔎 Перевіряю задачу <b>%1</b>...").arg(cleanId));
        m_reportContext[chatId]["tracker"] = "jira";
        m_reportContext[chatId]["taskId"] = cleanId;
        m_userState.remove(chatId);
        m_apiClient.fetchTaskDetails("jira", cleanId, chatId);
        return;
    }

    // --- 4. ЗАХИСТ ВІД НЕОЧІКУВАНИХ ВКЛАДЕНЬ (МОДИФІКОВАНО) ---
    if (message.contains("photo") || message.contains("document")) {
        // Якщо ми в стані очікування фото, цей блок не спрацює завдяки return вище
        m_telegramClient->sendMessage(chatId, "❌ Обробка вкладень не підтримується для цього етапу. Оберіть команду з меню.");
        return;
    }
}


// Bot.cpp (onTaskDetailsFetched)

/**
 * @brief Успішно отримано деталі задачі (валідація пройшла успішно).
 */
void Bot::onTaskDetailsFetched(const QJsonObject& taskDetails, qint64 telegramId)
{
    if (telegramId == 0) return;

    // 1. Отримуємо дані з контексту звіту
    // Переконуємося, що ми використовуємо актуальні дані, зафіксовані при натисканні кнопки
    QString taskId = m_reportContext[telegramId]["taskId"].toString();
    QString tracker = m_reportContext[telegramId]["tracker"].toString();

    logInfo() << "Bot: onTaskDetailsFetched context check - Tracker:" << tracker << "Task:" << taskId;

    // --- ЛОГІКА ДЛЯ JIRA (Етап 3 плану) ---
    if (tracker == "jira") {
        logInfo() << "Bot: Jira task details successfully received. Showing universal card for:" << taskId;

        // Викликаємо універсальний метод відображення картки (тема, статус, опис, кнопки)
        showJiraTaskCard(telegramId, taskDetails);

        // Скидаємо стан очікування валідації та зупиняємо таймер
        m_userState.remove(telegramId);
        stopSessionTimeout(telegramId);

        // ВАЖЛИВО: Виходимо, щоб не ініціювати призначення assignTaskToSelf
        return;
    }

    // --- ЛОГІКА ДЛЯ REDMINE (ЗАЛИШАЄТЬСЯ БЕЗ ЗМІН) ---
    // Для Redmine ми продовжуємо старий флоу автоматичного призначення
    QString subject = taskDetails.contains("subject") ? taskDetails["subject"].toString() : taskDetails["summary"].toString();
    QString statusName = taskDetails["status"].toObject()["name"].toString();

    QString messageText = QString("✅ Задача <b>%1</b> (REDMINE) знайдена:\n"
                                  "   Тема: %2\n"
                                  "   Статус: %3\n"
                                  "   <b>Проводиться призначення задачі на вас...</b>")
                              .arg(taskId)
                              .arg(escapeHtml(subject.simplified()))
                              .arg(statusName);

    m_telegramClient->sendMessage(telegramId, messageText);

    // Змінюємо стан на очікування призначення та викликаємо API сервера
    m_userState[telegramId] = UserState::WaitingForAssignment;
    stopSessionTimeout(telegramId);

    // Викликаємо метод призначення саме для Redmine
    m_apiClient.assignTaskToSelf("redmine", taskId, telegramId);
}

/**
 * @brief Помилка отримання деталей задачі (валідація провалилася).
 */
void Bot::onTaskDetailsFetchFailed(const ApiError& error, qint64 telegramId)
{
    if (telegramId == 0) return;

    QString taskId = m_reportContext[telegramId]["taskId"].toString();
    // 1. Скидаємо стан до очікування нового ID
    m_userState[telegramId] = UserState::WaitingForManualTaskId;
    startSessionTimeout(telegramId);

    QString selectedTracker = (m_userClientContext.value(telegramId) == 1) ? "Redmine" : "Jira";
    QString trackerPrefix = (selectedTracker == "Redmine") ? "redmine" : "jira";

    // --- ФОРМУВАННЯ ПОВІДОМЛЕННЯ ТА КЛАВІАТУРИ ---

    QString errorMessage;

    // --- ПРИХОВУЄМО ТЕХНІЧНІ ДЕТАЛІ (ВИПРАВЛЕННЯ ТУТ) ---

    // Сценарій 1: Найбільш ймовірно, задача закрита, неіснує або доступ заборонено
    if (error.httpStatusCode == 400 || error.httpStatusCode == 403 || error.httpStatusCode == 404) {

        // Використовуємо лише зрозумілий текст помилки, який прийшов від сервера (error.errorString)
        // Якщо errorString містить технічні деталі (Network error), ми його просто ігноруємо

        QString userFriendlyError = "Задача не знайдена, вже закрита або у вас немає доступу.";

        // Якщо сервер повернув більш детальну помилку у errorString (що очікується)
        if (!error.errorString.isEmpty() && !error.errorString.contains("Network error", Qt::CaseInsensitive)) {
            userFriendlyError = error.errorString;
        }

        errorMessage = QString("❌ Задача <b>%1</b> (%2) не може бути взята в роботу:\n"
                               "   %3 (HTTP %4).\n"
                               "   Можливо, вона вже закрита або не призначена на вас.")
                           .arg(taskId)
                           .arg(selectedTracker.toUpper())
                           .arg(userFriendlyError)
                           .arg(error.httpStatusCode);

    } else {
        // Сценарій 2: Загальна помилка сервера (наприклад, 5xx)
        errorMessage = QString("❌ Виникла помилка на сервері при валідації задачі %1:\n"
                               "   %2 (HTTP %3).")
                           .arg(selectedTracker)
                           .arg(error.errorString) // Тут можна залишити errorString для діагностики 5хх
                           .arg(error.httpStatusCode);
    }

    errorMessage += "\n\nВведіть коректний ID задачі ще раз.";

    // --- КНОПКА З ПОСИЛАННЯМ НА ЗАДАЧУ ---

    QJsonObject keyboard;
    QJsonArray rows;
    QJsonArray row1;

    // Визначення базового URL для посилання
    QString baseUrl;
    if (trackerPrefix == "redmine") {
        baseUrl = AppParams::instance().getParam("Global", "RedmineBaseUrl").toString();
    }

    if (!baseUrl.isEmpty()) {
        QString issueUrl = (trackerPrefix == "redmine")
        ? baseUrl + "/issues/" + taskId
        : baseUrl;

        row1.append(QJsonObject{
            {"text", QString("➡️ Перейти до задачі %1").arg(taskId)},
            {"url", issueUrl} // Кнопка-посилання (url button)
        });
        rows.append(row1);
        keyboard["inline_keyboard"] = rows;

        m_telegramClient->sendMessageWithInlineKeyboard(telegramId, errorMessage, keyboard);
    } else {
        // Якщо URL не налаштований, шлемо повідомлення без кнопки
        m_telegramClient->sendMessage(telegramId, errorMessage);
    }

    // Очищуємо кеш задачі
    m_reportContext.remove(telegramId);
}


/**
 * @brief Успішне призначення задачі на себе.
 * Перехід до вибору типу звіту (Коментар/Закрити).
 */
void Bot::onAssignTaskSuccess(const QJsonObject& response, qint64 telegramId)
{
    QString taskId = m_reportContext[telegramId]["taskId"].toString();
    QString tracker = m_reportContext[telegramId]["tracker"].toString();

    // 1. Повідомляємо про успіх
    m_telegramClient->sendMessage(telegramId,
                                  QString("✅ Задачу <b>%1</b> успішно взято в роботу!").arg(taskId));

    // 2. !!! УНІФІКАЦІЯ: ВИКЛИКАЄМО showReportMenu !!!
    // isEdit=false, messageId=0 - надсилаємо нове повідомлення
    showReportMenu(telegramId, taskId, tracker, false, 0);
}

/**
 * @brief Помилка призначення задачі на себе.
 * Скидаємо стан.
 */
void Bot::onAssignTaskFailed(const ApiError& error, qint64 telegramId)
{
    if (telegramId == 0) return;

    stopSessionTimeout(telegramId);
    // Скидаємо стан, оскільки процес застопорився
    m_userState.remove(telegramId);
    m_reportContext.remove(telegramId);

    QString errorMessage = QString("❌ Помилка призначення задачі на себе:\n"
                                   "   %1 (HTTP %2). Скидання діалогу.")
                               .arg(error.errorString)
                               .arg(error.httpStatusCode);

    m_telegramClient->sendMessage(telegramId, errorMessage);
}


/**
 * @brief Показує меню звіту (Коментар / Закрити) і фіксує контекст.
 * @param isEdit Якщо true, редагує messageId (для callback'ів).
 */
void Bot::showReportMenu(qint64 chatId, const QString& taskId, const QString& tracker, bool isEdit, qint64 messageId)
{
    // 1. Встановлюємо стан очікування типу звіту (повертаємося до WaitingForTaskSelection)
    // або використовуємо TaskValidated, якщо він існує.
    m_userState[chatId] = UserState::WaitingForTaskSelection;

    // 2. Фіксуємо контекст звіту, якщо ще не зафіксовано/не оновлено
    m_reportContext[chatId]["taskId"] = taskId;
    m_reportContext[chatId]["tracker"] = tracker;

    QJsonObject keyboard;
    QJsonArray rows;

    // Кнопки звітування. Callback-формат: report:type:<type>:<id>
    rows.append(QJsonArray{
        QJsonObject{{"text", "💬 Додати коментар"}, {"callback_data", QString("report:type:comment:%1").arg(taskId)}}
    });

    rows.append(QJsonArray{
        QJsonObject{{"text", "✅ Закрити з рішенням"}, {"callback_data", QString("report:type:close:%1").arg(taskId)}}
    });

    keyboard["inline_keyboard"] = rows;

    QString messageText = QString("<b>Оберіть тип звіту для задачі %1:</b>").arg(taskId);

    if (isEdit) {
        // Редагуємо повідомлення, яке відображало список задач або статус
        m_telegramClient->editMessageText(chatId, messageId, messageText, keyboard, false);
    } else {
        // Надсилаємо нове повідомлення (як після onAssignTaskSuccess)
        m_telegramClient->sendMessageWithInlineKeyboard(chatId, messageText, keyboard);
    }
}

/**
 * @brief Обробляє вибір типу звіту (Коментар / Закрити).
 * Callback-формат: report:type:<type>:<id>
 */
void Bot::handleCallbackReportSelectType(const QJsonObject& query, const QStringList& parts)
{
    qint64 chatId = query["message"].toObject()["chat"].toObject()["id"].toVariant().toLongLong();
    qint64 messageId = query["message"].toObject()["message_id"].toVariant().toLongLong();
    QString queryId = query["id"].toString();

    if (parts.size() < 4) {
        m_telegramClient->answerCallbackQuery(queryId, "Некоректний запит.");
        return;
    }

    QString reportType = parts.at(2); // "comment" або "close"
    QString taskId = parts.at(3); // ID з callback

    // 1. Фіксуємо тип звіту в контексті
    m_reportContext[chatId]["reportType"] = reportType;

    // 2. Встановлюємо стан очікування коментаря
    m_userState[chatId] = UserState::WaitingForComment;
    startSessionTimeout(chatId);

    // 3. Редагуємо повідомлення та просимо ввести коментар
    QString actionName = (reportType == "close") ? "рішення" : "коментаря";
    QString messageText = QString("✍️ Введіть, будь ласка, текст %1 для задачі <b>%2</b>.")
                              .arg(actionName)
                              .arg(taskId);

    // Видаляємо inline клавіатуру, оскільки очікуємо текст
    m_telegramClient->editMessageText(chatId, messageId, messageText, QJsonObject(), false);
    m_telegramClient->answerCallbackQuery(queryId, QString("Обрано %1.").arg(reportType));
}




/**
 * @brief Успішно відправлено звіт (коментар/закриття).
 */
void Bot::onReportTaskSuccess(const QJsonObject& response, qint64 telegramId)
{
    QString taskId = m_reportContext[telegramId]["taskId"].toString();
    QString action = m_reportContext[telegramId]["reportType"].toString();

    QString statusMessage = (action == "close")
                                ? QString("✅ Задача <b>%1</b> успішно закрита!").arg(taskId)
                                : QString("✅ Коментар до задачі <b>%1</b> успішно додано!").arg(taskId);

    m_telegramClient->sendMessage(telegramId, statusMessage);
    stopSessionTimeout(telegramId);
    // Очищаємо контекст та скидаємо стан
    m_userState.remove(telegramId);
    m_reportContext.remove(telegramId);
}

/**
 * @brief Помилка відправки звіту.
 */
void Bot::onReportTaskFailed(const ApiError& error, qint64 telegramId)
{
    QString taskId = m_reportContext[telegramId]["taskId"].toString();

    QString errorMessage = QString("❌ Помилка відправки звіту по задачі <b>%1</b>:\n"
                                   "   %2 (HTTP %3).")
                               .arg(taskId)
                               .arg(error.errorString)
                               .arg(error.httpStatusCode);

    m_telegramClient->sendMessage(telegramId, errorMessage);
    stopSessionTimeout(telegramId);
    // Після помилки очищаємо контекст
    m_userState.remove(telegramId);
    m_reportContext.remove(telegramId);
}


// Bot.cpp (Додайте в кінець файлу або в розділ приватних методів)

/**
 * @brief Запускає 5-хвилинний таймер для сесії користувача.
 */
void Bot::startSessionTimeout(qint64 telegramId)
{
    stopSessionTimeout(telegramId); // Зупиняємо попередній, якщо він був

    QTimer* timer = new QTimer(this);
    // Встановлюємо таймаут на 5 хвилин (300 000 мілісекунд)
    const int TIMEOUT_MS = 300000;
    timer->setInterval(TIMEOUT_MS);
    timer->setSingleShot(true); // Таймер спрацює лише один раз

    // Зберігаємо ID користувача у властивості таймера, щоб ідентифікувати його в слоті
    timer->setProperty("telegramId", telegramId);

    // Підключаємо до нашого загального обробника
    connect(timer, &QTimer::timeout, this, &Bot::handleSessionTimeout);

    m_sessionTimers[telegramId] = timer;
    timer->start();
    logDebug() << "Session timeout started for user:" << telegramId;
}

/**
 * @brief Зупиняє та видаляє таймер сесії користувача.
 */
void Bot::stopSessionTimeout(qint64 telegramId)
{
    if (m_sessionTimers.contains(telegramId)) {
        QTimer* timer = m_sessionTimers.take(telegramId);
        timer->stop();
        timer->deleteLater();
        logDebug() << "Session timeout stopped and deleted for user:" << telegramId;
    }
}

/**
 * @brief Скидає стан користувача, очищує контекст та повідомляє його.
 */
void Bot::resetSession(qint64 telegramId, const QString& reason)
{
    stopSessionTimeout(telegramId);
    m_userState.remove(telegramId);
    m_reportContext.remove(telegramId);

    // Надсилаємо повідомлення користувачеві
    m_telegramClient->sendMessage(telegramId,
                                  QString("❌ Сесія звітування скасована. Причина: <b>%1</b>.").arg(reason));

    logInfo() << "Session reset for user:" << telegramId << "due to:" << reason;
}

/**
 * @brief Слот, який спрацьовує, коли час очікування вичерпано.
 */
void Bot::handleSessionTimeout()
{
    QTimer* timer = qobject_cast<QTimer*>(sender());
    if (!timer) return;

    qint64 telegramId = timer->property("telegramId").toLongLong();

    // Скидаємо сесію
    resetSession(telegramId, "Таймаут неактивності (5 хвилин)");
}


void Bot::handleCallbackReportSearch(const QJsonObject& query, const QStringList& parts)
{
    qint64 chatId = query["message"].toObject()["chat"].toObject()["id"].toVariant().toLongLong();
    QString queryId = query["id"].toString();
    if (parts.size() < 4) return;

    QString searchType = parts.at(3); // "terminal" або "id"

    if (searchType == "terminal") {
        m_userState[chatId] = UserState::WaitingForJiraTerminalID;
        m_telegramClient->sendMessage(chatId, "?? <b>Введіть номер АЗС:</b>");
    }
    else if (searchType == "id") {
        // --- НОВИЙ БЛОК ---
        m_userState[chatId] = UserState::WaitingForJiraTaskId;
        m_telegramClient->sendMessage(chatId, "?? <b>Введіть номер задачі Jira:</b>\n<i>Наприклад: 46937 або AZS-46937</i>");
    }

    m_telegramClient->answerCallbackQuery(queryId);
}
void Bot::showJiraTaskCard(qint64 chatId, const QJsonObject& issue)
{
    QString key = issue["key"].toString();
    QJsonObject fields = issue["fields"].toObject();

    QString summary = fields["summary"].toString();
    QString status = fields["status"].toObject()["name"].toString();
    QString description = fields["description"].toString();

    // --- 1. Визначення Типу запиту (Request Type) ---
    // В Jira Service Desk це зазвичай customfield_10001
    QJsonObject serviceField = fields["customfield_10001"].toObject();
    QString requestType = serviceField["requestType"].toObject()["name"].toString();

    if (requestType.isEmpty()) {
        // Запасний варіант: взяти системне ім'я типу задачі
        requestType = fields["issuetype"].toObject()["name"].toString();
    }

    // --- 2. Логіка визначення АЗС (залишається універсальною) ---
    QString azsDisplay = fields["customfield_15803"].toString();
    if (azsDisplay.isEmpty()) {
        QJsonObject azsObj = fields["customfield_14108"].toObject();
        if (!azsObj.isEmpty()) {
            QString region = azsObj["value"].toString();
            QString number = azsObj["child"].toObject()["value"].toString();
            if (!region.isEmpty() && !number.isEmpty()) {
                azsDisplay = QString("%1 - %2").arg(region, number);
            } else if (!region.isEmpty()) {
                azsDisplay = region;
            }
        }
    }
    if (azsDisplay.isEmpty()) azsDisplay = "<i>не вказано</i>";

    // --- 3. Логіка визначення Ініціатора ---
    QString initiator = fields["customfield_14101"].toString();
    if (initiator.isEmpty()) {
        initiator = fields["reporter"].toObject()["displayName"].toString();
    }

    QString phone = fields["customfield_10301"].toString();

    // Форматування опису
    if (description.isEmpty()) description = "<i>Опис відсутній</i>";
    if (description.length() > 350) description = description.left(347) + "...";

    // Формуємо текст картки з новим полем "Тип запиту"
    QString message = QString(
                          "📄 <b>Задача Jira: %1</b>\n"
                          "━━━━━━━━━━━━━━━━━━\n"
                          "<b>📋 Тип:</b> %2\n"         // Додано Тип запиту
                          "<b>🏪 АЗС:</b> %3\n"
                          "<b>👤 Ініціатор:</b> %4 %5\n"
                          "<b>📝 Тема:</b> %6\n"
                          "<b>⚙️ Статус:</b> %7\n\n"
                          "<b>📖 Опис:</b>\n%8"
                          ).arg(key)
                          .arg(escapeHtml(requestType))   // Вивід типу запиту
                          .arg(escapeHtml(azsDisplay))
                          .arg(escapeHtml(initiator))
                          .arg(phone.isEmpty() ? "" : "(" + phone + ")")
                          .arg(escapeHtml(summary.simplified()))
                          .arg(status)
                          .arg(escapeHtml(description));

    // Клавіатура дій
    QJsonObject keyboard;
    QJsonArray rows;
    rows.append(QJsonArray{
        QJsonObject{{"text", "💬 Коментар"}, {"callback_data", QString("report:action:jira:comment:%1").arg(key)}},
        QJsonObject{{"text", "📸 Фото"}, {"callback_data", QString("report:action:jira:photo:%1").arg(key)}}
    });
    rows.append(QJsonArray{
        QJsonObject{{"text", "✅ Закрити"}, {"callback_data", QString("report:action:jira:close:%1").arg(key)}},
        QJsonObject{{"text", "❌ Відхилити"}, {"callback_data", QString("report:action:jira:reject:%1").arg(key)}}
    });
    keyboard["inline_keyboard"] = rows;

    m_telegramClient->sendMessageWithInlineKeyboard(chatId, message, keyboard);
}


void Bot::handleCallbackReportAction(const QJsonObject& query, const QStringList& parts)
{
    qint64 chatId = query["message"].toObject()["chat"].toObject()["id"].toVariant().toLongLong();
    QString queryId = query["id"].toString();

    // Структура вашої кнопки: report:action:jira:photo:AZS-46937
    if (parts.count() < 5) {
        m_telegramClient->answerCallbackQuery(queryId, "❌ Помилка: недостатньо даних.");
        return;
    }

    QString action = parts.at(3); // photo
    QString taskId = parts.at(4); // AZS-46937

    if (action == "photo") {
        // 1. Фіксуємо задачу в контексті
        m_reportContext[chatId]["taskId"] = taskId;
        m_reportContext[chatId]["tracker"] = "jira";

        // 2. Переводимо бота в стан очікування фото
        m_userState[chatId] = UserState::WaitingForJiraPhoto;

        // 3. Відповідаємо Telegram, щоб прибрати "годинник" на кнопці
        m_telegramClient->answerCallbackQuery(queryId);

        // 4. Просимо користувача надіслати файл
        m_telegramClient->sendMessage(chatId, QString("📸 Будь ласка, надішліть фото для задачі <b>%1</b>").arg(taskId));
    }
}
