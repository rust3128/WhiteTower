#include "clientslistdialog.h"
#include "ui_clientslistdialog.h"
#include "Oracle/ApiClient.h"
//#include "Oracle/criptpass.h"
#include "Oracle/Logger.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QListWidgetItem>
#include <QJsonDocument>

ClientsListDialog::ClientsListDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ClientsListDialog)
{
    ui->setupUi(this);
    setWindowTitle("Довідник клієнтів");
    m_syncStatusTimer = new QTimer(this);
    createConnections();
    loadInitialData(); // Запускаємо завантаження даних
    createUI();
}

ClientsListDialog::~ClientsListDialog()
{
    delete ui;
}

void ClientsListDialog::createUI()
{
    ui->comboBoxSyncMetod->addItems({"DIRECT", "PALANTIR", "FILE"});
    ui->groupBox_2->setVisible(false);
}


// Метод для налаштування всіх з'єднань (сигналів та слотів)
void ClientsListDialog::createConnections()
{
    // Макрос-помічник для уникнення дублювання коду
    auto showErrorBox = [this](const QString& title, const ApiError& error) {
        logCritical() << title << "Details:" << error.errorString << "| URL:" << error.requestUrl << "| HTTP Status:" << error.httpStatusCode;
        QMessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText(title);
        msgBox.setInformativeText(error.errorString);
        msgBox.setDetailedText(QString("URL: %1\nHTTP Status: %2").arg(error.requestUrl).arg(error.httpStatusCode));
        msgBox.exec();
    };

    // --- З'єднання для операцій з API ---
    connect(&ApiClient::instance(), &ApiClient::clientsFetched, this, &ClientsListDialog::onClientsReceived);
    connect(&ApiClient::instance(), &ApiClient::clientsFetchFailed, this, [=](const ApiError& error){
        showErrorBox("Не вдалося завантажити список клієнтів.", error);
    });

    connect(&ApiClient::instance(), &ApiClient::clientCreateSuccess, this, &ClientsListDialog::onClientCreated);
    connect(&ApiClient::instance(), &ApiClient::clientCreateFailed, this, [=](const ApiError& error){
        showErrorBox("Не вдалося створити клієнта.", error);
    });

    connect(&ApiClient::instance(), &ApiClient::clientDetailsFetched, this, &ClientsListDialog::onClientDetailsReceived);
    connect(&ApiClient::instance(), &ApiClient::clientDetailsFetchFailed, this, [=](const ApiError& error){
        showErrorBox("Не вдалося завантажити деталі клієнта.", error);
    });

    connect(&ApiClient::instance(), &ApiClient::clientUpdateSuccess, this, [this](){
        QMessageBox::information(this, "Успіх", "Дані клієнта успішно оновлено.");
        loadInitialData();
    });
    connect(&ApiClient::instance(), &ApiClient::clientUpdateFailed, this, [=](const ApiError& error){
        showErrorBox("Не вдалося оновити дані клієнта.", error);
    });

    connect(&ApiClient::instance(), &ApiClient::ipGenMethodsFetched, this, &ClientsListDialog::onIpGenMethodsReceived);
    connect(&ApiClient::instance(), &ApiClient::ipGenMethodsFetchFailed, this, [=](const ApiError& error){
        showErrorBox("Не вдалося завантажити методи генерації IP.", error);
    });

    connect(&ApiClient::instance(), &ApiClient::connectionTestSuccess, this, [&](const QString& message){
        QMessageBox::information(this, "Перевірка з'єднання", message);
    });
    connect(&ApiClient::instance(), &ApiClient::connectionTestFailed, this, [=](const ApiError& error){
        // Для цієї помилки використовуємо старий формат, бо вона повертає простий рядок
        QMessageBox::warning(this, "Перевірка з'єднання", "Помилка:\n" + error.errorString);
    });

    connect(ui->clientsListWidget, &QListWidget::currentItemChanged, this, &ClientsListDialog::onClientSelected);

    ui->pushButtonSync->setCheckable(true);
    connect(ui->pushButtonSync, &QPushButton::toggled, this, &ClientsListDialog::onSyncButtonToggled);

    // З'єднуємо сигнали для отримання статусу
    connect(&ApiClient::instance(), &ApiClient::syncRequestAcknowledged, this, &ClientsListDialog::onSyncRequestAcknowledged);
    connect(&ApiClient::instance(), &ApiClient::syncStatusFetched, this, &ClientsListDialog::onSyncStatusReceived);
    connect(m_syncStatusTimer, &QTimer::timeout, this, &ClientsListDialog::checkSyncStatus);

    // Обробка помилок (опціонально, але рекомендовано)
    connect(&ApiClient::instance(), &ApiClient::syncStatusFetchFailed, this, [this](int clientId, const ApiError& error){
        if (clientId == m_syncingClientId) {
            m_syncStatusTimer->stop();
            ui->pushButtonSync->setChecked(false); // "Відтискаємо" кнопку
            ui->pushButtonSync->setText("Синхронізувати");
            ui->pushButtonSync->setEnabled(true);
            QMessageBox::warning(this, "Помилка", "Не вдалося отримати статус синхронізації:\n" + error.errorString);
        }
    });
}


