#include "mainwindow.h"
#include "Oracle/User.h"
#include "version.h"
#include "./ui_mainwindow.h"
#include "Users/userlistdialog.h"
#include "Clients/clientslistdialog.h"
#include "Settings/settingsdialog.h"
#include "Settings/exporttasksdialog.h"
#include "Clients/objectslistdialog.h"
#include "Oracle/SessionManager.h"
#include "Oracle/Logger.h"
#include "Clients/syncstatusdialog.h"
#include "Oracle/ApiClient.h"
#include "Oracle/AppParams.h"
#include "Clients/SyncManager.h"

#include <QMessageBox>
#include <QTimer>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QString windowTitle = QString("Gandalf %1").arg(PROJECT_VERSION_STR);
    setWindowTitle(windowTitle);

    // Читаємо параметр (періодичність у днях).
    // Якщо параметра немає, беремо 1 день за замовчуванням.
    m_syncPeriodDays = AppParams::instance().getParam("Gandalf", "SyncPeriodDays", 1).toInt();

    // Якщо прийшов 0 або помилка -> ставимо 1
    if (m_syncPeriodDays <= 0) m_syncPeriodDays = 1;

    if (m_syncPeriodDays > 0) {
        logInfo() << "Auto-sync check scheduled in 2 seconds. Period:" << m_syncPeriodDays << "days.";
        // Запускаємо перевірку через 2 секунди після старту
        QTimer::singleShot(2000, this, &MainWindow::checkAutoSyncNeeded);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionUsers_triggered()
{
    UserListDialog *userList = new UserListDialog();
    userList->exec();
}


void MainWindow::on_actionClients_triggered()
{
    ClientsListDialog dlg(this);
    dlg.exec();
}


void MainWindow::on_action_triggered()
{
    SettingsDialog dlg(this);
    dlg.exec();
}


void MainWindow::on_actionObjectsList_triggered()
{
    ObjectsListDialog dlg(this);
    dlg.exec();
}


void MainWindow::on_actionExportTasks_triggered()
{
    // Перевірка, чи користувач має права адміністратора
    const User* currentUser = SessionManager::instance().currentUser();
    if (!currentUser || !currentUser->isAdmin()) {
        QMessageBox::critical(this, "Доступ заборонено",
                              "У вас немає прав адміністратора для керування завданнями експорту.");
        return;
    }

    logInfo() << "Opening Export Tasks Management Dialog.";

    // Створюємо та запускаємо діалог
    ExportTasksDialog dialog(this);
    dialog.exec();
}


void MainWindow::on_actionOpenSyncMonitor_triggered()
{
    SyncStatusDialog dlg(this);

    // Відкриваємо його в модальному режимі (блокує головне вікно, поки відкритий)
    dlg.exec();
}

void MainWindow::checkAutoSyncNeeded()
{
    // Підписуємося на сигнал ОТРИМАННЯ ДАНИХ один раз
    // Важливо використати Qt::SingleShotConnection, щоб цей слот спрацював лише 1 раз,
    // а не викликався щоразу, коли ми відкриваємо діалог моніторингу.
    connect(&ApiClient::instance(), &ApiClient::dashboardDataFetched,
            this, &MainWindow::onDashboardDataForAutoSync,
            Qt::SingleShotConnection);

    // Робимо запит до API (той самий, що і для діалогу)
    ApiClient::instance().fetchDashboardData();
}

void MainWindow::onDashboardDataForAutoSync(const QJsonArray& data)
{
    logInfo() << "Checking clients for auto-sync...";
    int queuedCount = 0;
    QDateTime now = QDateTime::currentDateTime();

    for (const QJsonValue& val : data) {
        QJsonObject obj = val.toObject();
        int clientId = obj["client_id"].toInt();
        QString lastSyncStr = obj["last_sync_date"].toString();
        QString status = obj["status"].toString();

        bool needSync = false;

        // 1. Якщо ніколи не синхронізувався
        if (lastSyncStr.isEmpty()) {
            needSync = true;
        }
        else {
            // 2. Перевіряємо давність
            QDateTime lastSync = QDateTime::fromString(lastSyncStr, "yyyy-MM-dd HH:mm:ss");
            if (lastSync.isValid()) {
                // Різниця у днях
                qint64 daysDiff = lastSync.daysTo(now);
                if (daysDiff >= m_syncPeriodDays) {
                    needSync = true;
                }
            } else {
                // Дата некоректна -> синхронізуємо
                needSync = true;
            }
        }

        // Додаткова умова: Якщо останній статус FAILED -> теж можна спробувати ще раз
        if (status == "FAILED") {
            needSync = true;
        }

        if (needSync) {
            logInfo() << "Auto-sync triggered for client" << clientId;
            SyncManager::instance().queueClient(clientId);
            queuedCount++;
        }
    }

    if (queuedCount > 0) {
        // Можна показати спливаюче повідомлення у треї або статус-барі
        ui->statusbar->showMessage(QString("Автозапуск: Розпочато синхронізацію %1 клієнтів").arg(queuedCount), 10000);

        // (Опціонально) Можна автоматично відкрити монітор, якщо хочете:
        // on_actionOpenSyncMonitor_triggered();
    } else {
        logInfo() << "All clients are up to date.";
    }
}
