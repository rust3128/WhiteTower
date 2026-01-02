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

void AttachmentManager::downloadFile(const QUrl &tgUrl, const QString &fullPath) {
    QNetworkRequest request(tgUrl);
    // Додаємо ідентифікацію бота для запиту
    request.setAttribute(QNetworkRequest::Attribute(QNetworkRequest::User + 1), "IsengardBot");

    QNetworkReply *reply = m_networkManager->get(request);
    reply->setProperty("destPath", fullPath);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
       onDownloadFinished(reply); // Використовуйте іменування, що відповідає слоту
    });
}

void AttachmentManager::onDownloadFinished(QNetworkReply *reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit downloadError(reply->errorString());
        return;
    }

    QString destPath = reply->property("destPath").toString();
    QFile file(destPath);

    if (file.open(QIODevice::WriteOnly)) {
        file.write(reply->readAll());
        file.close();
        qInfo() << "File successfully saved to:" << destPath;
        emit fileDownloaded(destPath);
    } else {
        emit downloadError("Could not open file for writing: " + destPath);
    }
}
