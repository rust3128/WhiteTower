#ifndef CLIENTSLISTDIALOG_H
#define CLIENTSLISTDIALOG_H

#include "Oracle/ApiClient.h"

#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QListWidgetItem>
#include <QTimer>

namespace Ui {
class ClientsListDialog;
}

class ClientsListDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ClientsListDialog(QWidget *parent = nullptr);
    ~ClientsListDialog();

private slots:
    // Слоти для обробки відповідей від ApiClient
    void onClientsReceived(const QJsonArray &clients);
    void onClientCreated(const QJsonObject &newClient);
    void onClientSelected(QListWidgetItem *current);
    void onClientDetailsReceived(const QJsonObject &client);
    void onIpGenMethodsReceived(const QJsonArray &methods);

    // Слот для обробки натискання кнопок
    void onNewClientClicked();

     void on_comboBoxSyncMetod_currentIndexChanged(int index);

    void on_pushButtonCheckConnections_clicked();

     void on_buttonBox_accepted();

    void on_buttonBox_rejected();
    void onSyncButtonClicked();
    void onSyncRequestAcknowledged(int clientId, bool success, const ApiError& details);
    void checkSyncStatus();
    void onSyncStatusReceived(int clientId, const QJsonObject& status);
    void onSyncButtonToggled(bool checked);

private:
    // Методи-помічники для налаштування
    void createConnections();
    void createUI();
    void loadInitialData();
    QJsonObject formToJson() const;
private:
    Ui::ClientsListDialog *ui;
    QTimer* m_syncStatusTimer;
    int m_currentClientId = -1;
    int m_syncingClientId = -1;
};

#endif // CLIENTSLISTDIALOG_H
