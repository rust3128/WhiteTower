#include "clientslistdialog.h"
#include "ui_clientslistdialog.h"
#include "Oracle/ApiClient.h"
#include "Oracle/criptpass.h"

#include "Oracle/SessionManager.h" // Для перевірки ролі
#include "Oracle/User.h"           // Для об'єкта User
#include <QAction>                 // Для кнопки "Око"
#include <QIcon>                   // Для іконок
#include <QStyle>                  // Для стандартних іконок
#include "Oracle/Logger.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QListWidgetItem>
#include <QJsonDocument>
#include <QFile>
#include <QFileDialog>
#include <QProcess>
#include <QTemporaryDir>
#include <QCoreApplication>

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
    m_pendingIpGenMethodId = -1; // (ДОДАНО) Ініціалізуємо наш буфер
}

ClientsListDialog::~ClientsListDialog()
{
    delete ui;
}

void ClientsListDialog::createUI()
{
    ui->comboBoxSyncMetod->addItems({"DIRECT", "PALANTIR", "FILE"});
    ui->groupBoxClientSettings->setVisible(false);
    // Ховаємо групи
    ui->groupBoxFileSettings->setVisible(false);
    ui->groupBoxSyncAPI->setVisible(false);
    ui->pushButtonGenerateExporter->setVisible(false);

    // "Безпечна Синхронізація": вимикаємо кнопку
    ui->pushButtonSync->setEnabled(false);
    m_isDirty = true;

    // --- (ДОДАНО) НАЛАШТУВАННЯ ВИДИМОСТІ ПАРОЛІВ ---

    // 1. Встановлюємо "зірочки" для всіх полів за замовчуванням
    ui->lineEditPass->setEchoMode(QLineEdit::Password);
    ui->lineEditAZSFbPass->setEchoMode(QLineEdit::Password);
    ui->lineEditApiKeyPalantir->setEchoMode(QLineEdit::Password);

    // 2. Перевіряємо, чи поточний користувач - адмін
    const User* currentUser = SessionManager::instance().currentUser();
    bool isAdmin = (currentUser && currentUser->isAdmin());

    // 3. Якщо не адмін, просто виходимо. Поля залишаться з "зірочками".
    if (!isAdmin) {
        logDebug() << "User is not admin. Password visibility actions will not be created.";
        return;
    }

    logDebug() << "User is admin. Creating password visibility actions.";

    // 4. Якщо АДМІН, створюємо кнопки "Око"

    // --- Допоміжна лямбда-функція ---
    auto createVisibilityAction = [this](QLineEdit* lineEdit, const QIcon& visibleIcon, const QIcon& hiddenIcon) -> QAction* {
        QAction* action = new QAction(hiddenIcon, "Show", this);
        action->setCheckable(true);

        connect(action, &QAction::toggled, this, [=](bool checked) {
            if (checked) {
                lineEdit->setEchoMode(QLineEdit::Normal);
                action->setIcon(visibleIcon);
            } else {
                lineEdit->setEchoMode(QLineEdit::Password);
                action->setIcon(hiddenIcon);
            }
        });

        lineEdit->addAction(action, QLineEdit::TrailingPosition);
        return action;
    };
    // --- Кінець лямбда-функції ---

    // 5. Завантажуємо іконки. (Якщо їх немає, будуть стандартні)
    QIcon eyeOpenIcon(":/res/Images/eye-open.png");
    QIcon eyeClosedIcon(":/res/Images/eye-closed.png");

    if (eyeOpenIcon.isNull())
        eyeOpenIcon = style()->standardIcon(QStyle::SP_DialogYesButton);
    if (eyeClosedIcon.isNull())
        eyeClosedIcon = style()->standardIcon(QStyle::SP_DialogNoButton);


    // 6. Створюємо кнопки для КОЖНОГО поля
    m_passVisAction = createVisibilityAction(ui->lineEditPass, eyeOpenIcon, eyeClosedIcon);
    m_azsPassVisAction = createVisibilityAction(ui->lineEditAZSFbPass, eyeOpenIcon, eyeClosedIcon);
    m_apiKeyVisAction = createVisibilityAction(ui->lineEditApiKeyPalantir, eyeOpenIcon, eyeClosedIcon);
    m_vncPassVisAction = createVisibilityAction(ui->lineEditVncPassword, eyeOpenIcon, eyeClosedIcon);
}


