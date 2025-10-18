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

UserListDialog::UserListDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::UserListDialog)
{
    ui->setupUi(this);
    setWindowTitle("Адміністрування користувачів"); // Встановимо більш відповідний заголовок

    createModel();
    createUI();
    createConnections(); // Важливо викликати до запиту даних

    // Запускаємо завантаження даних
    ApiClient::instance().fetchAllUsers();
}

UserListDialog::~UserListDialog()
{
    delete ui;
}

void UserListDialog::createUI()
{
    ui->tableView->setModel(m_model);
    // Налаштовуємо вигляд таблиці
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableView->setColumnHidden(0, true); // Ховаємо колонку з ID
}

void UserListDialog::createModel()
{
    m_model = new QStandardItemModel(this);
    // Встановлюємо заголовки колонок
    m_model->setHorizontalHeaderLabels({"ID", "Логін", "ПІБ", "Активний"});
}

void UserListDialog::createConnections()
{
    connect(&ApiClient::instance(), &ApiClient::usersFetched, this, &UserListDialog::onUsersDataReceived);

    // === АДАПТОВАНО ТУТ ===
    connect(&ApiClient::instance(), &ApiClient::usersFetchFailed, this, [&](const ApiError& error) {
        QMessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText("Не вдалося завантажити список користувачів.");
        msgBox.setInformativeText(error.errorString);
        msgBox.setDetailedText(QString("URL: %1\nHTTP Status: %2")
                                   .arg(error.requestUrl)
                                   .arg(error.httpStatusCode));
        msgBox.exec();
    });
}

void UserListDialog::onUsersDataReceived(const QJsonArray &users)
{
    m_model->removeRows(0, m_model->rowCount()); // Очищуємо модель

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
    ui->tableView->resizeColumnsToContents(); // Автоматично підбираємо ширину колонок
}

void UserListDialog::on_buttonBox_rejected()
{
    this->reject();
}

void UserListDialog::on_tableView_doubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;

    // 1. Отримуємо поточного користувача
    const User* currentUser = SessionManager::instance().currentUser();
    if (!currentUser) return;

    // 2. Отримуємо ID користувача, по якому клікнули
    int targetUserId = m_model->item(index.row(), 0)->text().toInt();

    // 3. Застосовуємо логіку прав
    bool canOpenEditor = currentUser->isAdmin() ||
                         currentUser->hasRole("Менеджер") ||
                         (currentUser->id() == targetUserId);

    if (canOpenEditor) {
        logDebug() << "Opening edit dialog for user ID:" << targetUserId;
        UserEditDialog dlg(targetUserId, this);
        if (dlg.exec() == QDialog::Accepted) {
            // Оновлюємо список після успішного збереження
            ApiClient::instance().fetchAllUsers();
        }
    } else {
        logWarning() << "ACCESS DENIED: User" << currentUser->login()
        << "tried to open editor for user ID" << targetUserId;
        QMessageBox::warning(this, "Доступ заборонено", "У вас недостатньо прав для редагування цього користувача.");
    }
}
