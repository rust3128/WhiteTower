#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QStringList> // Додано
#include "Exporter.h"
#include <QDebug> // Використовуємо qDebug/qInfo для логування

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // 1. Визначаємо базові шляхи
    QString baseDir = QCoreApplication::applicationDirPath();
    QString inboxDir = baseDir + "/Inbox";
    QString outboxDir = baseDir + "/Outbox";

    // 2. Створюємо Inbox та Outbox (якщо їх немає)
    QDir().mkpath(inboxDir);
    QDir().mkpath(outboxDir);

    qInfo() << "Exporter started. Checking Inbox for package...";

    // 3. Скануємо Inbox на наявність ZIP-файлів
    QDir inbox(inboxDir);
    inbox.setFilter(QDir::Files | QDir::NoSymLinks); // Тільки файли
    inbox.setNameFilters(QStringList() << "*.zip");  // Тільки ZIP-архіви

    QStringList zipFiles = inbox.entryList();

    if (zipFiles.isEmpty()) {
        qInfo() << "Exporter finished: No ZIP package found in Inbox. Exiting successfully.";
        return 0; // Завершення без помилки (немає роботи)
    }

    // 4. Беремо перший знайдений файл
    QString packageFileName = zipFiles.first();
    QString packagePath = inboxDir + "/" + packageFileName; // Повний абсолютний шлях

    qInfo() << "Found package:" << packagePath;

    // 5. Ініціалізація та запуск Exporter
    Exporter exporter(packagePath);

    bool success = exporter.run();

    if (success) {
        // Якщо успіх, ми можемо перемістити ZIP-файл у папку Outbox
        // або видалити його для уникнення повторної обробки.
        // Наприклад, перейменуємо в "processed_<ім'я>.zip" або видалимо.
        // QDir().remove(packagePath); // або перемістити

        qInfo() << "Exporter finished successfully. Results saved to Outbox.";
        return 0;
    } else {
        qCritical() << "Exporter failed processing package:" << packageFileName;
        return 1;
    }
}
