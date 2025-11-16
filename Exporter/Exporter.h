#ifndef EXPORTER_H
#define EXPORTER_H

#include <QObject>
#include <QJsonObject>
#include <QStringList>
#include <QSqlQuery>


class Exporter : public QObject
{
    Q_OBJECT
public:
    // Тепер приймає шлях до ZIP-архіву, який знаходиться в INBOX
    explicit Exporter(const QString& packagePath, QObject *parent = nullptr);

    bool run();

private:
    // Розпаковує архів з m_packagePath у m_workDir (яка буде INBOX)
    bool unpackArchive();

    // loadConfig тепер шукає файл у m_workDir
    bool loadConfig();

    bool runTask(const QJsonObject& taskConfig);
    bool zipResults(const QStringList& filesToZip, const QString& archiveName);

    QJsonArray convertQueryToJson(QSqlQuery& query);

    QString m_packagePath;      // Шлях до вхідного ZIP-файлу (наприклад, D:/Exporter/Inbox/6_package.zip)
    QString m_workDir;          // Робоча директорія (де розпаковуємо: D:/Exporter/Inbox/)
    QString m_outputDir;        // Директорія для результатів (D:/Exporter/Outbox/)
    QJsonObject m_config;
    QStringList m_generatedFiles;
};

#endif // EXPORTER_H
