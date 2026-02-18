#ifndef SYNCSTATUSDIALOG_H
#define SYNCSTATUSDIALOG_H

#include <QDialog>
#include <QMap>
#include <QJsonArray>
#include <QJsonObject>
#include "Oracle/ApiClient.h" // Важливо: підключаємо ApiClient
#include <QTimer>

namespace Ui {
class SyncStatusDialog;
}

class SyncStatusDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SyncStatusDialog(QWidget *parent = nullptr);
    ~SyncStatusDialog();

    // Запит на оновлення списку через API
    void refreshClientList();

private slots:
    // --- UI Слоти ---
    void on_btnSyncAll_clicked();
    void on_btnClose_clicked();
    void on_syncSingleClientClicked(); // Слот для кнопок у таблиці
    void on_btnRefresh_clicked();

    // --- Слоти від SyncManager (Прогрес виконання) ---
    void onClientSyncStarted(int clientId);
    void onSyncRequestSent(int clientId, bool success, QString message);
    void onQueueProgress(int remainingCount);
    void onAllFinished();

    // --- Слоти від ApiClient (Завантаження даних) ---
    void onDashboardDataLoaded(const QJsonArray& data);
    void onDashboardDataFailed(const ApiError& error);

private:
    Ui::SyncStatusDialog *ui;

    // Карта для швидкого пошуку рядка таблиці за ID клієнта
    QMap<int, int> m_clientRowMap;
    QTimer *m_refreshTimer;

    // Метод для налаштування всіх з'єднань (Signals/Slots)
    void createConnections();

    void setupTable();
    void addClientToTable(int clientId, const QString& name, const QString& method,
                          const QString& lastDate, const QString& status, const QString& msg);

    // Встановлює іконку статусу
    void setRowStatus(int row, const QString& status);

    void updateClientRow(int row, const QString& lastDate, const QString& status, const QString& msg);

};

#endif // SYNCSTATUSDIALOG_H
