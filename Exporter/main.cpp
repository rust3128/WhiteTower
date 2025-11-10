#include <QCoreApplication>
#include <QFileInfo>
#include "Exporter.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // (Тут можна налаштувати логер, як ми робили в Conduit/Isengard)
    // const QString appName = QFileInfo(QCoreApplication::applicationFilePath()).baseName();
    // preInitLogger(appName);

    qInfo() << "Exporter started.";

    QString configPath = QCoreApplication::applicationDirPath() + "/Exporter.config.json";
    Exporter exporter(configPath);

    bool success = exporter.run();

    if (success) {
        qInfo() << "Exporter finished successfully.";
        return 0;
    } else {
        qCritical() << "Exporter failed.";
        return 1;
    }
}
