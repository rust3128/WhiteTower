#ifndef TELEGRAMCLIENT_H
#define TELEGRAMCLIENT_H

#include <QObject>
#include <QString>
#include <QJsonArray>

class QNetworkAccessManager;
class QTimer;

class TelegramClient : public QObject
{
    Q_OBJECT
public:
    explicit TelegramClient(const QString& token, QObject* parent = nullptr);

    void startPolling();
    void stopPolling();

    void sendMessage(qint64 chatId, const QString& text);
    void sendMessage(qint64 chatId, const QString& text, const QJsonObject& replyMarkup);

    void sendChatAction(qint64 chatId, const QString& action = "typing");

    /**
     * @brief Надсилає повідомлення з кнопками ПІД ним (inline keyboard).
     */
    void sendMessageWithInlineKeyboard(qint64 chatId, const QString& text, const QJsonObject& inlineMarkup);

    /**
     * @brief "Відповідає" на натискання кнопки, щоб зняти "годинник".
     */
    void answerCallbackQuery(const QString& callbackQueryId, const QString& text = "");

    /**
     * @brief (НОВИЙ) Редагує текст та кнопки існуючого повідомлення.
     */
    void editMessageText(qint64 chatId, int messageId, const QString& text, const QJsonObject& inlineMarkup);

    /**
     * @brief Надсилає текстове повідомлення з опцією вимкнення веб-прев'ю.
     */
    void sendMessage(qint64 chatId, const QString& text, bool disablePreview);

    /**
     * @brief Редагує існуюче повідомлення з опцією вимкнення веб-прев'ю.
     * (Це потрібно, щоб уникнути появи прев'ю при редагуванні, якщо ми додамо URL)
     */
    void editMessageText(qint64 chatId, int messageId, const QString& text, const QJsonObject& inlineMarkup, bool disablePreview);

    void sendLocation(qint64 chatId, double latitude, double longitude);

signals:
    void updatesReceived(const QJsonArray& updates);
    void errorOccurred(const QString& error);

private slots:
    void getUpdates();
    void onUpdatesReplyFinished();

private:
    QString m_token;
    qint64 m_lastUpdateId = 0;
    QNetworkAccessManager* m_networkManager;
    QTimer* m_pollingTimer;
};

#endif // TELEGRAMCLIENT_H
