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
    explicit Exporter(const QString& configPath, QObject *parent = nullptr);

    // Головний метод, що запускає всі завдання
    bool run();

private:
    bool loadConfig();
    bool runTask(const QJsonObject& taskConfig);
    bool zipResults(const QStringList& filesToZip, const QString& archiveName);

    // Допоміжна функція для "тупого" перетворення
    QJsonArray convertQueryToJson(QSqlQuery& query);

    QString m_configPath;
    QJsonObject m_config;
    QStringList m_generatedFiles; // Список файлів для ZIP-архіву
};

#endif // EXPORTER_H
