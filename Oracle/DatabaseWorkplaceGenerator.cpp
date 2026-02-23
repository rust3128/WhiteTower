#include "DatabaseWorkplaceGenerator.h"
#include "Logger.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

QJsonArray DatabaseWorkplaceGenerator::generate(int clientId, int objectId, int terminalId)
{
    QJsonArray workplacesArray;

    logInfo() << "DatabaseWorkplaceGenerator: Fetching workplaces for Object:" << objectId << "Terminal:" << terminalId;

    QSqlQuery query;
    // Шукаємо всі робочі місця, прив'язані до цього об'єкта
    query.prepare("SELECT WORKPLACE_ID, VERSION_TYPE, POS_ID, IPADR, PASSVNC, PORTVNC "
                  "FROM WORKPLACES "
                  "WHERE OBJECT_ID = :objId");
    query.bindValue(":objId", objectId);

    if (!query.exec()) {
        qCritical() << "DatabaseWorkplaceGenerator SQL Error:" << query.lastError().text();
        return workplacesArray; // Повертаємо порожній масив у разі помилки БД
    }

    // Проходимось по всіх знайдених записах
    while (query.next()) {
        QJsonObject wp;

        wp["workplace_id"] = query.value("WORKPLACE_ID").toInt();
        wp["version_type"] = query.value("VERSION_TYPE").toInt();
        wp["pos_id"]       = query.value("POS_ID").toInt();
        wp["ip_address"]   = query.value("IPADR").toString();
        wp["vnc_port"]     = query.value("PORTVNC").toInt();

        // Зберігаємо terminal_id, щоб UI (Gandalf) розумів, до якої АЗС це належить
        wp["terminal_id"]  = terminalId;

        // --- ЛОГІКА РОЗШИФРОВКИ ---
        QString encryptedPass = query.value("PASSVNC").toString();

        // ЗАМІНІТЬ ЦЕЙ РЯДОК НА РЕАЛЬНИЙ ВИКЛИК ВАШОГО КЛАСУ CriptPass:
        // CriptPass crypt;
        // QString decryptedPass = crypt.decrypt(encryptedPass);

        QString decryptedPass = encryptedPass; // Поки що просто копіюємо як є

        wp["vnc_password"] = decryptedPass;

        // Додаємо готову картку в масив
        workplacesArray.append(wp);
    }

    qInfo() << "DatabaseWorkplaceGenerator: Found" << workplacesArray.size() << "workplaces.";

    return workplacesArray;
}
