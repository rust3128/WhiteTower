#include "usereditdialog.h"
#include "ui_usereditdialog.h"
#include "Oracle/ApiClient.h"
#include <QMessageBox>

UserEditDialog::UserEditDialog(int userId, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UserEditDialog),
    m_userId(userId)
{
    ui->setupUi(this);
    setWindowTitle(QString("Редагування користувача (ID: %1)").arg(m_userId));

    // З'єднуємо сигнали для обох запитів
    connect(&ApiClient::instance(), &ApiClient::userDetailsFetched, this, &UserEditDialog::onUserDataReceived);
    connect(&ApiClient::instance(), &ApiClient::userDetailsFetchFailed, this, [&](const QString& error) {
        QMessageBox::critical(this, "Помилка", "Не вдалося завантажити дані користувача:\n" + error);
        reject();
    });

    connect(&ApiClient::instance(), &ApiClient::rolesFetched, this, &UserEditDialog::onAllRolesReceived);
    connect(&ApiClient::instance(), &ApiClient::rolesFetchFailed, this, [&](const QString& error) {
        QMessageBox::critical(this, "Помилка", "Не вдалося завантажити список ролей:\n" + error);
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
}