// Метод для налаштування всіх з'єднань (сигналів та слотів)
void ClientsListDialog::createConnections()
{
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
        // (Після успішного збереження ми *дозволяємо* синхронізацію)
        m_isDirty = false;
        ui->pushButtonSync->setEnabled(true);
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
        QMessageBox::warning(this, "Перевірка з'єднання", "Помилка:\n" + error.errorString);
    });

    connect(ui->clientsListWidget, &QListWidget::currentItemChanged, this, &ClientsListDialog::onClientSelected);

    ui->pushButtonSync->setCheckable(true);
    connect(ui->pushButtonSync, &QPushButton::toggled, this, &ClientsListDialog::onSyncButtonToggled);
    connect(ui->newClientButton, &QPushButton::clicked, this, &ClientsListDialog::onNewClientClicked);

    connect(&ApiClient::instance(), &ApiClient::syncRequestAcknowledged, this, &ClientsListDialog::onSyncRequestAcknowledged);
    connect(&ApiClient::instance(), &ApiClient::syncStatusFetched, this, &ClientsListDialog::onSyncStatusReceived);
    connect(m_syncStatusTimer, &QTimer::timeout, this, &ClientsListDialog::checkSyncStatus);

    connect(&ApiClient::instance(), &ApiClient::syncStatusFetchFailed, this, [this](int clientId, const ApiError& error){
        if (clientId == m_syncingClientId) {
            m_syncStatusTimer->stop();
            ui->pushButtonSync->setChecked(false);
            ui->pushButtonSync->setText("Синхронізувати");
            ui->pushButtonSync->setEnabled(true);
            QMessageBox::warning(this, "Помилка", "Не вдалося отримати статус синхронізації:\n" + error.errorString);
        }
    });

    connect(&ApiClient::instance(), &ApiClient::exportTasksFetched, this, &ClientsListDialog::onExportTasksFetched);
    connect(&ApiClient::instance(), &ApiClient::exportTasksFetchFailed, this, &ClientsListDialog::onExportTasksFetchFailed);

    // "Безпечна Синхронізація"
    // (Переконайтеся, що назви полів тут відповідають вашому .ui)
    connect(ui->lineEditClientName, &QLineEdit::textChanged, this, &ClientsListDialog::onFieldChanged);
    connect(ui->checkBoxIsActive, &QCheckBox::stateChanged, this, &ClientsListDialog::onFieldChanged);
    connect(ui->comboBoxSyncMetod, &QComboBox::currentIndexChanged, this, &ClientsListDialog::onFieldChanged);
    connect(ui->lineEditMinTermID, &QLineEdit::textChanged, this, &ClientsListDialog::onFieldChanged);
    connect(ui->lineEditMaxTermID, &QLineEdit::textChanged, this, &ClientsListDialog::onFieldChanged);
    connect(ui->lineEditAZSFbPass, &QLineEdit::textChanged, this, &ClientsListDialog::onFieldChanged);
    connect(ui->lineEditServerName, &QLineEdit::textChanged, this, &ClientsListDialog::onFieldChanged);
    connect(ui->spinBoxPort, &QSpinBox::valueChanged, this, &ClientsListDialog::onFieldChanged);
    connect(ui->lineEditDBFileName, &QLineEdit::textChanged, this, &ClientsListDialog::onFieldChanged);
    connect(ui->lineEditUser, &QLineEdit::textChanged, this, &ClientsListDialog::onFieldChanged);
    connect(ui->lineEditPass, &QLineEdit::textChanged, this, &ClientsListDialog::onFieldChanged);
    connect(ui->lineEditImportPathFile, &QLineEdit::textChanged, this, &ClientsListDialog::onFieldChanged);
    connect(ui->lineEditApiKeyPalantir, &QLineEdit::textChanged, this, &ClientsListDialog::onFieldChanged);
    // connect(ui->comboBoxTemplateHostname, &QComboBox::currentIndexChanged, this, &ClientsListDialog::onFieldChanged);
    connect(ui->lineEditVncPath, &QLineEdit::textChanged, this, &ClientsListDialog::onFieldChanged);
    connect(ui->spinBoxVncPort, &QSpinBox::valueChanged, this, &ClientsListDialog::onFieldChanged);
    connect(ui->lineEditVncPassword, &QLineEdit::textChanged, this, &ClientsListDialog::onFieldChanged);
    connect(ui->lineEditIpPrefix, &QLineEdit::textChanged, this, &ClientsListDialog::onFieldChanged);

}


void ClientsListDialog::loadInitialData()
{
    ApiClient::instance().fetchAllClients();
    ApiClient::instance().fetchAllIpGenMethods();
}

