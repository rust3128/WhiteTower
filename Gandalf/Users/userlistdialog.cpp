#include "userlistdialog.h"
#include "ui_userlistdialog.h"
#include "Oracle/ApiClient.h"
#include "Oracle/Logger.h"
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

    // Отримуємо ID користувача з першої колонки (яка у нас прихована)
    int userId = m_model->item(index.row(), 0)->text().toInt();
    logDebug() << "Opening edit dialog for user ID:" << userId;

    // Створюємо та відкриваємо діалог редагування
    UserEditDialog dlg(userId, this); // Передаємо ID в конструктор
    dlg.exec(); // exec() відкриває вікно модально

    // Після того, як вікно закриється, оновлюємо список
    ApiClient::instance().fetchAllUsers();
}

