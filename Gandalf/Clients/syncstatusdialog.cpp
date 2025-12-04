#include "syncstatusdialog.h"
#include "ui_syncstatusdialog.h"

#include "SyncManager.h"      // Логіка черги
#include "Oracle/ApiClient.h" // Логіка мережі
#include "Oracle/Logger.h"    // Логування

#include <QPushButton>
#include <QDateTime>
#include <QStyle>
#include <QMessageBox>
#include <QHeaderView>

SyncStatusDialog::SyncStatusDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SyncStatusDialog)
{
    ui->setupUi(this);
    setWindowTitle("Монітор Синхронізації");

    // Трохи збільшимо розмір за замовчуванням
    resize(950, 600);

    setupTable();
    createConnections(); // Всі підключення винесені сюди

    // --- ТАЙМЕР АВТООНОВЛЕННЯ ---
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(5000); // 5000 мс = 5 секунд
    // Таймер просто викликає той самий метод, що і кнопка "Оновити"
    connect(m_refreshTimer, &QTimer::timeout, this, &SyncStatusDialog::refreshClientList);
    m_refreshTimer->start();
    // -----------------------------

    // Запускаємо завантаження даних при відкритті вікна
    refreshClientList();
}

SyncStatusDialog::~SyncStatusDialog()
{
    delete ui;
}

void SyncStatusDialog::createConnections()
{
    // 1. SyncManager -> Dialog (Оновлення статусу в реальному часі)
    connect(&SyncManager::instance(), &SyncManager::clientSyncStarted,
            this, &SyncStatusDialog::onClientSyncStarted);

    connect(&SyncManager::instance(), &SyncManager::clientSyncFinished,
            this, &SyncStatusDialog::onClientSyncFinished);

    connect(&SyncManager::instance(), &SyncManager::queueProgress,
            this, &SyncStatusDialog::onQueueProgress);

    connect(&SyncManager::instance(), &SyncManager::allFinished,
            this, &SyncStatusDialog::onAllFinished);

    // 2. ApiClient -> Dialog (Завантаження початкового списку)
    connect(&ApiClient::instance(), &ApiClient::dashboardDataFetched,
            this, &SyncStatusDialog::onDashboardDataLoaded);

    connect(&ApiClient::instance(), &ApiClient::dashboardDataFetchFailed,
            this, &SyncStatusDialog::onDashboardDataFailed);
}

void SyncStatusDialog::setupTable()
{
    // Налаштування ширини колонок
    ui->tableWidget->setColumnWidth(0, 40);  // Іконка статусу
    ui->tableWidget->setColumnWidth(1, 200); // Назва клієнта
    ui->tableWidget->setColumnWidth(2, 80);  // Метод (DIRECT/FILE)
    ui->tableWidget->setColumnWidth(3, 150); // Дата останньої синхронізації
    ui->tableWidget->setColumnWidth(4, 300); // Повідомлення
    ui->tableWidget->setColumnWidth(5, 100); // Кнопка "Start"

    // Остання колонка не розтягується автоматично, розтягуємо повідомлення
    ui->tableWidget->horizontalHeader()->setStretchLastSection(false);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
}

void SyncStatusDialog::refreshClientList()
{
    // Очищаємо таблицю перед завантаженням (або можна додати лоадер)
//   ui->tableWidget->setRowCount(0);

    // Робимо запит до сервера через API
    ApiClient::instance().fetchDashboardData();
}

// =============================================================================
// ОБРОБКА ДАНИХ ВІД API (Завантаження списку)
// =============================================================================

void SyncStatusDialog::onDashboardDataLoaded(const QJsonArray& data)
{
    // Ми НЕ робимо setRowCount(0) і НЕ очищаємо m_clientRowMap
    // щоб зберегти стан інтерфейсу.

    for (const QJsonValue& val : data) {
        if (!val.isObject()) continue;
        QJsonObject obj = val.toObject();

        int id = obj["client_id"].toInt();
        QString name = obj["client_name"].toString();
        QString method = obj["sync_method"].toString();
        QString lastDate = obj["last_sync_date"].toString();
        QString status = obj["status"].toString();
        QString msg = obj["message"].toString();

        // Перевіряємо, чи є цей клієнт вже в таблиці
        if (m_clientRowMap.contains(id)) {
            // ОНОВЛЮЄМО існуючий рядок
            int row = m_clientRowMap[id];
            updateClientRow(row, lastDate, status, msg);
        } else {
            // ДОДАЄМО новий рядок
            addClientToTable(id, name, method, lastDate, status, msg);
        }
    }
}

void SyncStatusDialog::onDashboardDataFailed(const ApiError& error)
{
    QMessageBox::critical(this, "Помилка",
                          QString("Не вдалося завантажити дані моніторингу:\n%1").arg(error.errorString));
}

// =============================================================================
// РОБОТА З ТАБЛИЦЕЮ
// =============================================================================