// (ЗАМІНІТЬ ВСЮ ФУНКЦІЮ)
void ClientsListDialog::onClientsReceived(const QJsonArray &clients)
{
    // 1. (ВИПРАВЛЕННЯ) Отримуємо ID *ДО* того, як 'clear()' його скине
    int previouslySelectedId = -1;
    if (ui->clientsListWidget->currentItem()) {
        // Найкращий спосіб - взяти ID з поточного елемента
        previouslySelectedId = ui->clientsListWidget->currentItem()->data(Qt::UserRole).toInt();
    } else if (m_currentClientId != -1) {
        // Якщо раптом елемента немає, беремо з нашого буфера
        previouslySelectedId = m_currentClientId;
    }

    // 2. (КЛЮЧОВЕ ВИПРАВЛЕННЯ) Блокуємо сигнали, щоб 'clear()'
    //    не викликав 'onClientSelected(nullptr)'
    ui->clientsListWidget->blockSignals(true);

    ui->clientsListWidget->clear();
    QListWidgetItem* itemToSelect = nullptr;

    for (const QJsonValue &value : clients) {
        QJsonObject client = value.toObject();
        int clientId = client["client_id"].toInt();
        QString clientName = client["client_name"].toString();

        QListWidgetItem *item = new QListWidgetItem(clientName);
        item->setData(Qt::UserRole, clientId);
        ui->clientsListWidget->addItem(item);

        if (clientId == previouslySelectedId) {
            itemToSelect = item;
        }
    }

    // 3. (ВИПРАВЛЕННЯ) Розблоковуємо сигнали
    ui->clientsListWidget->blockSignals(false);

    // 4. Встановлюємо виділення.
    if (itemToSelect) {
        // Цей виклик тепер коректно викличе onClientSelected(itemToSelect)
        // і завантажить дані
        ui->clientsListWidget->setCurrentItem(itemToSelect);
    } else if (m_currentClientId != -1) {
        // Якщо ми не знайшли елемент (напр., його перейменували),
        // але ID той самий, просто завантажуємо дані для цього ID
        // (Це рідкісний випадок, але про всяк випадок)
        ApiClient::instance().fetchClientById(m_currentClientId);
    }
    // (Якщо нічого не було вибрано, форма залишиться порожньою, що коректно)
}
void ClientsListDialog::onNewClientClicked()
{
    bool ok;
    QString clientName = QInputDialog::getText(this, "Створення нового клієнта",
                                               "Введіть назву нового клієнта:", QLineEdit::Normal,
                                               "", &ok);
    if (ok && !clientName.trimmed().isEmpty()) {
        QJsonObject newClientData;
        newClientData["client_name"] = clientName.trimmed();
        ApiClient::instance().createClient(newClientData);
    }
}

void ClientsListDialog::onClientCreated(const QJsonObject &newClient)
{
    QString clientName = newClient["client_name"].toString();
    int clientId = newClient["client_id"].toInt();
    QListWidgetItem *item = new QListWidgetItem(clientName);
    item->setData(Qt::UserRole, clientId);
    ui->clientsListWidget->addItem(item);
    ui->clientsListWidget->setCurrentItem(item);
    QMessageBox::information(this, "Успіх", QString("Клієнт '%1' успішно створений.").arg(clientName));
}

// -----------------------------------------------------------------
// !!! ВИПРАВЛЕННЯ "ТРЕШУ" !!!
// (Цей метод агресивно очищує форму)
// -----------------------------------------------------------------
void ClientsListDialog::onClientSelected(QListWidgetItem *current)
{
    ui->groupBoxClientSettings->setVisible(current != nullptr);
    if (!current) {
        m_currentClientId = -1;
        return;
    }

    logDebug() << "Client selected. Clearing all fields first to prevent 'trash' state...";

    // --- ПРИМУСОВЕ ОЧИЩЕННЯ ФОРМИ ---
    ui->checkBoxIsActive->setChecked(false);
    ui->lineEditClientName->clear();
    ui->lineEditMinTermID->clear();
    ui->lineEditMaxTermID->clear();

    ui->lineEditServerName->clear();
    ui->spinBoxPort->setValue(3050);
    ui->lineEditDBFileName->clear();
    ui->lineEditUser->clear();
    ui->lineEditPass->clear();
    ui->lineEditAZSFbPass->clear();

    ui->lineEditImportPathFile->clear();

    ui->lineEditApiKeyPalantir->clear();
    ui->comboBoxTemplateHostname->setCurrentIndex(-1);
    ui->comboBoxSyncMetod->setCurrentIndex(0);

    ui->lineEditVncPath->clear();
    ui->spinBoxVncPort->setValue(5900);
    ui->lineEditVncPassword->clear();
    ui->lineEditIpPrefix->setText("10.");

    // (ДОДАНО) Скидаємо наш буфер "гонки"
    m_pendingIpGenMethodId = -1;

    // "Безпечна синхронізація"
    ui->pushButtonSync->setEnabled(false);
    m_isDirty = true;
    // --- (КІНЕЦЬ ОЧИЩЕННЯ) ---

    // --- Запит нових даних ---
    m_currentClientId = current->data(Qt::UserRole).toInt();
    logDebug() << "Requesting details for client ID:" << m_currentClientId;
    ApiClient::instance().fetchClientById(m_currentClientId);
}

// Файл: clientslistdialog.cpp

