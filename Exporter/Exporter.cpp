#include "Exporter.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug> // Використовуємо qDebug() для консолі
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDateTime>
#include <QProcess> // Для Zipping

// Підключаємо логер (якщо він у бібліотеці 'Oracle')
// #include "Oracle/Logger.h"

Exporter::Exporter(const QString& configPath, QObject *parent)
    : QObject(parent), m_configPath(configPath)
{
}

bool Exporter::loadConfig()
{
    QFile configFile(m_configPath);
    if (!configFile.open(QIODevice::ReadOnly)) {
        qCritical() << "Failed to open config file:" << m_configPath;
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll());
    if (!doc.isObject()) {
        qCritical() << "Config file is not a valid JSON object.";
        return false;
    }

    m_config = doc.object();
    qInfo() << "Config loaded successfully.";
    return true;
}

bool Exporter::run()
{
    if (!loadConfig()) {
        return false;
    }

    QJsonArray tasks = m_config["tasks"].toArray();
    if (tasks.isEmpty()) {
        qWarning() << "No tasks found in config file.";
        return false;
    }

    // 1. Виконуємо всі завдання
    for (const QJsonValue& taskVal : tasks) {
        if (!runTask(taskVal.toObject())) {
            // Якщо одне завдання не вдалося, ми можемо зупинитися
            // або продовжити (на ваш вибір)
            qCritical() << "Failed to run task:" << taskVal.toObject()["task_name"].toString();
            // return false;
        }
    }

    // 2. Збираємо ZIP-архів
    QString archiveName = m_config["output_package_name"].toString("import_package.zip");
    if (!zipResults(m_generatedFiles, archiveName)) {
        qCritical() << "Failed to create ZIP package.";
        return false;
    }

    qInfo() << "Successfully created package:" << archiveName;
    return true;
}

//
// ПОВНІСТЮ ЗАМІНІТЬ ЦЕЙ МЕТОД