void SyncStatusDialog::addClientToTable(int clientId, const QString& name, const QString& method,
                                        const QString& lastDate, const QString& status, const QString& msg)
{
    int row = ui->tableWidget->rowCount();
    ui->tableWidget->insertRow(row);

    // Зберігаємо зв'язок: ID Клієнта -> Номер рядка
    m_clientRowMap.insert(clientId, row);

    // 0. Статус (Іконка)
    QTableWidgetItem *itemStatus = new QTableWidgetItem();
    itemStatus->setTextAlignment(Qt::AlignCenter);
    ui->tableWidget->setItem(row, 0, itemStatus);
    setRowStatus(row, status);

    // 1. Ім'я
    ui->tableWidget->setItem(row, 1, new QTableWidgetItem(name));

    // 2. Метод
    ui->tableWidget->setItem(row, 2, new QTableWidgetItem(method));

    // 3. Дата
    ui->tableWidget->setItem(row, 3, new QTableWidgetItem(lastDate));

    // 4. Повідомлення
    QTableWidgetItem *itemMsg = new QTableWidgetItem(msg);
    itemMsg->setToolTip(msg); // Повний текст у підказці
    ui->tableWidget->setItem(row, 4, itemMsg);

    // 5. Кнопка "Start"
    QPushButton *btnSync = new QPushButton("Почати");
    // Зберігаємо ID клієнта прямо у властивості кнопки
    btnSync->setProperty("clientId", clientId);
    connect(btnSync, &QPushButton::clicked, this, &SyncStatusDialog::on_syncSingleClientClicked);

    ui->tableWidget->setCellWidget(row, 5, btnSync);
}

void SyncStatusDialog::setRowStatus(int row, const QString& status)
{
    QTableWidgetItem *item = ui->tableWidget->item(row, 0);
    if (!item) return;

    QStyle *style = QApplication::style();

    // Використовуємо стандартні іконки Qt
    if (status == "SUCCESS") {
        item->setIcon(style->standardIcon(QStyle::SP_DialogApplyButton)); // Зелена галочка
        item->setToolTip("Успішно");
    } else if (status == "ERROR" || status == "FAILED") {
        item->setIcon(style->standardIcon(QStyle::SP_MessageBoxCritical)); // Червоний хрест
        item->setToolTip("Помилка");
    } else if (status == "PENDING" || status == "RUNNING") {
        item->setIcon(style->standardIcon(QStyle::SP_BrowserReload)); // Стрілочка оновлення
        item->setToolTip("Виконується...");
    } else {
        item->setIcon(QIcon()); // Пусто
        item->setToolTip("Невідомо");
    }
}

// =============================================================================
// СЛОТИ UI (Кнопки)
// =============================================================================

void SyncStatusDialog::on_btnSyncAll_clicked()
{
    // Проходимо по всіх клієнтах, що є в таблиці, і додаємо їх у чергу
    for (auto it = m_clientRowMap.begin(); it != m_clientRowMap.end(); ++it) {
        int clientId = it.key();
        SyncManager::instance().queueClient(clientId);
    }
    ui->progressBar->setFormat("Завдання додано в чергу...");
}

void SyncStatusDialog::on_btnClose_clicked()
{
    close();
}

void SyncStatusDialog::on_syncSingleClientClicked()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (btn) {
        int clientId = btn->property("clientId").toInt();
        SyncManager::instance().queueClient(clientId);
    }
}

// =============================================================================
// СЛОТИ ВІД SYNCMANAGER (Оновлення в реальному часі)
// =============================================================================

void SyncStatusDialog::onClientSyncStarted(int clientId)
{
    if (!m_clientRowMap.contains(clientId)) return;
    int row = m_clientRowMap[clientId];

    setRowStatus(row, "RUNNING");
    ui->tableWidget->item(row, 4)->setText("Синхронізація розпочата...");

    // Блокуємо кнопку
    if (QWidget *w = ui->tableWidget->cellWidget(row, 5)) {
        w->setEnabled(false);
    }
}

void SyncStatusDialog::onClientSyncFinished(int clientId, bool success, QString message)
{
    if (!m_clientRowMap.contains(clientId)) return;
    int row = m_clientRowMap[clientId];

    setRowStatus(row, success ? "SUCCESS" : "ERROR");
    ui->tableWidget->item(row, 4)->setText(message);
    ui->tableWidget->item(row, 3)->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));

    // Розблокуємо кнопку
    if (QWidget *w = ui->tableWidget->cellWidget(row, 5)) {
        w->setEnabled(true);
    }
}

void SyncStatusDialog::onQueueProgress(int remainingCount)
{
    // Режим "бігаючого" прогрес-бару, бо ми не знаємо точного % виконання одного клієнта
    ui->progressBar->setRange(0, 0);
    ui->progressBar->setFormat(QString("В черзі: %1").arg(remainingCount));
}

void SyncStatusDialog::onAllFinished()
{
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(100);
    ui->progressBar->setFormat("Всі завдання завершено");
}

void SyncStatusDialog::on_btnRefresh_clicked()
{
    // Очищаємо таблицю візуально, щоб видно було, що процес пішов
    // ui->tableWidget->setRowCount(0);

    // Запускаємо оновлення
    refreshClientList();
}

void SyncStatusDialog::updateClientRow(int row, const QString& lastDate, const QString& status, const QString& msg)
{
    // Додатковий захист: якщо карта каже, що рядок є, а таблиця каже, що нема
    if (row >= ui->tableWidget->rowCount()) return;

    // Оновлюємо статус (іконку)
    setRowStatus(row, status);

    // Оновлюємо дату (колонка 3)
    QTableWidgetItem* dateItem = ui->tableWidget->item(row, 3);
    if (dateItem) {
        dateItem->setText(lastDate);
    }

    // Оновлюємо повідомлення (колонка 4)
    QTableWidgetItem* msgItem = ui->tableWidget->item(row, 4);
    if (msgItem) {
        msgItem->setText(msg);
        msgItem->setToolTip(msg);
    }
}