void ClientsListDialog::onClientDetailsReceived(const QJsonObject &client)
{
    logDebug() << "Received client details. Populating form...";
    m_currentClientData = client;
    ui->labelTitle->setText(client["client_name"].toString());

    // --- Заповнюємо "Загальні" ---
    ui->checkBoxIsActive->setChecked(client["is_active"].toBool());
    ui->lineEditClientName->setText(client["client_name"].toString());
    ui->lineEditMinTermID->setText(QString::number(client["term_id_min"].toInt()));
    ui->lineEditMaxTermID->setText(QString::number(client["term_id_max"].toInt()));
    ui->lineEditIpPrefix->setText(client["subnet_prefix"].toString());

    // --- Заповнюємо "DIRECT" (Джерело даних) ---
    QJsonObject dbConfig = client["config_direct"].toObject();
    ui->lineEditServerName->setText(dbConfig["db_host"].toString());
    ui->spinBoxPort->setValue(dbConfig["db_port"].toInt(3050));
    ui->lineEditDBFileName->setText(dbConfig["db_path"].toString());
    ui->lineEditUser->setText(dbConfig["db_user"].toString());

    // !!! ВИПРАВЛЕНО: ДЕШИФРУВАННЯ ПАРОЛЯ БД !!!
    QString encryptedDbPass = dbConfig["db_password"].toString();
    if (!encryptedDbPass.isEmpty()) {
        ui->lineEditPass->setText(CriptPass::instance().decriptPass(encryptedDbPass));
    } else {
        ui->lineEditPass->clear();
    }

    // !!! ВИПРАВЛЕНО: ДЕШИФРУВАННЯ ПАРОЛЯ АЗС !!!
    QString encryptedAzsPass = client["gas_station_db_password"].toString();
    if (!encryptedAzsPass.isEmpty()) {
        ui->lineEditAZSFbPass->setText(CriptPass::instance().decriptPass(encryptedAzsPass));
    } else {
        ui->lineEditAZSFbPass->clear();
    }

    // --- Заповнюємо "FILE" ---
    QJsonObject fileConfig = client["config_file"].toObject();
    ui->lineEditImportPathFile->setText(fileConfig["import_path"].toString());

    // --- Заповнюємо "PALANTIR" ---
    QJsonObject palantirConfig = client["config_palantir"].toObject();

    // !!! ВИПРАВЛЕНО: ДЕШИФРУВАННЯ API КЛЮЧА !!!
    QString encryptedApiKey = palantirConfig["api_key"].toString();
    if (!encryptedApiKey.isEmpty()) {
        ui->lineEditApiKeyPalantir->setText(CriptPass::instance().decriptPass(encryptedApiKey));
    } else {
        ui->lineEditApiKeyPalantir->clear();
    }

    // --- ВИРІШЕННЯ "ГОНКИ" ---
    // 1. Зберігаємо ID з КОРЕНЕВОГО об'єкта
    m_pendingIpGenMethodId = client["ip_gen_method_id"].toInt(-1);

    // 2. Намагаємося встановити по ID
    //    findData шукає ID (який ми зберегли у 'data' ролі)
    int index = ui->comboBoxTemplateHostname->findData(m_pendingIpGenMethodId);
    ui->comboBoxTemplateHostname->setCurrentIndex(index);
    // --- КІНЕЦЬ ВИРІШЕННЯ "ГОНКИ" ---

    // --- Встановлюємо метод ---
    ui->comboBoxSyncMetod->setCurrentText(client["sync_method"].toString());
    on_comboBoxSyncMetod_currentIndexChanged(ui->comboBoxSyncMetod->currentIndex());

    // --- Заповнюємо VNC ---
    QJsonObject vncConfig = client["vnc_settings"].toObject();
    ui->lineEditVncPath->setText(vncConfig["vnc_path"].toString());
    ui->spinBoxVncPort->setValue(vncConfig["vnc_port"].toInt(5900));

    QString encryptedVncPass = vncConfig["vnc_password"].toString();
    if (!encryptedVncPass.isEmpty()) {
        ui->lineEditVncPassword->setText(CriptPass::instance().decriptPass(encryptedVncPass));
    } else {
        ui->lineEditVncPassword->clear();
    }

    // "Безпечна синхронізація"
    m_isDirty = false;
    ui->pushButtonSync->setEnabled(true);
}

// -----------------------------------------------------------------
// !!! (НОВЕ) ВИПРАВЛЕННЯ "ГОНКИ" (Частина 2) !!!
// (Тепер працює з ID)
// -----------------------------------------------------------------
void ClientsListDialog::onIpGenMethodsReceived(const QJsonArray &methods)
{
    logDebug() << "Received" << methods.count() << "IP gen methods. Populating combo box...";

    ui->comboBoxTemplateHostname->clear();
    for (const QJsonValue &value : methods) {
        QJsonObject method = value.toObject();
        ui->comboBoxTemplateHostname->addItem(method["method_name"].toString(),
                                              method["method_id"].toInt()); // Зберігаємо ID в 'data'
    }

    // --- (НОВЕ) ВИРІШЕННЯ "ГОНКИ" (Частина 2) ---
    // Якщо onClientDetailsReceived спрацював ПЕРШИМ,
    // ми беремо збережений ID і встановлюємо його ЗАРАЗ.
    if (m_pendingIpGenMethodId != -1) {
        logDebug() << "Applying pending ip_gen_method_id:" << m_pendingIpGenMethodId;
        int index = ui->comboBoxTemplateHostname->findData(m_pendingIpGenMethodId);
        ui->comboBoxTemplateHostname->setCurrentIndex(index);
        m_pendingIpGenMethodId = -1; // Очищуємо буфер
        // Примусово оновлюємо видимість поля префікса після застосування ID
        on_comboBoxTemplateHostname_currentIndexChanged(ui->comboBoxTemplateHostname->currentIndex());
    }
    // --- КІНЕЦЬ НОВОГО ВИРІШЕННЯ ---
}

