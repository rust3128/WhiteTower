#ifndef ATTACHMENTMANAGER_H
#define ATTACHMENTMANAGER_H

#include <QObject>
#include <QString>
#include <QDir>
#include <QDate>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include "Oracle/User.h" // Для методу login()

class AttachmentManager : public QObject {
    Q_OBJECT
public:
    explicit AttachmentManager(QObject *parent = nullptr);

    // Створює ієрархію папок LOGIN/YYYYMMDD/TASKID та повертає шлях
    QString prepareStoragePath(const QString &baseRoot, User *user, const QString &taskId);

    // Запускає завантаження файлу з Telegram
    void downloadFile(const QUrl &tgUrl, const QString &fullPath);



signals:
    void fileDownloaded(const QString &localPath);
    void downloadError(const QString &error);

private slots:
    void onDownloadFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_networkManager;
};

#endif
