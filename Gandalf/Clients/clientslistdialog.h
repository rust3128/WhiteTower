#ifndef CLIENTSLISTDIALOG_H
#define CLIENTSLISTDIALOG_H

#include "Oracle/ApiClient.h"

#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QListWidgetItem>
#include <QTimer>
#include <QAction>

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

    void on_pushButtonGenerateExporter_clicked();
    void onExportTasksFetched(const QJsonArray& tasks);
    void onExportTasksFetchFailed(const ApiError& error);
    void onFieldChanged(); // реагує на будь-яку зміну


    void on_toolButtonBrowseImport_clicked();



private:
    // Методи-помічники для налаштування
    void createConnections();
    void createUI();
    void loadInitialData();
    QJsonObject formToJson() const;
    void generateExporterPackage(const QJsonArray& tasks);
    QJsonObject gatherClientDataForConfig();

private:
    Ui::ClientsListDialog *ui;
    QTimer* m_syncStatusTimer;
    int m_currentClientId = -1;
    int m_syncingClientId = -1;
    QJsonArray m_exportTasks;
    QJsonObject m_currentClientData;
    bool m_isDirty;

    // --------------------------------------------------
    // (ВИДАЛЕНО) QString m_pendingHostnameTemplate;
    // --------------------------------------------------

    // (ДОДАНО) Надійний буфер для ID
    int m_pendingIpGenMethodId;
    QAction* m_passVisAction;
    QAction* m_azsPassVisAction;
    QAction* m_apiKeyVisAction;
};

#endif // CLIENTSLISTDIALOG_H