// (ЗАМІНІТЬ НА ЦЕЙ КОД)
void ClientsListDialog::on_comboBoxSyncMetod_currentIndexChanged(int index)
{
    QString method = ui->comboBoxSyncMetod->currentText();

    // --- (ДОДАНО) Визначаємо, яка група активна ---
    bool isDirect = (method == "DIRECT");
    bool isPalantir = (method == "PALANTIR");
    bool isFile = (method == "FILE");

    // --- (ОНОВЛЕНО) Керуємо всіма трьома групами ---

    // 1. Група "DIRECT" (groupBoxSyncDB)
    //    (Потрібна для 'DIRECT' та 'FILE')
    ui->groupBoxDatabase->setVisible(isDirect || isFile);

    // 2. Група "PALANTIR" (groupBoxSyncAPI)
    //    (Потрібна тільки для 'PALANTIR')
    ui->groupBoxSyncAPI->setVisible(isPalantir);

    // 3. Група "FILE" (groupBoxFileSettings)
    //    (Потрібна тільки для 'FILE')
    ui->groupBoxFileSettings->setVisible(isFile);
    ui->pushButtonGenerateExporter->setVisible(isFile); // Кнопка пов'язана з FILE
}

void ClientsListDialog::on_pushButtonCheckConnections_clicked()
{
    QJsonObject config;
    config["db_host"] = ui->lineEditServerName->text();
    config["db_port"] = ui->spinBoxPort->value();
    config["db_path"] = ui->lineEditDBFileName->text();
    config["db_user"] = ui->lineEditUser->text();
    config["db_password"] = ui->lineEditPass->text();

    ApiClient::instance().testDbConnection(config);
}

QJsonObject ClientsListDialog::formToJson() const
{
    QJsonObject root;
    QString method = ui->comboBoxSyncMetod->currentText();

    // 1. Завжди збираємо ЗАГАЛЬНІ налаштування
    root["client_name"] = ui->lineEditClientName->text();
    root["is_active"] = ui->checkBoxIsActive->isChecked();
    root["term_id_min"] = ui->lineEditMinTermID->text().toInt();
    root["term_id_max"] = ui->lineEditMaxTermID->text().toInt();
    root["sync_method"] = method;
    root["subnet_prefix"] = ui->lineEditIpPrefix->text();

    // (Пароль АЗС - "загальний")
    QString azsPass = ui->lineEditAZSFbPass->text();
    if (!azsPass.isEmpty()) {
        // ✅ ШИФРУВАННЯ ПЕРЕД ВІДПРАВКОЮ
        root["gas_station_db_password"] = CriptPass::instance().criptPass(azsPass);
    }

    // (Шаблон IP - "загальний")
    QVariant ipGenData = ui->comboBoxTemplateHostname->currentData();
    if (ipGenData.isValid()) {
        root["ip_gen_method_id"] = ipGenData.toInt();
    }

    // 2. Збираємо специфічні налаштування ЗАЛЕЖНО ВІД МЕТОДУ

    // --- Налаштування DIRECT ---
    if (method == "DIRECT" || method == "FILE") {
        QJsonObject configDirect;
        configDirect["db_host"] = ui->lineEditServerName->text();
        configDirect["db_port"] = ui->spinBoxPort->value();
        configDirect["db_path"] = ui->lineEditDBFileName->text();
        configDirect["db_user"] = ui->lineEditUser->text();

        // (Пароль БД)
        QString dbPass = ui->lineEditPass->text();
        if (!dbPass.isEmpty()) {
            // ✅ ШИФРУВАННЯ ПЕРЕД ВІДПРАВКОЮ
            configDirect["db_password"] = CriptPass::instance().criptPass(dbPass);
        }
        root["config_direct"] = configDirect;
    }

    // --- Налаштування PALANTIR ---
    if (method == "PALANTIR") {
        QJsonObject configPalantir;

        // (API Key)
        QString apiKey = ui->lineEditApiKeyPalantir->text();
        if (!apiKey.isEmpty()) {
            // ✅ ШИФРУВАННЯ ПЕРЕД ВІДПРАВКОЮ
            configPalantir["api_key"] = CriptPass::instance().criptPass(apiKey);
        }
        root["config_palantir"] = configPalantir;
    }

    // --- Налаштування FILE ---
    // (Не містить паролів, залишаємо без змін)
    if (method == "FILE") {
        QJsonObject configFile;
        configFile["import_path"] = ui->lineEditImportPathFile->text();
        root["config_file"] = configFile;
    }

    // --- Налаштування VNC ---
    QJsonObject configVnc;
    configVnc["vnc_path"] = ui->lineEditVncPath->text();
    configVnc["vnc_port"] = ui->spinBoxVncPort->value();

    QString vncPass = ui->lineEditVncPassword->text();
    if (!vncPass.isEmpty()) {
        configVnc["vnc_password"] = CriptPass::instance().criptPass(vncPass);
    } else {
        configVnc["vnc_password"] = "";
    }
    root["vnc_settings"] = configVnc;

    return root;
}

