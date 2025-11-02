#include "userlistdialog.h"
#include "ui_userlistdialog.h"
#include "Oracle/ApiClient.h"
#include "Oracle/Logger.h"
#include "Oracle/SessionManager.h"
#include "Oracle/User.h"
#include "usereditdialog.h"

#include <QStandardItemModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QInputDialog>

UserListDialog::UserListDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::UserListDialog)
{
    ui->setupUi(this);
    setWindowTitle("Адміністрування користувачів");

    createModel();
    createUI();
    createConnections();

    // --- ОНОВЛЕНО ---
    // Запускаємо завантаження даних для обох вкладок
    ApiClient::instance().fetchAllUsers();
    ApiClient::instance().fetchBotRequests(); // <-- ДОДАНО
}

UserListDialog::~UserListDialog()
{
    delete ui;
}

void UserListDialog::createUI()
{
    // --- Налаштування для вкладки "Всі користувачі" ---
    ui->tableViewUsers->setModel(m_model);
    ui->tableViewUsers->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewUsers->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableViewUsers->setColumnHidden(0, true); // Ховаємо колонку з ID
    ui->tableViewUsers->verticalHeader()->hide();
    ui->tableViewUsers->horizontalHeader()->setStretchLastSection(true);

    // --- ДОДАНО: Налаштування для вкладки "Запити від бота" ---
    ui->tableViewRequests->setModel(m_requestsModel);
    ui->tableViewRequests->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewRequests->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableViewRequests->setColumnHidden(0, true); // Також ховаємо колонку з REQUEST_ID
    ui->tableViewRequests->verticalHeader()->hide();
    ui->tableViewRequests->horizontalHeader()->setStretchLastSection(true);
}

void UserListDialog::createModel()
{
    // --- Модель для вкладки "Всі користувачі" ---
    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({"ID", "Логін", "ПІБ", "Активний"});

    // --- ДОДАНО: Модель для вкладки "Запити від бота" ---
    m_requestsModel = new QStandardItemModel(this);
    // ID буде в першій колонці (але ми її приховаємо)
    m_requestsModel->setHorizontalHeaderLabels({"ID", "Login (Telegram)", "ПІБ"});
}

void UserListDialog::createConnections()
{
    // --- З'єднання для вкладки "Всі користувачі" ---
    connect(&ApiClient::instance(), &ApiClient::usersFetched, this, &UserListDialog::onUsersDataReceived);
    connect(&ApiClient::instance(), &ApiClient::usersFetchFailed, this, [&](const ApiError& error) {
        QMessageBox::critical(this, "Помилка завантаження користувачів", error.errorString);
    });

    // Виправляємо підключення слота, щоб воно відповідало об'єкту у файлі .ui
    connect(ui->tableViewUsers, &QTableView::doubleClicked, this, &UserListDialog::on_tableViewUsers_doubleClicked);

    // --- ДОДАНО: З'єднання для вкладки "Запити від бота" ---
    connect(&ApiClient::instance(), &ApiClient::botRequestsFetched, this, &UserListDialog::onBotRequestsFetched);
    connect(&ApiClient::instance(), &ApiClient::botRequestsFetchFailed, this, &UserListDialog::onBotRequestsFetchFailed);

    connect(&ApiClient::instance(), &ApiClient::botRequestRejected, this, &UserListDialog::onBotRequestRejected);
    connect(&ApiClient::instance(), &ApiClient::botRequestRejectFailed, this, &UserListDialog::onBotRequestRejectFailed);

    connect(&ApiClient::instance(), &ApiClient::botRequestApproved, this, &UserListDialog::onBotRequestApproved);
    connect(&ApiClient::instance(), &ApiClient::botRequestApproveFailed, this, &UserListDialog::onBotRequestApproveFailed);

    connect(&ApiClient::instance(), &ApiClient::botRequestLinked, this, &UserListDialog::onBotRequestLinked);
    connect(&ApiClient::instance(), &ApiClient::botRequestLinkFailed, this, &UserListDialog::onBotRequestLinkFailed);
}

