#include "usereditdialog.h"
#include "ui_usereditdialog.h"

#include "Oracle/SessionManager.h"
#include "Oracle/User.h"
#include "Oracle/criptpass.h"
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

    connect(&ApiClient::instance(), &ApiClient::userUpdateSuccess, this, &UserEditDialog::accept);
    connect(&ApiClient::instance(), &ApiClient::userUpdateFailed, this, &UserEditDialog::onUpdateFailed);

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

    // --- ПОЧАТОК ЗМІН ---
    // Ми все ще відображаємо ID, але робимо поле нередагованим
    qint64 telegramId = m_currentUserData["telegram_id"].toInteger();
    if (telegramId > 0) {
        ui->lineEditTelegramId->setText(QString::number(telegramId));
    } else {
        ui->lineEditTelegramId->setText("(не прив'язано)");
    }
    ui->lineEditTelegramId->setReadOnly(true); // <--- ВАЖЛИВО
    // --- КІНЕЦЬ ЗМІН ---

    QString encryptedJiraToken = m_currentUserData["jira_token"].toString();
    QString encryptedRedmineToken = m_currentUserData["redmine_token"].toString(); // <--- НОВЕ ПОЛЕ

    // 1. Дешифрування Jira Token та відображення
    if (!encryptedJiraToken.isEmpty()) {
        QString decryptedJiraToken = CriptPass::instance().decriptPass(encryptedJiraToken);
        ui->lineEditJiraToken->setText(decryptedJiraToken);
    } else {
        ui->lineEditJiraToken->clear();
    }

    // 2. Дешифрування Redmine Token та відображення
    if (!encryptedRedmineToken.isEmpty()) {
        QString decryptedRedmineToken = CriptPass::instance().decriptPass(encryptedRedmineToken);
        ui->lineEditRedmineToken->setText(decryptedRedmineToken); // <--- НОВЕ ПОЛЕ UI
    } else {
        ui->lineEditRedmineToken->clear();
    }

    // 2. Динамічно створюємо прапорці для ролей (код без змін)
    QLayoutItem* item;
    while ((item = ui->rolesLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    QSet<QString> userRoles;
    for (const auto& roleValue : m_currentUserData["roles"].toArray()) {
        userRoles.insert(roleValue.toString());
    }
    for (const auto& roleValue : m_allRolesData) {
        QJsonObject roleObj = roleValue.toObject();
        QString roleName = roleObj["role_name"].toString();
        QCheckBox* checkBox = new QCheckBox(roleName, this);
        if (userRoles.contains(roleName)) {
            checkBox->setChecked(true);
        }
        ui->rolesLayout->addWidget(checkBox);
    }

    // Перевірка прав адміністратора (код без змін)
    const User* currentUser = SessionManager::instance().currentUser();
    if (currentUser) {
        ui->groupBoxRoles->setEnabled(currentUser->isAdmin());
    }
}



void UserEditDialog::on_buttonBox_rejected()
{
    this->reject();
}


void UserEditDialog::on_buttonBox_accepted()
{
    // 1. Збираємо дані з форми та ШИФРУЄМО токени
    QJsonObject userData = gatherFormData();

    // 2. Відправляємо дані на сервер (Conduit).
    ApiClient::instance().updateUser(m_userId, userData);
}

// --- ДОДАНО НОВИЙ СЛОТ ---
void UserEditDialog::onUpdateFailed(const ApiError& error)
{
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setText("Не вдалося зберегти зміни.");
    msgBox.setInformativeText(error.errorString);
    msgBox.setDetailedText(QString("URL: %1\nHTTP Status: %2").arg(error.requestUrl).arg(error.httpStatusCode));
    msgBox.exec();
    // Ми НЕ закриваємо вікно, щоб користувач міг виправити дані або спробувати ще раз.
}


QJsonObject UserEditDialog::gatherFormData() const
{
    QJsonObject userData;
    userData["fio"] = ui->lineEditPIB->text();
    userData["is_active"] = ui->checkBoxIsActive->isChecked();

    // --- ОБРОБКА ТА ШИФРУВАННЯ JIRA TOKEN ---
    QString jiraTokenInput = ui->lineEditJiraToken->text();
    QString encryptedJiraToken = jiraTokenInput.isEmpty() ? QString() : CriptPass::instance().criptPass(jiraTokenInput);
    userData["jira_token"] = encryptedJiraToken;

    // --- ОБРОБКА ТА ШИФРУВАННЯ REDMINE TOKEN ---
    QString redmineTokenInput = ui->lineEditRedmineToken->text(); // <--- ЗЧИТУЄМО НОВЕ ПОЛЕ
    QString encryptedRedmineToken = redmineTokenInput.isEmpty() ? QString() : CriptPass::instance().criptPass(redmineTokenInput);
    userData["redmine_token"] = encryptedRedmineToken; // <--- ДОДАЄМО НОВЕ ПОЛЕ

    // Збір ролей (існуючий код)
    QJsonArray rolesArray;
    for (int i = 0; i < ui->rolesLayout->count(); ++i) {
        QCheckBox* checkBox = qobject_cast<QCheckBox*>(ui->rolesLayout->itemAt(i)->widget());
        if (checkBox && checkBox->isChecked()) {
            rolesArray.append(checkBox->text());
        }
    }
    userData["roles"] = rolesArray;

    return userData;
}