void ClientsListDialog::on_buttonBox_accepted()
{
    logDebug() << "Save button clicked. Current Client ID is:" << m_currentClientId;
    if (m_currentClientId == -1) return;

    QJsonObject clientData = formToJson();

    ApiClient::instance().updateClient(m_currentClientId, clientData);
    ui->groupBoxClientSettings->setVisible(false);
}


void ClientsListDialog::on_buttonBox_rejected()
{
    this->reject();
}

void ClientsListDialog::onSyncButtonClicked()
{
    if (m_currentClientId != -1) {
        logDebug() << "Sync button clicked for client ID:" << m_currentClientId;
        ui->pushButtonSync->setEnabled(false);
        ui->pushButtonSync->setText("Синхронізація...");

        ApiClient::instance().syncClientObjects(m_currentClientId);
    }
}


void ClientsListDialog::onSyncButtonToggled(bool checked)
{
    if (checked) {
        if (m_currentClientId == -1) {
            ui->pushButtonSync->setChecked(false);
            return;
        }
        ui->pushButtonSync->setText("Синхронізація...");
        ui->pushButtonSync->setEnabled(false);
        ApiClient::instance().syncClientObjects(m_currentClientId);
    } else {
        // (Логіка для зупинки?)
    }
}

void ClientsListDialog::onSyncRequestAcknowledged(int clientId, bool success, const ApiError& details)
{
    if (success) {
        m_syncingClientId = clientId;
        ui->pushButtonSync->setEnabled(true);
        m_syncStatusTimer->start(2000);
    } else {
        QMessageBox::critical(this, "Помилка запуску", details.errorString);
        ui->pushButtonSync->setChecked(false);
        ui->pushButtonSync->setText("Синхронізувати");
        ui->pushButtonSync->setEnabled(true);
    }
}

void ClientsListDialog::checkSyncStatus()
{
    if (m_syncingClientId != -1) {
        ApiClient::instance().fetchSyncStatus(m_syncingClientId);
    }
}

void ClientsListDialog::onSyncStatusReceived(int clientId, const QJsonObject& status)
{
    if (clientId != m_syncingClientId) return;

    QString currentStatus = status["status"].toString();
    if (currentStatus == "SUCCESS") {
        m_syncStatusTimer->stop();
        m_syncingClientId = -1;
        ui->pushButtonSync->setChecked(false);
        ui->pushButtonSync->setText("Синхронізувати");
        QMessageBox::information(this, "Успіх", "Синхронізація успішно завершена.\n" + status["message"].toString());
    } else if (currentStatus == "FAILED") {
        m_syncStatusTimer->stop();
        m_syncingClientId = -1;
        ui->pushButtonSync->setChecked(false);
        ui->pushButtonSync->setText("Синхронізувати");
        QMessageBox::critical(this, "Помилка", "Синхронізація не вдалася:\n" + status["message"].toString());
    }
}


// --- "ФАБРИКА КОНФІГУРАЦІЙ" ---

void ClientsListDialog::on_pushButtonGenerateExporter_clicked()
{
    if (!m_exportTasks.isEmpty()) {
        generateExporterPackage(m_exportTasks);
    } else {
        logInfo() << "Fetching export tasks from server...";
        ui->pushButtonGenerateExporter->setEnabled(false);
        ui->pushButtonGenerateExporter->setText("Завантаження шаблонів...");
        ApiClient::instance().fetchExportTasks();
    }
}

void ClientsListDialog::onExportTasksFetched(const QJsonArray& tasks)
{
    logInfo() << "Successfully fetched" << tasks.count() << "export tasks.";
    m_exportTasks = tasks;
    ui->pushButtonGenerateExporter->setEnabled(true);
    ui->pushButtonGenerateExporter->setText("💾 Згенерувати пакет Експортера");
    generateExporterPackage(m_exportTasks);
}

void ClientsListDialog::onExportTasksFetchFailed(const ApiError& error)
{
    logCritical() << "Failed to fetch export tasks:" << error.errorString;
    QMessageBox::critical(this, "Помилка завантаження",
                          "Не вдалося завантажити шаблони завдань з сервера.\n" + error.errorString);
    ui->pushButtonGenerateExporter->setEnabled(true);
    ui->pushButtonGenerateExporter->setText("💾 Згенерувати пакет Експортера");
}

QJsonObject ClientsListDialog::gatherClientDataForConfig()
{
    QJsonObject dbConfig;
    dbConfig["host"] = ui->lineEditServerName->text();
    dbConfig["port"] = ui->spinBoxPort->value();
    dbConfig["path"] = ui->lineEditDBFileName->text();
    dbConfig["user"] = ui->lineEditUser->text();

    if (dbConfig["host"].toString().isEmpty() || dbConfig["path"].toString().isEmpty() || ui->lineEditPass->text().isEmpty()) {
        logWarning() << "DB config (host, path or password) is empty. Reading from UI fields.";
        return QJsonObject();
    }

    dbConfig["password"] = CriptPass::instance().criptPass(ui->lineEditPass->text());

    QJsonObject params;
    params["minTermId"] = ui->lineEditMinTermID->text().toInt();
    params["maxTermId"] = ui->lineEditMaxTermID->text().toInt();

    QJsonObject config;
    config["source_db"] = dbConfig;
    config["params"] = params;
    config["embed_client_id"] = m_currentClientId;

    // (ДОДАНО) Використовуємо поле ExportPath
    QString outputDir;
    if (outputDir.isEmpty()) {
        outputDir = "."; // Поточна папка, якщо нічого не вказано
    }
    // Формуємо ім'я .zip
    QString zipName = QString("%1_import_package.zip").arg(m_currentClientId);
    config["output_package_path"] = outputDir + "/" + zipName;


    return config;
}