// Метод для початкового завантаження даних
void ClientsListDialog::loadInitialData()
{
    // Запускаємо запит на отримання списку клієнтів
    ApiClient::instance().fetchAllClients();

    ApiClient::instance().fetchAllIpGenMethods();
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
                                               "Введіть назву нового клієнта:", QLineEdit::Normal,
                                               "", &ok);
    if (ok && !clientName.trimmed().isEmpty()) {
        // === ВИПРАВЛЕНО ТУТ ===
        // Створюємо мінімальний JSON-об'єкт, який очікує сервер
        QJsonObject newClientData;
        newClientData["client_name"] = clientName.trimmed();

        // Тепер викликаємо метод, передаючи QJsonObject
        ApiClient::instance().createClient(newClientData);
    }
    // Цей рядок тут більше не потрібен, бо ми не переходимо в режим редагування
    // m_currentClientId = -1;
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

void ClientsListDialog::onClientSelected(QListWidgetItem *current)
{
    ui->groupBox_2->setVisible(current != nullptr);
    if (!current) {
        // Тут можна очистити форму справа
        m_currentClientId = -1;
        return;
    }
    m_currentClientId = current->data(Qt::UserRole).toInt(); // Зберігаємо ID обраного клієнта
    ui->pushButtonSync->setEnabled(current != nullptr);
    ApiClient::instance().fetchClientById(m_currentClientId);
}

void ClientsListDialog::onClientDetailsReceived(const QJsonObject &client)
{
    // === ЗАГАЛЬНА ВКЛАДКА ===
    ui->labelTitle->setText(client["client_name"].toString()); // Оновлюємо великий заголовок
    ui->lineEditClientName->setText(client["client_name"].toString());
    ui->checkBoxIsActive->setChecked(client["is_active"].toBool());

    // Встановлюємо значення для випадаючих списків
    ui->comboBoxSyncMetod->setCurrentText(client["sync_method"].toString());

    // Для ip_gen_method_id ми шукаємо елемент за ID, який ми зберегли раніше
    int ipGenMethodId = client["ip_gen_method_id"].toInt();
    int index = ui->comboBoxTemplateHostname->findData(ipGenMethodId);
    if (index != -1) { // -1, якщо не знайдено
        ui->comboBoxTemplateHostname->setCurrentIndex(index);
    }

    // Заповнюємо інші поля з таблиці CLIENTS
    ui->lineEditMinTermID->setText(QString::number(client["term_id_min"].toInt()));
    ui->lineEditMaxTermID->setText(QString::number(client["term_id_max"].toInt()));
    ui->lineEditAZSFbPass->setText(client["gas_station_db_password"].toString());

    // === ВКЛАДКА "СИНХРОНІЗАЦІЯ" ===
    QString syncMethod = client["sync_method"].toString();

    if (syncMethod == "DIRECT" && client.contains("config_direct")) {
        QJsonObject directConfig = client["config_direct"].toObject();
        ui->lineEditServerName->setText(directConfig["db_host"].toString());
        ui->spinBoxPort->setValue(directConfig["db_port"].toInt());
        ui->lineEditDBFileName->setText(directConfig["db_path"].toString());
        ui->lineEditUser->setText(directConfig["db_user"].toString());
        ui->lineEditPass->setText(directConfig["db_password"].toString());
    }
    else if (syncMethod == "PALANTIR" && client.contains("config_palantir")) {
        // Логіка для Palantir (буде реалізована, коли ми додамо її в DbManager)
        // QJsonObject palantirConfig = client["config_palantir"].toObject();
        // ui->lineEditApiKeyPalantir->setText(palantirConfig["api_key"].toString());
    }
    else if (syncMethod == "FILE" && client.contains("config_file")) {
        // Логіка для File
        // QJsonObject fileConfig = client["config_file"].toObject();
        // ui->lineEditPathroFile->setText(fileConfig["import_path"].toString());
    }
}

