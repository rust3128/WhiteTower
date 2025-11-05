#include "TelegramClient.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

TelegramClient::TelegramClient(const QString& token, QObject* parent)
    : QObject(parent), m_token(token)
{
    m_networkManager = new QNetworkAccessManager(this);
    m_pollingTimer = new QTimer(this);
    // Таймер буде викликати слот getUpdates кожні 2 секунди
    connect(m_pollingTimer, &QTimer::timeout, this, &TelegramClient::getUpdates);
}

void TelegramClient::startPolling()
{
    if (!m_pollingTimer->isActive()) {
        getUpdates(); // Робимо перший запит негайно
        m_pollingTimer->start(2000); // Потім запускаємо опитування кожні 2 секунди
    }
}

void TelegramClient::stopPolling()
{
    m_pollingTimer->stop();
}

// Приватний метод для запиту оновлень
void TelegramClient::getUpdates()
{
    QUrl url("https://api.telegram.org/bot" + m_token + "/getUpdates");
    QUrlQuery query;
    query.addQueryItem("offset", QString::number(m_lastUpdateId + 1));
    query.addQueryItem("timeout", "10"); // Час очікування на стороні Telegram
    url.setQuery(query);

    QNetworkRequest request(url);
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &TelegramClient::onUpdatesReplyFinished);
}

void TelegramClient::onUpdatesReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(reply->errorString());
    } else {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj["ok"].toBool()) {
                QJsonArray updates = obj["result"].toArray();
                if (!updates.isEmpty()) {
                    // Оновлюємо ID останнього повідомлення
                    m_lastUpdateId = updates.last().toObject()["update_id"].toVariant().toLongLong();
                    // І сповіщаємо "мозок" про нові події
                    emit updatesReceived(updates);
                }
            }
        }
    }
    reply->deleteLater();
}


/**
 * @brief Надсилає текстове повідомлення користувачу.
 * @param chatId ID чату (зазвичай це ID користувача).
 * @param text Текст повідомлення.
 */
void TelegramClient::sendMessage(qint64 chatId, const QString &text)
{
    QUrl url("https://api.telegram.org/bot" + m_token + "/sendMessage");

    // Створюємо тіло запиту
    QJsonObject jsonBody;
    jsonBody["chat_id"] = chatId;
    jsonBody["text"] = text;
    jsonBody["parse_mode"] = "HTML";

    // Готуємо запит
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Відправляємо POST і відразу "забуваємо" про нього.
    // Нам не потрібно обробляти відповідь на відправлене повідомлення,
    // тому ми не підключаємо жодних слотів.
    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(jsonBody).toJson());

    // Встановлюємо зв'язок, щоб reply видалив себе сам після завершення
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

// ... (в кінець файлу)

/**
 * @brief Надсилає текстове повідомлення з кастомною клавіатурою.
 * @param chatId ID чату (користувача).
 * @param text Текст повідомлення.
 * @param replyMarkup JSON-об'єкт клавіатури (напр., ReplyKeyboardMarkup).
 */
void TelegramClient::sendMessage(qint64 chatId, const QString &text, const QJsonObject &replyMarkup)
{
    QUrl url("https://api.telegram.org/bot" + m_token + "/sendMessage");

    // Створюємо тіло запиту
    QJsonObject jsonBody;
    jsonBody["chat_id"] = chatId;
    jsonBody["text"] = text;
    jsonBody["reply_markup"] = replyMarkup;
    jsonBody["parse_mode"] = "HTML";

    // Готуємо запит
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Відправляємо POST
    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(jsonBody).toJson());

    // Встановлюємо зв'язок, щоб reply видалив себе сам після завершення
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

//

/**
 * @brief (НОВИЙ) Надсилає статус (напр., "typing") в чат.
 * @param chatId ID чату.
 * @param action Статус ("typing", "upload_photo" тощо).
 */
void TelegramClient::sendChatAction(qint64 chatId, const QString &action)
{
    QUrl url("https://api.telegram.org/bot" + m_token + "/sendChatAction");

    QJsonObject jsonBody;
    jsonBody["chat_id"] = chatId;
    jsonBody["action"] = action;

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Це "fire-and-forget" запит, нам не потрібна відповідь
    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(jsonBody).toJson());
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}


//

/**
 * @brief (НОВИЙ/ВІДНОВЛЕНИЙ) Надсилає повідомлення з INLINE-клавіатурою.
 */
void TelegramClient::sendMessageWithInlineKeyboard(qint64 chatId, const QString &text, const QJsonObject &inlineMarkup)
{
    QUrl url("https://api.telegram.org/bot" + m_token + "/sendMessage");

    QJsonObject jsonBody;
    jsonBody["chat_id"] = chatId;
    jsonBody["text"] = text;
    jsonBody["reply_markup"] = inlineMarkup; // <
    jsonBody["parse_mode"] = "HTML";

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // "fire-and-forget"
    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(jsonBody).toJson());
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

/**
 * @brief (НОВИЙ/ВІДНОВЛЕНИЙ) Надсилає відповідь на натискання inline-кнопки.
 * Це необхідно, щоб у клієнта Telegram зник значок завантаження ("годинник").
 */
void TelegramClient::answerCallbackQuery(const QString &callbackQueryId, const QString &text)
{
    QUrl url("https://api.telegram.org/bot" + m_token + "/answerCallbackQuery");

    QJsonObject jsonBody;
    jsonBody["callback_query_id"] = callbackQueryId;
    if (!text.isEmpty()) {
        jsonBody["text"] = text;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // "fire-and-forget"
    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(jsonBody).toJson());
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

/**
 * @brief (НОВИЙ) Редагує існуюче текстове повідомлення (змінює текст та/або кнопки).
 * Це ключовий метод для "безшовної" навігації по меню.
 * @param chatId ID чату.
 * @param messageId ID повідомлення, яке треба редагувати.
 * @param text Новий текст.
 * @param inlineMarkup Новий JSON кнопок.
 */
void TelegramClient::editMessageText(qint64 chatId, int messageId, const QString &text, const QJsonObject &inlineMarkup)
{
    QUrl url("https://api.telegram.org/bot" + m_token + "/editMessageText");

    QJsonObject jsonBody;
    jsonBody["chat_id"] = chatId;
    jsonBody["message_id"] = messageId;
    jsonBody["text"] = text;
    jsonBody["reply_markup"] = inlineMarkup;
    jsonBody["parse_mode"] = "HTML";

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(jsonBody).toJson());

    // Ми не будемо обробляти помилки редагування (напр., якщо повідомлення застаріле),
    // просто видалимо відповідь.
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}
