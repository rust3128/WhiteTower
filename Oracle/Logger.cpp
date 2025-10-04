#include "Logger.h"
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
    Q_UNUSED(context);

    static QFile logFile;
    static QDate logDate;

    QDate currentDate = QDate::currentDate();
    if (logDate != currentDate || !logFile.isOpen()) {
        if (logFile.isOpen()) {
            logFile.close();
        }

        logDate = currentDate;
        // Ім'я файлу тепер динамічне
        const QString fileName = QString("%1_%2.log").arg(g_logFilePrefix, currentDate.toString("yyyyMMdd"));

        // Використовуємо глобальний шлях
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

    const QString time = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString level;
    switch (type) {
    case QtDebugMsg:    level = "DEBG"; break;
    case QtInfoMsg:     level = "INFO"; break;
    case QtWarningMsg:  level = "WARN"; break;
    case QtCriticalMsg: level = "CRIT"; break;
    case QtFatalMsg:    level = "FATL"; break;
    }

    const QString formattedMessage = QString("%1 | %2 | %3\n").arg(time, level, msg);

    QTextStream out(&logFile);
    out << formattedMessage;
    out.flush();

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
    // 1. Визначаємо і зберігаємо глобальні змінні
    g_logFilePrefix = appName;
    // Надійно отримуємо шлях до теки з .exe і додаємо /Logs
    g_logDirectoryPath = QCoreApplication::applicationDirPath() + "/Logs";

    qInstallMessageHandler(customLogMessageHandler);
    QLoggingCategory::setFilterRules(QStringLiteral("logApp.debug=true"));
    const int logRetentionDays = 7;
    cleanupOldLogs(logRetentionDays);
    logInfo() << "Logger initialized for" << appName << ". Log retention period:" << logRetentionDays << "days.";
}