void ClientsListDialog::onIpGenMethodsReceived(const QJsonArray &methods)
{
    ui->comboBoxTemplateHostname->clear();
    ui->comboBoxTemplateHostname->addItem("Не визначено", 0);
    for (const QJsonValue &value : methods) {
        QJsonObject methodObj = value.toObject();
        QString name = methodObj["method_name"].toString();
        int id = methodObj["method_id"].toInt();
        // Додаємо і назву (яку бачить юзер), і ID (який ми будемо зберігати)
        ui->comboBoxTemplateHostname->addItem(name, id);
    }
}

void ClientsListDialog::on_comboBoxSyncMetod_currentIndexChanged(int index)
{
    ui->stackedWidgetSync->setCurrentIndex(index);
}


void ClientsListDialog::on_pushButtonCheckConnections_clicked()
{
    QJsonObject config;
    config["db_host"] = ui->lineEditServerName->text();
    config["db_port"] = ui->spinBoxPort->value();
    config["db_path"] = ui->lineEditDBFileName->text();
    config["db_user"] = ui->lineEditUser->text();
    config["db_password"] = ui->lineEditPass->text(); // Шифруємо пароль перед відправкою

    ApiClient::instance().testDbConnection(config);
}

QJsonObject ClientsListDialog::formToJson() const
{
    QJsonObject root;
    // Знаходимо всі віджети, у яких є властивість "jsonKey"
    const auto widgets = this->findChildren<QWidget*>();

    for (QWidget* w : widgets) {
        // Перевіряємо, чи є у віджета наша властивість
        QString key = w->property("jsonKey").toString();
        if (key.isEmpty()) {
            continue;
        }

        QVariant value;
        // Отримуємо значення в залежності від типу віджета
        if (auto widget = qobject_cast<QLineEdit*>(w)) {
            value = widget->text();
        } else if (auto widget = qobject_cast<QCheckBox*>(w)) {
            value = widget->isChecked();
        } else if (auto widget = qobject_cast<QComboBox*>(w)) {
            // Для comboBox'ів ми зберігаємо або текст, або пов'язані дані (ID)
            QVariant data = widget->currentData();
            value = data.isValid() ? data : widget->currentText();
        } else if (auto widget = qobject_cast<QSpinBox*>(w)) {
            value = widget->value();
        }

        // Обробка вкладених об'єктів (напр., "config_direct.db_host")
        if (key.contains('.')) {
            QStringList parts = key.split('.');
            QJsonObject parent = root[parts[0]].toObject();
            parent[parts[1]] = QJsonValue::fromVariant(value);
            root[parts[0]] = parent;
        } else {
            root[key] = QJsonValue::fromVariant(value);
        }
    }
    return root;
}

void ClientsListDialog::on_buttonBox_accepted()
{
    logDebug() << "Save button clicked. Current Client ID is:" << m_currentClientId;
    QJsonObject clientData = formToJson();

    if (m_currentClientId != -1) {
        // --- РЕЖИМ ОНОВЛЕННЯ ---
        ApiClient::instance().updateClient(m_currentClientId, clientData);
    } else {
        // --- РЕЖИМ СТВОРЕННЯ ---
        ApiClient::instance().createClient(clientData);
    }
    ui->groupBox_2->setVisible(false);
}


void ClientsListDialog::on_buttonBox_rejected()
{
    this->reject();
}

