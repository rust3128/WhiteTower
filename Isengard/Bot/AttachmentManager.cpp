#include "AttachmentManager.h"
#include <QNetworkRequest>
#include <QDebug>

AttachmentManager::AttachmentManager(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)) {}

QString AttachmentManager::prepareStoragePath(const QString &baseRoot, User *user, const QString &taskId) {
    if (!user || baseRoot.isEmpty()) return QString();


    // 1. Формуємо шлях: LOGIN / YYYYMMDD / TASKID
    QString dateFolder = QDate::currentDate().toString("yyyyMMdd");

    // Використовуємо QDir::separator() або просто "/" для кросплатформеності
    QString relativePath = QString("%1/%2/%3")
                               .arg(user->login())
                               .arg(dateFolder)
                               .arg(taskId);

    // Нормалізуємо базовий шлях (змінюємо \ на /)
    QString cleanRoot = QDir::fromNativeSeparators(baseRoot);
    QDir rootDir(cleanRoot);
    QString fullPath = rootDir.absoluteFilePath(relativePath);

    // 2. Створюємо директорії
    if (!QDir().exists(fullPath)) {
        if (!QDir().mkpath(fullPath)) {
            qCritical() << "Failed to create directory path:" << fullPath;
            return QString();
        }
    }

    return fullPath;
}

void AttachmentManager::downloadFile(const QUrl &tgUrl, const QString &fullPath, qint64 telegramId, const QString &taskId) {
    QNetworkRequest request(tgUrl);
    request.setAttribute(QNetworkRequest::Attribute(QNetworkRequest::User + 1), "IsengardBot");

    QNetworkReply *reply = m_networkManager->get(request);

    // ЗБЕРІГАЄМО КОНТЕКСТ У ВЛАСТИВОСТЯХ REPLY
    reply->setProperty("destPath", fullPath);
    reply->setProperty("telegramId", telegramId);
    reply->setProperty("taskId", taskId);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onDownloadFinished(reply);
    });
}
void AttachmentManager::onDownloadFinished(QNetworkReply *reply) {
    reply->deleteLater();

    // ВИТЯГУЄМО КОНТЕКСТ
    qint64 telegramId = reply->property("telegramId").toLongLong();
    QString taskId = reply->property("taskId").toString();
    QString destPath = reply->property("destPath").toString();

    if (reply->error() != QNetworkReply::NoError) {
        // Передаємо ID, щоб Бот знав, кому писати про помилку
        emit downloadError(reply->errorString(), telegramId);
        return;
    }

    QFile file(destPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(reply->readAll());
        file.close();
        qInfo() << "File saved locally:" << destPath;

        // ЕМІТИМО СИГНАЛ З УСІМА ДАНИМИ
        emit fileDownloaded(destPath, telegramId, taskId);
    } else {
        emit downloadError("Could not open file for writing: " + destPath, telegramId);
    }
}
