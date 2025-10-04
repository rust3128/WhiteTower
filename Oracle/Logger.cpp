#include "Logger.h"
#include "AppParams.h"
#include <QCoreApplication> // Потрібен для шляху до додатку
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo> // Потрібен для шляху до додатку
#include <QTextStream>
#include <QRegularExpression>
#include <iostream>

Q_LOGGING_CATEGORY(logApp, "logApp")

// Глобальні (статичні) змінні для нашого файлу,
// щоб не передавати їх у кожну функцію.
static QString g_logFilePrefix;     // Наприклад, "Conduit"
static QString g_logDirectoryPath;  // Повний шлях до теки Logs

static void cleanupOldLogs(int daysToKeep)
{
    logInfo() << QString("Starting cleanup of logs older than %1 days...").arg(daysToKeep);
    QDir logDir(g_logDirectoryPath); // Використовуємо глобальний шлях
    if (!logDir.exists()) {
        logWarning() << "Logs directory does not exist. Nothing to clean.";
        return;
    }

    // Шаблон і фільтр тепер динамічні, на основі імені додатку
    const QString logFilePattern = g_logFilePrefix + "_(\\d{8})\\.log";
    const QRegularExpression re(logFilePattern);
    const QStringList logFiles = logDir.entryList(QStringList() << g_logFilePrefix + "_*.log", QDir::Files);

    for (const QString &fileName : logFiles) {
        QRegularExpressionMatch match = re.match(fileName);
        if (match.hasMatch()) {
            QString dateString = match.captured(1);
            QDate logDate = QDate::fromString(dateString, "yyyyMMdd");
            if (logDate.isValid() && logDate.daysTo(QDate::currentDate()) > daysToKeep) {
                QString fullPath = logDir.filePath(fileName);
                QFile::remove(fullPath);
                logInfo() << "Removed old log file:" << fileName;
            }
        }
    }
}

static void customLogMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // 1. --- Логіка щоденної ротації файлів ---
    static QFile logFile;
    static QDate logDate;

    QDate currentDate = QDate::currentDate();
    if (logDate != currentDate || !logFile.isOpen()) {
        if (logFile.isOpen()) {
            logFile.close();
        }

        logDate = currentDate;
        const QString fileName = QString("%1_%2.log").arg(g_logFilePrefix, currentDate.toString("yyyyMMdd"));

        QDir logDir(g_logDirectoryPath);
        if (!logDir.exists()) {
            QDir().mkpath(g_logDirectoryPath);
        }

        const QString fullFilePath = logDir.filePath(fileName);
        logFile.setFileName(fullFilePath);

        if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QFileInfo fileInfo(logFile);
            std::cerr << "--- LOGGER CRITICAL ERROR ---" << std::endl;
            std::cerr << "Failed to open log file for writing!" << std::endl;
            std::cerr << "Attempted Path: " << qPrintable(fileInfo.absoluteFilePath()) << std::endl;
            std::cerr << "Error Details: " << qPrintable(logFile.errorString()) << std::endl;
            std::cerr << "-----------------------------" << std::endl;
            return;
        }
    }

    // 2. --- Форматування повідомлення ---
    const QString time = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString level;
    switch (type) {
    case QtDebugMsg:    level = "DEBG"; break;
    case QtInfoMsg:     level = "INFO"; break;
    case QtWarningMsg:  level = "WARN"; break;
    case QtCriticalMsg: level = "CRIT"; break;
    case QtFatalMsg:    level = "FATL"; break;
    }

    QString formattedMessage;

#ifndef QT_NO_DEBUG
    {
        // === ВИПРАВЛЕНО ТУТ ===
        // Зберігаємо ім'я файлу в змінну типу QString, а не const char*
        QString file = QFileInfo(context.file).fileName();

        QString logContext = QString("[%1@%2:%3]")
                                 .arg(context.function)
                                 .arg(file)
                                 .arg(context.line);
        formattedMessage = QString("%1 | %2 | %3 %4\n").arg(time, level, msg, logContext);
    }
#else
    {
        formattedMessage = QString("%1 | %2 | %3\n").arg(time, level, msg);
    }
#endif

    // 3. --- Запис у файл (завжди) ---
    QTextStream out(&logFile);
    out << formattedMessage;
    out.flush();

    // 4. --- Дублювання в консоль (тільки для Debug) ---
#ifndef QT_NO_DEBUG
    if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
        std::cerr << qPrintable(formattedMessage);
    } else {
        std::cout << qPrintable(formattedMessage);
    }
#endif
}

void initLogger(const QString& appName)
{
    qInstallMessageHandler(customLogMessageHandler);

    // --- ВИКОРИСТОВУЄМО ПАРАМЕТРИ З БД ---
    AppParams& params = AppParams::instance();

    // Отримуємо LogLevel. Якщо в БД його немає, за замовчуванням буде "INFO"
    QString logLevel = params.getParam(appName, "LogLevel", "INFO").toString().toUpper();

    // Отримуємо LogRetentionDays. Якщо немає, за замовчуванням буде 7
    int logRetentionDays = params.getParam(appName, "LogRetentionDays", 7).toInt();
    // ------------------------------------

    // Встановлюємо правило фільтрації на основі значення з БД
    // Наприклад, "logApp.DEBUG=true" або "logApp.INFO=true"
    QString filterRule = QString("logApp.%1=true").arg(logLevel);
    QLoggingCategory::setFilterRules(filterRule);

    cleanupOldLogs(logRetentionDays);

    logInfo() << "Logger initialized for" << appName
              << ". Log Level:" << logLevel
              << ". Log retention:" << logRetentionDays << "days.";
}
// pvayd dkdfksf