void UserListDialog::onUsersDataReceived(const QJsonArray &users)
{
    m_model->removeRows(0, m_model->rowCount());
    for (const QJsonValue &value : users) {
        QJsonObject userObj = value.toObject();
        QList<QStandardItem *> rowItems;
        rowItems.append(new QStandardItem(QString::number(userObj["user_id"].toInt())));
        rowItems.append(new QStandardItem(userObj["login"].toString()));
        rowItems.append(new QStandardItem(userObj["fio"].toString()));
        QStandardItem* activeItem = new QStandardItem(userObj["is_active"].toBool() ? "Так" : "Ні");
        rowItems.append(activeItem);
        m_model->appendRow(rowItems);
    }
    ui->tableViewUsers->resizeColumnsToContents();
}

// --- ДОДАНО НОВІ МЕТОДИ ---

void UserListDialog::onBotRequestsFetched(const QJsonArray &requests)
{
    m_requestsModel->removeRows(0, m_requestsModel->rowCount()); // Очищуємо модель запитів

    for (const QJsonValue& value : requests) {
        QJsonObject obj = value.toObject();

        // --- ОНОВЛЕНО ---
        // Читаємо request_id замість user_id
        int requestId = obj["request_id"].toInt();
        QString login = obj["login"].toString();
        QString fio = obj["fio"].toString();

        // Створюємо елементи для рядка
        auto idItem = new QStandardItem(QString::number(requestId));
        auto loginItem = new QStandardItem(login);
        auto fioItem = new QStandardItem(fio);

        // Зберігаємо requestId в даних першого елемента.
        // Це КЛЮЧОВИЙ момент для майбутньої обробки.
        idItem->setData(requestId, Qt::UserRole);

        m_requestsModel->appendRow({idItem, loginItem, fioItem});
    }
    ui->tableViewRequests->resizeColumnsToContents();
}

void UserListDialog::onBotRequestsFetchFailed(const ApiError &error)
{
    QMessageBox::critical(this, "Помилка", "Не вдалося завантажити список запитів:\n" + error.errorString);
}

// Слот для кнопки "Оновити"
void UserListDialog::on_pushButtonrefreshRequests_clicked()
{
    logInfo() << "Refreshing bot requests list...";
    ApiClient::instance().fetchBotRequests();
}

// --- КІНЕЦЬ НОВИХ МЕТОДІВ ---

void UserListDialog::on_buttonBox_rejected()
{
    this->reject();
}

// Виправлено ім'я методу, щоб воно відповідало імені об'єкта `tableViewUsers`
void UserListDialog::on_tableViewUsers_doubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;

    const User* currentUser = SessionManager::instance().currentUser();
    if (!currentUser) return;

    int targetUserId = m_model->item(index.row(), 0)->text().toInt();

    bool canOpenEditor = currentUser->isAdmin() ||
                         currentUser->hasRole("Менеджер") ||
                         (currentUser->id() == targetUserId);

    if (canOpenEditor) {
        logDebug() << "Opening edit dialog for user ID:" << targetUserId;

        // --- ПОЧАТОК ЗМІН ---
        // 1. Створюємо діалог на "купі" (heap) за допомогою 'new', а не на стеку.
        UserEditDialog* dlg = new UserEditDialog(targetUserId, this);

        // 2. Встановлюємо атрибут, щоб Qt автоматично видалив діалог після закриття.
        // Це запобігає витокам пам'яті.
        dlg->setAttribute(Qt::WA_DeleteOnClose);

        connect(dlg, &UserEditDialog::accepted, this, [this]() {
            logInfo() << "User edit dialog accepted, refreshing user list.";
            ApiClient::instance().fetchAllUsers(); // Оновлюємо список
        });
        dlg->open();
    } else {
        logWarning() << "ACCESS DENIED: User" << currentUser->login()
        << "tried to open editor for user ID" << targetUserId;
        QMessageBox::warning(this, "Доступ заборонено", "У вас недостатньо прав для редагування цього користувача.");
    }
}


