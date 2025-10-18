#include "usereditdialog.h"
#include "ui_usereditdialog.h"
#include "Oracle/ApiClient.h"
#include "Oracle/SessionManager.h"
#include "Oracle/User.h"
#include <QMessageBox>

UserEditDialog::UserEditDialog(int userId, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UserEditDialog),
    m_userId(userId)
{
    ui->setupUi(this);
    setWindowTitle(QString("Редагування користувача (ID: %1)").arg(m_userId));

    // === АДАПТОВАНО ТУТ ===
    connect(&ApiClient::instance(), &ApiClient::userDetailsFetched, this, &UserEditDialog::onUserDataReceived);
    connect(&ApiClient::instance(), &ApiClient::userDetailsFetchFailed, this, [&](const ApiError& error) {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText("Не вдалося завантажити дані користувача.");
        msgBox.setInformativeText(error.errorString);
        msgBox.setDetailedText(QString("URL: %1\nHTTP Status: %2").arg(error.requestUrl).arg(error.httpStatusCode));
        msgBox.exec();
        reject(); // Закриваємо вікно з помилкою
    });

    connect(&ApiClient::instance(), &ApiClient::rolesFetched, this, &UserEditDialog::onAllRolesReceived);
    connect(&ApiClient::instance(), &ApiClient::rolesFetchFailed, this, [&](const ApiError& error) {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText("Не вдалося завантажити список ролей.");
        msgBox.setInformativeText(error.errorString);
        msgBox.setDetailedText(QString("URL: %1\nHTTP Status: %2").arg(error.requestUrl).arg(error.httpStatusCode));
        msgBox.exec();
        reject();
    });

    // Запускаємо обидва запити
    ApiClient::instance().fetchUserById(m_userId);
    ApiClient::instance().fetchAllRoles();
}

UserEditDialog::~UserEditDialog()
{
    delete ui;
}

// Слот #1: прийшли дані користувача
void UserEditDialog::onUserDataReceived(const QJsonObject& user)
{
    m_currentUserData = user;
    populateForm(); // Намагаємось заповнити форму
}

// Слот #2: прийшов список всіх ролей
void UserEditDialog::onAllRolesReceived(const QJsonArray& roles)
{
    m_allRolesData = roles;
    populateForm(); // Намагаємось заповнити форму
}

void UserEditDialog::populateForm()
{
    // Виконуємо, тільки якщо обидва набори даних вже отримані
    if (m_currentUserData.isEmpty() || m_allRolesData.isEmpty()) {
        return;
    }

    ui->lineEditLogin->setText(m_currentUserData["login"].toString());
    ui->lineEditPIB->setText(m_currentUserData["fio"].toString());
    ui->checkBoxIsActive->setChecked(m_currentUserData["is_active"].toBool());
    ui->lineEditLogin->setReadOnly(true);

    ui->lineEditTelegramId->setText(QString::number(m_currentUserData["telegram_id"].toInteger()));
    ui->lineEditJiraToken->setText(m_currentUserData["jira_token"].toString());

    // 2. Динамічно створюємо прапорці для ролей

    // Очищуємо layout від старих віджетів, якщо вони є
    QLayoutItem* item;
    while ((item = ui->rolesLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    // Створюємо набір ролей поточного користувача для швидкого пошуку
    QSet<QString> userRoles;
    for (const auto& roleValue : m_currentUserData["roles"].toArray()) {
        userRoles.insert(roleValue.toString());
    }

    // Створюємо CheckBox для кожної можливої ролі
    for (const auto& roleValue : m_allRolesData) {
        QJsonObject roleObj = roleValue.toObject();
        QString roleName = roleObj["role_name"].toString();

        QCheckBox* checkBox = new QCheckBox(roleName, this);

        // Якщо у користувача є ця роль, встановлюємо галочку
        if (userRoles.contains(roleName)) {
            checkBox->setChecked(true);
        }

        ui->rolesLayout->addWidget(checkBox);
    }

    // === ДОДАНО БЛОК ПЕРЕВІРКИ ПРАВ АДМІНІСТРАТОРА ===
    const User* currentUser = SessionManager::instance().currentUser();
    if (currentUser) {
        // Вмикаємо або вимикаємо всю групу з ролями в залежності від того,
        // чи є поточний користувач адміністратором.
        ui->groupBoxRoles->setEnabled(currentUser->isAdmin());
    }
}




void UserEditDialog::on_buttonBox_rejected()
{
    this->reject();
}


void UserEditDialog::on_buttonBox_accepted()
{
    // 1. Збираємо дані з форми (код без змін)
    QJsonObject userData;
    userData["fio"] = ui->lineEditPIB->text();
    userData["is_active"] = ui->checkBoxIsActive->isChecked();
    userData["telegram_id"] = ui->lineEditTelegramId->text().toLongLong();
    userData["jira_token"] = ui->lineEditJiraToken->text();
    QJsonArray rolesArray;
    for (int i = 0; i < ui->rolesLayout->count(); ++i) {
        QCheckBox* checkBox = qobject_cast<QCheckBox*>(ui->rolesLayout->itemAt(i)->widget());
        if (checkBox && checkBox->isChecked()) {
            rolesArray.append(checkBox->text());
        }
    }
    userData["roles"] = rolesArray;

    // 2. Відправляємо дані на сервер
    // === АДАПТОВАНО ТУТ ===
    connect(&ApiClient::instance(), &ApiClient::userUpdateSuccess, this, &UserEditDialog::accept);
    connect(&ApiClient::instance(), &ApiClient::userUpdateFailed, this, [&](const ApiError& error){
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText("Не вдалося зберегти зміни.");
        msgBox.setInformativeText(error.errorString);
        msgBox.setDetailedText(QString("URL: %1\nHTTP Status: %2").arg(error.requestUrl).arg(error.httpStatusCode));
        msgBox.exec();
        // Вікно не закриваємо, щоб користувач міг спробувати ще раз
    });

    ApiClient::instance().updateUser(m_userId, userData);
}

