#include "Exporter.h"
#include "Oracle/criptpass.h"
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
#include <QDir>
#include <QCoreApplication>

// Підключаємо логер (якщо він у бібліотеці 'Oracle')
// #include "Oracle/Logger.h"

Exporter::Exporter(const QString& packagePath, QObject *parent)
    : QObject(parent), m_packagePath(packagePath)
{
    // Встановлюємо робочі директорії
    QString baseDir = QCoreApplication::applicationDirPath();

    // Робоча папка: де лежить вхідний ZIP-файл. Ми розпакуємо поруч.
    m_workDir = QFileInfo(m_packagePath).absolutePath();

    // Папка для вихідних даних
    m_outputDir = baseDir + "/Outbox";

    qInfo() << "Output directory set to:" << m_outputDir;
}

bool Exporter::unpackArchive()
{
    qInfo() << "Unpacking archive to working directory:" << m_workDir;

    // Команда для розпакування: 7z x <archive_path> -o<output_dir>
    QProcess unzipper;
    unzipper.setWorkingDirectory(m_workDir); // Важливо: 7z буде створювати файли тут

    QStringList args;
    args << "x"           // Команда "extract"
         << m_packagePath // Шлях до ZIP-файлу
         << QString("-o%1").arg(m_workDir) // Шлях до робочої папки (де лежить ZIP)
         << "-y";         // Придушити запити "yes/no"

    qInfo() << "Running 7-Zip extraction:" << "7z" << args.join(" ");
    unzipper.start("7z", args);

    if (!unzipper.waitForFinished(60000)) {
        qCritical() << "7-Zip extraction timed out.";
        return false;
    }

    if (unzipper.exitCode() != 0) {
        qCritical() << "7-Zip extraction failed. Error output:" << unzipper.readAllStandardError();
        return false;
    }

    qInfo() << "Archive unpacked successfully.";
    return true;
}

bool Exporter::loadConfig()
{
    QFileInfo fileInfo(m_packagePath);
    // Знаходимо ID клієнта з імені ZIP-файлу
    QString idString = fileInfo.baseName().split("_").first();
    QString configFilePath = m_workDir + "/" + idString + "_config.json";

    if (!QFile::exists(configFilePath)) {
        qCritical() << "Config file not found after unpacking:" << configFilePath;
        return false;
    }

    QFile configFile(configFilePath);
    if (!configFile.open(QIODevice::ReadOnly)) {
        qCritical() << "Failed to open config file:" << configFilePath;
        return false;
    }

    // ... (решта логіки loadConfig без змін) ...
    QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll());
    configFile.close(); // Закриваємо файл

    if (!doc.isObject()) {
        qCritical() << "Config file is not a valid JSON object.";
        return false;
    }

    m_config = doc.object();
    qInfo() << "Config loaded successfully from" << configFilePath;
    return true;
}

bool Exporter::run()
{
    QFileInfo fileInfo(m_packagePath);

    // 1. Розпаковуємо пакет
    if (!unpackArchive()) {
        return false;
    }

    // 2. Змінюємо робочу директорію на ту, куди розпакували,
    //    щоб runTask міг знайти .sql файли та зберегти .json.
    QDir::setCurrent(m_workDir);
    qInfo() << "Working directory set to:" << m_workDir;

    // 3. Завантажуємо конфіг
    if (!loadConfig()) {
        return false;
    }

    // 4. Ініціалізація DB-з'єднання
    QJsonObject sourceDbConfig = m_config["source_db"].toObject();

    QString encryptedPass = sourceDbConfig["password"].toString();
    QString decryptedPass = CriptPass::instance().decriptPass(encryptedPass); // Ваше коректне виправлення

    QSqlDatabase db = QSqlDatabase::addDatabase("QIBASE", "exporter_connection");
    db.setHostName(sourceDbConfig["host"].toString());
    db.setPort(sourceDbConfig["port"].toInt());
    db.setDatabaseName(sourceDbConfig["path"].toString());
    db.setUserName(sourceDbConfig["user"].toString());
    db.setPassword(decryptedPass);

    if (!db.open()) {
        qCritical() << "Failed to connect to source DB:" << db.lastError().text();
        QSqlDatabase::removeDatabase("exporter_connection");
        return false;
    }
    qInfo() << "Successfully connected to source DB.";

    // 5. Виконання завдань
    bool allTasksSuccess = true;
    QJsonArray tasks = m_config["tasks"].toArray();

    // Список m_generatedFiles заповнюється у runTask()
    for (const QJsonValue& value : tasks) {
        if (!runTask(value.toObject())) {
            allTasksSuccess = false;
        }
    }

    // 6. Створення ZIP-архіву у папці Outbox (якщо все пройшло успішно)
    if (allTasksSuccess) {
        // Ім'я архіву: 6_import_package.zip
        QString zipName = fileInfo.fileName();
        QString resultZipName = QString("RESULT_") + zipName;
        QString finalZipPath = m_outputDir + "/" + resultZipName;

        // Перевіряємо та створюємо Outbox, якщо вона ще не створена
        if (!QDir().mkpath(m_outputDir)) {
            qCritical() << "Failed to create output directory:" << m_outputDir;
            allTasksSuccess = false;
        } else {
            // zipResults буде викликаний у робочій директорії (m_workDir)
            // Нам потрібно запакувати лише файли-результати (.json)
            if (!zipResults(m_generatedFiles, finalZipPath)) {
                allTasksSuccess = false;
            }
        }
    }

    // 7. Закриття та очищення БД
    db.close();
    QSqlDatabase::removeDatabase("exporter_connection");

    // 8. !!! ФІНАЛЬНЕ ОЧИЩЕННЯ INBOX (ЗАЛИШАЄМО ЛИШЕ ВХІДНИЙ ZIP) !!!
    if (allTasksSuccess) {
        QDir workDir(m_workDir);

        // Встановлюємо фільтр для видалення ВСЬОГО, крім ZIP-архівів.
        // Оскільки в Inbox може бути кілька ZIP-файлів, ми видаляємо тільки розпаковані файли.

        // Список усіх розпакованих файлів (JSON/SQL/Config JSON)
        workDir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
        workDir.setNameFilters(QStringList() << "*.json" << "*.sql");

        qInfo() << "Starting cleanup of Inbox (work directory)...";

        for (const QString& fileName : workDir.entryList()) {
            QString fullPath = workDir.absoluteFilePath(fileName);
            // Перевіряємо, чи це не вхідний ZIP, хоча фільтр NameFilters цього не охоплює
            if (fileName.toLower() != fileInfo.fileName().toLower()) {
                if (QFile::remove(fullPath)) {
                    qDebug() << "Removed intermediate file:" << fileName;
                } else {
                    qWarning() << "Could not remove file:" << fullPath;
                }
            }
        }

        // Фактично, ми видаляємо всі *.json та *.sql, які були розпаковані.
        // Вхідний ZIP-файл залишається.
    }

    return allTasksSuccess;
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
    QString decryptedPass = CriptPass::instance().decriptPass(dbConfig["password"].toString());
    db.setPassword(decryptedPass);
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