void UserListDialog::on_pushButtonRejectBot_clicked()
{
    // 1. Отримуємо поточний вибраний рядок
    QModelIndexList selectedRows = ui->tableViewRequests->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) {
        QMessageBox::warning(this, "Не вибрано", "Будь ласка, виберіть запит для відхилення.");
        return;
    }

    // 2. Отримуємо ID запиту, який ми зберегли в моделі
    // Беремо індекс першої колонки (0) вибраного рядка
    QModelIndex idIndex = selectedRows.first().siblingAtColumn(0);
    int requestId = m_requestsModel->data(idIndex, Qt::UserRole).toInt();

    if (requestId <= 0) {
        logCritical() << "Could not get valid request_id from table view.";
        QMessageBox::critical(this, "Помилка", "Не вдалося отримати ID запиту з таблиці.");
        return;
    }

    // 3. Підтвердження дії
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Підтвердження відхилення",
                                  "Ви впевнені, що хочете відхилити цей запит?",
                                  QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::No) {
        return;
    }

    // 4. Викликаємо ApiClient
    logInfo() << "Attempting to reject bot request ID:" << requestId;
    ApiClient::instance().rejectBotRequest(requestId);
}

// Цей слот спрацює, коли сервер успішно відхилить запит
void UserListDialog::onBotRequestRejected(int requestId)
{
    logInfo() << "Request ID" << requestId << "successfully rejected.";
    QMessageBox::information(this, "Успіх", "Запит було успішно відхилено.");

    // Оновлюємо список, щоб відхилений запит зник
    ApiClient::instance().fetchBotRequests();
}

// Цей слот спрацює, якщо сервер поверне помилку
void UserListDialog::onBotRequestRejectFailed(const ApiError &error)
{
    logCritical() << "Failed to reject request:" << error.errorString;
    QMessageBox::critical(this, "Помилка", "Не вдалося відхилити запит:\n" + error.errorString);
}


// --- ДОДАНО НОВІ МЕТОДИ ДЛЯ СХВАЛЕННЯ ---

void UserListDialog::on_pushButtonApproveBot_clicked()
{
    // 1. Отримуємо поточний вибраний рядок
    QModelIndexList selectedRows = ui->tableViewRequests->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) {
        QMessageBox::warning(this, "Не вибрано", "Будь ласка, виберіть запит для схвалення.");
        return;
    }

    // 2. Отримуємо ID запиту з моделі
    QModelIndex idIndex = selectedRows.first().siblingAtColumn(0);
    int requestId = m_requestsModel->data(idIndex, Qt::UserRole).toInt();

    // 3. Отримуємо ПІБ з моделі, щоб показати його у вікні
    QString fio = m_requestsModel->item(selectedRows.first().row(), 2)->text();

    if (requestId <= 0) {
        logCritical() << "Could not get valid request_id from table view.";
        QMessageBox::critical(this, "Помилка", "Не вдалося отримати ID запиту з таблиці.");
        return;
    }

    // 4. --- КЛЮЧОВИЙ КРОК ---
    // Запитуємо у адміністратора корпоративний логін
    bool ok;
    QString login = QInputDialog::getText(this, "Створення користувача",
                                          QString("Введіть корпоративний логін для:\n%1").arg(fio),
                                          QLineEdit::Normal,
                                          "", &ok);

    // 5. Перевіряємо, чи користувач ввів логін і натиснув "ОК"
    if (ok && !login.trimmed().isEmpty()) {
        QString corporateLogin = login.trimmed();

        // 6. Підтвердження
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Підтвердження дії",
                                      QString("Створити або прив'язати користувача з логіном '%1'?").arg(corporateLogin),
                                      QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) {
            return;
        }

        // 7. Викликаємо ApiClient
        logInfo() << "Attempting to approve request ID" << requestId << "with new login" << corporateLogin;
        ApiClient::instance().approveBotRequest(requestId, corporateLogin);
    }
}

// Цей слот спрацює, коли сервер успішно схвалить запит
void UserListDialog::onBotRequestApproved(int requestId)
{
    logInfo() << "Request ID" << requestId << "successfully approved.";
    QMessageBox::information(this, "Успіх", "Запит було успішно схвалено і нового користувача створено/прив'язано.");

    // --- ВАЖЛИВО ---
    // Оновлюємо ОБИДВА списки:
    ApiClient::instance().fetchBotRequests(); // Запит має зникнути звідси
    ApiClient::instance().fetchAllUsers();    // Користувач має з'явитися тут
}