bool Exporter::runTask(const QJsonObject& taskConfig)
{
    QString taskName = taskConfig["task_name"].toString();
    QString queryFile = taskConfig["query_file"].toString();
    QString outputFile = taskConfig["output_file"].toString();
    int clientId = taskConfig["embed_client_id"].toInt();
    QJsonObject params = taskConfig["params"].toObject();

    qInfo() << "Running task:" << taskName;

    // --- 1. Читаємо SQL-запит ---
    QFile qf(queryFile);
    if (!qf.open(QIODevice::ReadOnly)) {
        qCritical() << "Failed to read query file:" << queryFile;
        return false;
    }
    QString sql = qf.readAll();
    qf.close();

    if (sql.isEmpty()) {
        qCritical() << "Query file is empty:" << queryFile;
        return false;
    }

    // --- 2. Налаштовуємо НОВЕ з'єднання з БД ---
    QSqlDatabase db = QSqlDatabase::addDatabase("QIBASE", "exporter_task_" + taskName);
    QJsonObject dbConfig = m_config["source_db"].toObject();
    db.setHostName(dbConfig["host"].toString());
    db.setPort(dbConfig["port"].toInt());
    db.setDatabaseName(dbConfig["path"].toString());
    db.setUserName(dbConfig["user"].toString());
    db.setPassword(dbConfig["password"].toString());
    db.setConnectOptions("ISC_DPB_LC_CTYPE=UTF8");

    if (!db.open()) {
        qCritical() << "Failed to connect to client database:" << db.lastError().text();
        QSqlDatabase::removeDatabase("exporter_task_" + taskName);
        return false;
    }

    // --- 3. Виконуємо запит (вкладений блок для 'query') ---
    QJsonArray data;
    bool querySuccess = false;

    { // Початок блоку, щоб 'query' був знищений до 'removeDatabase'
        QSqlQuery query(db);

        // Перевіряємо, чи підготовка пройшла успішно
        if (!query.prepare(sql)) {
            qCritical() << "Failed to PREPARE query for task" << taskName << ":" << query.lastError().text();
            qCritical() << "Query text:" << sql;
            // querySuccess залишається false
        } else {

            // --- ОНОВЛЕНА ЛОГІКА BINDING ---
            // Ми переходимо на позиційні '?'
            // Ми припускаємо, що нам потрібні 'minTermId' та 'maxTermId' з конфігу

            QJsonValue minVal = params["minTermId"];
            QJsonValue maxVal = params["maxTermId"];

            if(minVal.isUndefined() || maxVal.isUndefined()) {
                qCritical() << "Task" << taskName << "is missing minTermId or maxTermId in config params.";
                querySuccess = false;
            } else {
                // Додаємо значення у тому порядку, в якому '?' з'являються в SQL
                query.addBindValue(minVal.toVariant());
                query.addBindValue(maxVal.toVariant());

                if (query.exec()) {
                    data = convertQueryToJson(query);
                    querySuccess = true;
                } else {
                    qCritical() << "Failed to EXECUTE query for task" << taskName << ":" << query.lastError().text();
                    // querySuccess залишається false
                }
            }
        }
    } // Кінець блоку, 'query' тут знищується

    // --- 4. Завжди закриваємо і видаляємо з'єднання ---
    db.close();
    QSqlDatabase::removeDatabase("exporter_task_" + taskName);

    // --- 5. Перевіряємо, чи був запит успішним ---
    if (!querySuccess) {
        return false; // Помилку вже залоговано
    }

    // --- 6. Створюємо JSON-обгортку ---
    QJsonObject outputJson;
    outputJson["client_id"] = clientId;
    outputJson["task_name"] = taskName;
    outputJson["export_date"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    outputJson["data"] = data;

    // --- 7. Зберігаємо файл ---
    QFile outFile(outputFile);
    if (!outFile.open(QIODevice::WriteOnly)) {
        qCritical() << "Failed to write output file:" << outputFile;
        return false;
    }
    outFile.write(QJsonDocument(outputJson).toJson(QJsonDocument::Indented));
    outFile.close();

    qInfo() << "Task" << taskName << "completed. Found" << data.count() << "records. Saved to" << outputFile;
    m_generatedFiles.append(outputFile);
    return true;
}

/**
 * @brief "Тупий" конвертер.
 * Проходить по результату запиту і перетворює його в JSON-масив.
 */
QJsonArray Exporter::convertQueryToJson(QSqlQuery& query)
{
    QJsonArray recordsArray;
    QSqlRecord record = query.record();

    while (query.next()) {
        QJsonObject recordObject;
        for (int i = 0; i < record.count(); ++i) {
            QString fieldName = record.fieldName(i);
            // QVariant::toJsonValue() чудово обробляє більшість типів
            recordObject[fieldName] = QJsonValue::fromVariant(query.value(i));
        }
        recordsArray.append(recordObject);
    }
    query.finish();
    return recordsArray;
}

/**
 * @brief Використовує QProcess для виклику 7-Zip для створення архіву.
 */
bool Exporter::zipResults(const QStringList& filesToZip, const QString& archiveName)
{
    if (filesToZip.isEmpty()) {
        qWarning() << "No files to zip.";
        return true;
    }

    // Переконуємося, що старого архіву немає
    if (QFile::exists(archiveName)) {
        QFile::remove(archiveName);
    }

    // Ми припускаємо, що 7z.exe знаходиться в PATH або поруч з Exporter.exe
    QStringList args;
    args << "a"           // Команда "add"
         << "-tzip"       // Тип архіву - zip
         << archiveName;  // Ім'я архіву

    args.append(filesToZip); // Додаємо всі файли

    qInfo() << "Running 7-Zip command:" << "7z" << args.join(" ");

    QProcess zipper;
    zipper.start("7z", args);

    if (!zipper.waitForFinished()) {
        qCritical() << "7-Zip process failed to start or timed out.";
        qCritical() << zipper.errorString();
        return false;
    }

    if (zipper.exitCode() != 0) {
        qCritical() << "7-Zip failed with exit code" << zipper.exitCode();
        qCritical() << "7-Zip output:" << zipper.readAllStandardOutput();
        qCritical() << "7-Zip error:" << zipper.readAllStandardError();
        return false;
    }

    qInfo() << "ZIP archive created successfully.";

    // (Опціонально) Видаляємо .json файли після пакування
    // for(const QString& file : filesToZip) {
    //    QFile::remove(file);
    // }

    return true;
}
