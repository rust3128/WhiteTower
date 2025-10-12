#ifndef CLIENTSLISTDIALOG_H
#define CLIENTSLISTDIALOG_H

#include <QDialog>
#include <QJsonArray>  // Додаємо
#include <QJsonObject> // Додаємо

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

    // Слот для обробки натискання кнопок
    void onNewClientClicked();

private:
    // Методи-помічники для налаштування
    void createConnections();
    void loadInitialData();

    Ui::ClientsListDialog *ui;
};

#endif // CLIENTSLISTDIALOG_H