void ClientsListDialog::generateExporterPackage(const QJsonArray& tasks)
{
    // Вибір папки користувачем
    QString saveDir = QFileDialog::getExistingDirectory(this, "Виберіть папку для збереження пакета Експортера");
    if (saveDir.isEmpty()) {
        return;
    }

    // --- 1. Створюємо ТИМЧАСОВУ ПАПКУ всередині обраної папки ---
    // Це гарантує, що у нас є права на запис і диск існує
    QString tempDirName = "temp_export_pack_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    QDir destinationDir(saveDir);

    if (!destinationDir.mkdir(tempDirName)) {
        QMessageBox::critical(this, "Помилка", "Не вдалося створити тимчасову папку в: " + saveDir);
        return;
    }

    // Повний шлях до нашої робочої папки
    QString tempDirPath = saveDir + "/" + tempDirName;

    // Отримуємо конфігураційний об'єкт
    QJsonObject config = gatherClientDataForConfig();
    int clientId = config["embed_client_id"].toInt();

    // Імена файлів
    QString finalZipName = QString("%1_import_package.zip").arg(clientId);
    QString configFilename = QString("%1_config.json").arg(clientId);
    QStringList filesToZip;

    QJsonArray tasksArray;

    // 2. Генеруємо файли .json та .sql у НАШІЙ тимчасовій папці
    for (const QJsonValue& value : tasks) {
        QJsonObject task = value.toObject();

        if (task["is_active"].toInt() != 1) {
            continue;
        }

        QString queryFilename = task["query_filename"].toString();
        if (queryFilename.isEmpty()) {
            queryFilename = QString("query_%1.sql").arg(task["task_name"].toString().toLower().simplified().replace(" ", ""));
        }

        // 2a. Зберігаємо .sql файл (використовуємо tempDirPath)
        QFile sqlFile(tempDirPath + "/" + queryFilename);
        if (sqlFile.open(QIODevice::WriteOnly)) {
            QString sqlTemplate = task["sql_template"].toString();
            sqlFile.write(sqlTemplate.toUtf8());
            sqlFile.close();
            filesToZip.append(queryFilename);
        } else {
            QMessageBox::critical(this, "Помилка", "Не вдалося записати файл: " + sqlFile.fileName());
            // Очистка перед виходом
            QDir(tempDirPath).removeRecursively();
            return;
        }

        // 2b. Додаємо завдання в конфіг
        QJsonObject taskConfigEntry;
        taskConfigEntry["task_name"] = task["task_name"];
        taskConfigEntry["query_file"] = queryFilename;

        QString outputJsonName = queryFilename;
        outputJsonName.replace(".sql", ".json", Qt::CaseInsensitive);

        taskConfigEntry["output_file"] = outputJsonName;
        taskConfigEntry["target_table"] = task["target_table"].toString();
        taskConfigEntry["embed_client_id"] = config["embed_client_id"];
        taskConfigEntry["params"] = config["params"];
        tasksArray.append(taskConfigEntry);
    }
    config["tasks"] = tasksArray;

    // 3. Зберігаємо фінальний конфіг .json
    QFile configFile(tempDirPath + "/" + configFilename);
    if (configFile.open(QIODevice::WriteOnly)) {
        configFile.write(QJsonDocument(config).toJson(QJsonDocument::Indented));
        configFile.close();
        filesToZip.append(configFilename);
    } else {
        QMessageBox::critical(this, "Помилка", "Не вдалося записати конфігураційний файл.");
        QDir(tempDirPath).removeRecursively();
        return;
    }

    // 4. АРХІВАЦІЯ (7z)
    QProcess zipper;
    zipper.setWorkingDirectory(tempDirPath);

    QStringList args;
    args << "a"           // Add
         << "-tzip"       // Type ZIP
         << finalZipName; // Output filename

    args.append(filesToZip);

    // --- ПОЧАТОК ЗМІН: РОЗУМНИЙ ПОШУК 7-ZIP ---
    QString sevenZipExe = "7z"; // Значення за замовчуванням (через PATH)

    // 1. Пріоритет: Шукаємо 7z.exe прямо біля нашої програми (Gandalf.exe)
    // Це найнадійніший варіант для сервера.
    QString local7z = QCoreApplication::applicationDirPath() + "/7z.exe";

    // 2. Пріоритет: Стандартний шлях установки 7-Zip x64
    QString system7z = "C:/Program Files/7-Zip/7z.exe";

    // 3. Пріоритет: Стандартний шлях установки 7-Zip x86 (рідко, але буває)
    QString system7z86 = "C:/Program Files (x86)/7-Zip/7z.exe";

    if (QFile::exists(local7z)) {
        sevenZipExe = local7z;
        logDebug() << "Archiver found locally:" << sevenZipExe;
    }
    else if (QFile::exists(system7z)) {
        sevenZipExe = system7z;
        logDebug() << "Archiver found in Program Files:" << sevenZipExe;
    }
    else if (QFile::exists(system7z86)) {
        sevenZipExe = system7z86;
        logDebug() << "Archiver found in Program Files (x86):" << sevenZipExe;
    }
    else {
        logWarning() << "7z.exe not found in standard paths. Trying system PATH...";
    }
    // --- КІНЕЦЬ ЗМІН ---

    logDebug() << "Running 7-Zip command in" << tempDirPath << ":" << sevenZipExe << args.join(" ");

    // Запускаємо знайдений шлях
    zipper.start(sevenZipExe, args);

    if (!zipper.waitForFinished(60000) || zipper.exitCode() != 0) {
        QString error = zipper.exitCode() != 0 ? zipper.readAllStandardError() : "Таймаут або невдалий запуск.";
        logCritical() << "7-Zip failed:" << error;
        QMessageBox::critical(this, "Помилка 7-Zip",
                              "Не вдалося створити архів.\n"
                              "Переконайтеся, що файл 7z.exe знаходиться у папці з програмою.\n\n"
                              "Деталі: " + error);

        // Очистка
        QDir(tempDirPath).removeRecursively();
        return;
    }

    // 5. Переміщуємо готовий ZIP з тимчасової папки в основну (рівнем вище)
    QString sourceZipPath = tempDirPath + "/" + finalZipName;
    QString finalDestinationPath = saveDir + "/" + finalZipName;

    // Видаляємо старий файл, якщо він там вже є
    if (QFile::exists(finalDestinationPath)) {
        QFile::remove(finalDestinationPath);
    }

    bool moveSuccess = QFile::rename(sourceZipPath, finalDestinationPath);

    // 6. ВИДАЛЯЄМО ТИМЧАСОВУ ПАПКУ (Cleanup)
    // Оскільки ми створили її вручну, треба і видалити вручну
    QDir(tempDirPath).removeRecursively();

    if (!moveSuccess) {
        QMessageBox::critical(this, "Помилка", "Не вдалося перемістити архів у: " + finalDestinationPath);
        return;
    }

    QMessageBox::information(this, "Успіх",
                             QString("Пакет Експортера успішно створено:\n%1").arg(finalDestinationPath));
}

