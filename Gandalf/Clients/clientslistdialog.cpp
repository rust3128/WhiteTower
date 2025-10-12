#include "clientslistdialog.h"
#include "ui_clientslistdialog.h"
#include "Oracle/ApiClient.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QListWidgetItem>

ClientsListDialog::ClientsListDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ClientsListDialog)
{
    ui->setupUi(this);
    setWindowTitle("Довідник клієнтів");

    createConnections();
    loadInitialData(); // Запускаємо завантаження даних
}

ClientsListDialog::~ClientsListDialog()
{
    delete ui;
}

// Метод для налаштування всіх з'єднань (сигналів та слотів)
void ClientsListDialog::createConnections()
{
    // Для завантаження списку
    connect(&ApiClient::instance(), &ApiClient::clientsFetched, this, &ClientsListDialog::onClientsReceived);
    connect(&ApiClient::instance(), &ApiClient::clientsFetchFailed, this, [&](const QString& error){
        QMessageBox::critical(this, "Помилка", "Не вдалося завантажити список клієнтів:\n" + error);
    });

    // Для створення нового клієнта
    connect(ui->newClientButton, &QPushButton::clicked, this, &ClientsListDialog::onNewClientClicked);
    connect(&ApiClient::instance(), &ApiClient::clientCreateSuccess, this, &ClientsListDialog::onClientCreated);
    connect(&ApiClient::instance(), &ApiClient::clientCreateFailed, this, [&](const QString& error){
        QMessageBox::critical(this, "Помилка", "Не вдалося створити клієнта:\n" + error);
    });
}

// Метод для початкового завантаження даних
void ClientsListDialog::loadInitialData()
{
    // Запускаємо запит на отримання списку клієнтів
    ApiClient::instance().fetchAllClients();
}

// Слот, який виконається, коли дані про всіх клієнтів прийдуть з сервера
void ClientsListDialog::onClientsReceived(const QJsonArray &clients)
{
    ui->clientsListWidget->clear();

    for (const QJsonValue &value : clients) {
        QJsonObject clientObj = value.toObject();
        QString clientName = clientObj["client_name"].toString();
        int clientId = clientObj["client_id"].toInt();

        QListWidgetItem *item = new QListWidgetItem(clientName);
        item->setData(Qt::UserRole, clientId);
        ui->clientsListWidget->addItem(item);
    }
}

// Слот, що спрацьовує при натисканні "Новий клієнт"
void ClientsListDialog::onNewClientClicked()
{
    bool ok;
    QString clientName = QInputDialog::getText(this, "Створення нового клієнта",
                                               "Введіть назву клієнта:", QLineEdit::Normal,
                                               "", &ok);
    if (ok && !clientName.trimmed().isEmpty()) {
        ApiClient::instance().createClient(clientName.trimmed());
    }
}

// Слот, що спрацьовує, коли сервер підтвердив створення клієнта
void ClientsListDialog::onClientCreated(const QJsonObject &newClient)
{
    QString clientName = newClient["client_name"].toString();
    int clientId = newClient["client_id"].toInt();

    // Додаємо щойно створеного клієнта в наш список
    QListWidgetItem *item = new QListWidgetItem(clientName);
    item->setData(Qt::UserRole, clientId);
    ui->clientsListWidget->addItem(item);

    // Робимо його поточним (виділеним)
    ui->clientsListWidget->setCurrentItem(item);

    QMessageBox::information(this, "Успіх", QString("Клієнт '%1' успішно створений.").arg(clientName));
}
