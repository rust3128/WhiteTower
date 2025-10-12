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
    createModel();

    ApiClient::instance().fetchAllUsers();

    createConnections();
    createUI();
}

UserListDialog::~UserListDialog()
{
    delete ui;
}

void UserListDialog::createUI()
{
    ui->tableView->setModel(m_model);
}

void UserListDialog::createModel()
{
    m_model = new QStandardItemModel(this);
}

void UserListDialog::createConnections()
{
    connect(&ApiClient::instance(), &ApiClient::usersFetched, this, &UserListDialog::onUsersDataReceived);
    connect(&ApiClient::instance(), &ApiClient::usersFetchFailed, this, [&](const QString& error){
        QMessageBox::critical(this, "Помилка", "Не вдалося завантажити список користувачів:\n" + error);
    });
}


void UserListDialog::onUsersDataReceived(const QJsonArray& users)
{
    // Очищуємо стару модель і встановлюємо заголовки
    m_model->clear();
    m_model->setHorizontalHeaderLabels({"ID", "Логін", "ПІБ", "Активний"});

    // 5. Заповнюємо модель даними з JSON
    for (const QJsonValue& value : users) {
        QJsonObject userObj = value.toObject();
        QList<QStandardItem*> rowItems;
        rowItems << new QStandardItem(QString::number(userObj["user_id"].toInt()));
        rowItems << new QStandardItem(userObj["login"].toString());
        rowItems << new QStandardItem(userObj["fio"].toString());
        rowItems << new QStandardItem(userObj["is_active"].toBool() ? "Так" : "Ні");
        m_model->appendRow(rowItems);
    }
    ui->tableView->resizeColumnsToContents();
}
void UserListDialog::on_buttonBox_rejected()
{
    this->reject();
}


void UserListDialog::on_tableView_doubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;

    // --- ПЕРЕВІРКА ПРАВ ДОСТУПУ (ВАША ІДЕЯ) ---

    // 1. Отримуємо поточного користувача
    const User* currentUser = SessionManager::instance().currentUser();
    if (!currentUser) return; // Про всяк випадок

    // 2. Отримуємо ID користувача, по якому клікнули
    int targetUserId = m_model->item(index.row(), 0)->text().toInt();

    // 3. Застосовуємо логіку прав
    bool canOpenEditor = currentUser->isAdmin() ||
                         currentUser->hasRole("Менеджер") ||
                         (currentUser->id() == targetUserId);

    if (canOpenEditor) {
        // Якщо права є, відкриваємо вікно
        logDebug() << "Opening edit dialog for user ID:" << targetUserId;
        UserEditDialog dlg(targetUserId, this);

        // Відкриваємо вікно і, якщо користувач зберіг зміни (натиснув ОК), оновлюємо список
        if (dlg.exec() == QDialog::Accepted) {
            ApiClient::instance().fetchAllUsers();
        }
    } else {
        // Якщо прав немає, виводимо сповіщення
        logWarning() << "ACCESS DENIED: User" << currentUser->login()
                     << "tried to open editor for user ID" << targetUserId;
        QMessageBox::warning(this, "Доступ заборонено", "У вас недостатньо прав для редагування цього користувача.");
    }
}