void ClientsListDialog::onFieldChanged()
{
    m_isDirty = true;
    ui->pushButtonSync->setEnabled(false); // Вимикаємо синхронізацію, доки не збережено
}

void ClientsListDialog::on_toolButtonBrowseImport_clicked()
{
    // 1. Беремо поточний шлях з поля (якщо там щось є)
    QString currentPath = ui->lineEditImportPathFile->text();

    // Якщо пусто, відкриваємо домашню папку або диск C
    if (currentPath.isEmpty()) {
        currentPath = QDir::homePath();
    }

    // 2. Відкриваємо діалог вибору папки
    QString dir = QFileDialog::getExistingDirectory(this,
                                                    "Виберіть папку для імпорту (Inbox)",
                                                    currentPath,
                                                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    // 3. Якщо користувач щось вибрав (не натиснув Cancel)
    if (!dir.isEmpty()) {
        ui->lineEditImportPathFile->setText(dir);

        // Викликаємо збереження стану (щоб активувалася кнопка Зберегти, якщо треба)
        onFieldChanged();
    }
}

void ClientsListDialog::on_toolButtonBrowseVncPath_clicked()
{
    QString currentPath = ui->lineEditVncPath->text();
    if (currentPath.isEmpty()) {
        // Якщо пусто, пропонуємо стандартну папку Program Files
        currentPath = "C:/Program Files";
    }

    QString exeFile = QFileDialog::getOpenFileName(this,
                                                   "Виберіть виконуваний файл VNC клієнта",
                                                   currentPath,
                                                   "Виконувані файли (*.exe);;Всі файли (*.*)");

    if (!exeFile.isEmpty()) {
        ui->lineEditVncPath->setText(exeFile);
        onFieldChanged(); // Активуємо кнопку "Зберегти"
    }
}

void ClientsListDialog::on_comboBoxTemplateHostname_currentIndexChanged(int index)
{
    Q_UNUSED(index);

    // Отримуємо назву обраного методу
    QString method = ui->comboBoxTemplateHostname->currentText();

    // Перевіряємо, чи це метод DATABASE
    // (перевірте, щоб рядок точно збігався з тим, що у вас в базі)
    bool isDatabase = method.contains("DATABASE", Qt::CaseInsensitive);

    // Ховаємо або показуємо елементи
    ui->lineEditIpPrefix->setVisible(isDatabase);
    ui->labelIpPrefix->setVisible(isDatabase);

    // Активуємо кнопку "Зберегти"
    onFieldChanged();
}