// Цей слот спрацює, якщо сервер поверне помилку
void UserListDialog::onBotRequestApproveFailed(const ApiError &error)
{
    logCritical() << "Failed to approve request:" << error.errorString;
    QMessageBox::critical(this, "Помилка", "Не вдалося схвалити запит:\n" + error.errorString);
}

// --- ДОДАНО НОВІ МЕТОДИ ДЛЯ ПРИВ'ЯЗКИ ---

// Слот для кнопки 'pushButtonLinkBot'
void UserListDialog::on_pushButtonLinkBot_clicked()
{
    // 1. Отримуємо поточний вибраний запит
    QModelIndexList selectedRequests = ui->tableViewRequests->selectionModel()->selectedRows();
    if (selectedRequests.isEmpty()) {
        QMessageBox::warning(this, "Не вибрано", "Будь ласка, виберіть запит для прив'язки.");
        return;
    }

    // 2. Отримуємо ID запиту з моделі
    QModelIndex requestIndex = selectedRequests.first();
    int requestId = m_requestsModel->data(requestIndex.siblingAtColumn(0), Qt::UserRole).toInt();
    QString requestFio = m_requestsModel->item(requestIndex.row(), 2)->text();

    if (requestId <= 0) {
        logCritical() << "Could not get valid request_id from table view.";
        return;
    }

    // 3. --- КЛЮЧОВИЙ КРОК: Готуємо список користувачів для вибору ---
    // Ми візьмемо дані прямо з моделі 'm_model' (з першої вкладки)

    QStringList userDisplayList; // Список для QInputDialog: "ПІБ (логін)"
    QList<int> userIdList;       // Список ID, що відповідає userDisplayList

    for (int i = 0; i < m_model->rowCount(); ++i) {
        // Отримуємо ID, ПІБ та логін з моделі всіх користувачів
        int userId = m_model->item(i, 0)->text().toInt();
        QString login = m_model->item(i, 1)->text();
        QString fio = m_model->item(i, 2)->text();

        // Формуємо рядок для відображення
        userDisplayList.append(QString("%1 (%2)").arg(fio, login));
        // Зберігаємо ID
        userIdList.append(userId);
    }

    if (userDisplayList.isEmpty()) {
        QMessageBox::critical(this, "Помилка", "Список користувачів порожній. Неможливо ні до кого прив'язати запит.");
        return;
    }

    // 4. Показуємо діалог QInputDialog::getItem
    bool ok;
    QString selectedUserString = QInputDialog::getItem(this, "Вибір користувача",
                                                       QString("Виберіть існуючого користувача, до якого потрібно прив'язати запит від\n%1:").arg(requestFio),
                                                       userDisplayList, 0, false, &ok);

    // 5. Обробляємо вибір
    if (ok && !selectedUserString.isEmpty()) {
        // Знаходимо індекс вибраного рядка
        int selectedIndex = userDisplayList.indexOf(selectedUserString);
        if (selectedIndex == -1) {
            logCritical() << "Could not find index for selected user string.";
            return;
        }

        // Використовуємо цей індекс, щоб отримати реальний ID користувача
        int selectedUserId = userIdList.at(selectedIndex);

        // 6. Підтвердження та виклик API
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Підтвердження прив'язки",
                                      QString("Прив'язати цей запит до користувача\n%1?").arg(selectedUserString),
                                      QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            logInfo() << "Attempting to link request ID" << requestId << "to existing user ID" << selectedUserId;
            ApiClient::instance().linkBotRequest(requestId, selectedUserId);
        }
    }
}

// Цей слот спрацює, коли сервер успішно прив'яже запит
void UserListDialog::onBotRequestLinked(int requestId)
{
    logInfo() << "Request ID" << requestId << "successfully linked.";
    QMessageBox::information(this, "Успіх", "Запит було успішно прив'язано до існуючого користувача.");

    // Оновлюємо обидва списки
    ApiClient::instance().fetchBotRequests(); // Запит має зникнути
    ApiClient::instance().fetchAllUsers();    // У користувача має з'явитися прив'язка
}

// Цей слот спрацює, якщо сервер поверне помилку
void UserListDialog::onBotRequestLinkFailed(const ApiError &error)
{
    logCritical() << "Failed to link request:" << error.errorString;
    QMessageBox::critical(this, "Помилка", "Не вдалося прив'язати запит:\n" + error.errorString);
}