void ClientsListDialog::onSyncButtonClicked()
{
    if (m_currentClientId != -1) {
        logDebug() << "Sync button clicked for client ID:" << m_currentClientId;
        ui->pushButtonSync->setEnabled(false); // Вимикаємо кнопку, щоб уникнути подвійних кліків
        ui->pushButtonSync->setText("Синхронізація...");

        ApiClient::instance().syncClientObjects(m_currentClientId);
    }
}

// void ClientsListDialog::onSyncRequestAcknowledged(int clientId, bool success, const ApiError& details)
// {
//     if (clientId != m_currentClientId) {
//         return;
//     }

//     ui->pushButtonSync->setEnabled(true);
//     ui->pushButtonSync->setText("Синхронізувати об'єкти");

//     if (success) {
//         QMessageBox::information(this,
//                                  "Завдання прийнято",
//                                  QString("Сервер почав синхронізацію для клієнта (ID: %1).\n%2")
//                                      .arg(clientId).arg(details.errorString)); // Використовуємо поле зі структури
//     } else {
//         // Тепер ми можемо показати більш детальну помилку!
//         QMessageBox msgBox(this);
//         msgBox.setIcon(QMessageBox::Warning);
//         msgBox.setText(QString("Не вдалося запустити синхронізацію для клієнта (ID: %1).").arg(clientId));
//         msgBox.setInformativeText(details.errorString);
//         msgBox.setDetailedText(QString("URL: %1\nHTTP Status: %2").arg(details.requestUrl).arg(details.httpStatusCode));
//         msgBox.exec();
//     }
// }


void ClientsListDialog::onSyncButtonToggled(bool checked)
{
    if (checked) {
        // Кнопку натиснули - запускаємо синхронізацію
        if (m_currentClientId == -1) {
            ui->pushButtonSync->setChecked(false); // Відтискаємо, якщо клієнт не обраний
            return;
        }
        ui->pushButtonSync->setText("Синхронізація...");
        ui->pushButtonSync->setEnabled(false); // Блокуємо, поки не отримаємо відповідь
        ApiClient::instance().syncClientObjects(m_currentClientId);
    } else {
        // Кнопку "відтиснули" програмно, коли все завершено - нічого не робимо
    }
}

// Цей слот реагує на початкову відповідь сервера (202 Accepted)
void ClientsListDialog::onSyncRequestAcknowledged(int clientId, bool success, const ApiError& details)
{
    if (success) {
        m_syncingClientId = clientId;
        ui->pushButtonSync->setEnabled(true); // Розблоковуємо, щоб показати процес
        m_syncStatusTimer->start(2000); // Запускаємо опитування кожні 2 секунди
    } else {
        QMessageBox::critical(this, "Помилка запуску", details.errorString);
        ui->pushButtonSync->setChecked(false);
        ui->pushButtonSync->setText("Синхронізувати");
        ui->pushButtonSync->setEnabled(true);
    }
}

// Цей слот викликається таймером
void ClientsListDialog::checkSyncStatus()
{
    if (m_syncingClientId != -1) {
        ApiClient::instance().fetchSyncStatus(m_syncingClientId);
    }
}

// Цей слот обробляє отриманий статус
void ClientsListDialog::onSyncStatusReceived(int clientId, const QJsonObject& status)
{
    if (clientId != m_syncingClientId) return; // Ігноруємо, якщо це не наш клієнт

    QString currentStatus = status["status"].toString();
    if (currentStatus == "SUCCESS") {
        m_syncStatusTimer->stop();
        m_syncingClientId = -1;
        ui->pushButtonSync->setChecked(false); // "Відтискаємо" кнопку
        ui->pushButtonSync->setText("Синхронізувати");
        QMessageBox::information(this, "Успіх", "Синхронізація успішно завершена.\n" + status["message"].toString());
    } else if (currentStatus == "FAILED") {
        m_syncStatusTimer->stop();
        m_syncingClientId = -1;
        ui->pushButtonSync->setChecked(false);
        ui->pushButtonSync->setText("Синхронізувати");
        QMessageBox::critical(this, "Помилка", "Синхронізація не вдалася:\n" + status["message"].toString());
    }
    // Якщо статус "PENDING" або інший, просто чекаємо наступного спрацювання таймера
}
