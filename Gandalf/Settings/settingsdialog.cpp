#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "Oracle/AppParams.h"
#include "Oracle/ApiClient.h"
#include <QMessageBox>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    setWindowTitle("Налаштування Gandalf");

    createConnections();
    loadSettings();
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

// 1. Метод для налаштування всіх з'єднань
void SettingsDialog::createConnections()
{
    // З'єднуємо кнопку "Зберегти" (сигнал accepted) з нашим слотом saveSettings
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::saveSettings);

    // З'єднуємо сигнали від ApiClient з нашими обробниками
    connect(&ApiClient::instance(), &ApiClient::settingsUpdateSuccess, this, &SettingsDialog::onSettingsUpdateSuccess);
    connect(&ApiClient::instance(), &ApiClient::settingsUpdateFailed, this, &SettingsDialog::onSettingsUpdateFailed);
}

// 2. Метод для завантаження налаштувань у UI
void SettingsDialog::loadSettings()
{
    // Читаємо параметр з AppParams. Якщо його там немає, використовуємо значення за замовчуванням (7).
    int syncPeriod = AppParams::instance().getParam("Gandalf", "SyncPeriodDays", 7).toInt();
    ui->spinBoxSyncPeriod->setValue(syncPeriod);
}

// 3. Метод, що викликається при збереженні
void SettingsDialog::saveSettings()
{
    QVariantMap settings;
    // Збираємо дані з форми
    settings["SyncPeriodDays"] = ui->spinBoxSyncPeriod->value();
    // (тут можна додати інші параметри, якщо вони з'являться)

    // Відправляємо дані на сервер
    ApiClient::instance().updateSettings("Gandalf", settings);
}

// 4. Обробник успішної відповіді від сервера
void SettingsDialog::onSettingsUpdateSuccess()
{
    // Оновлюємо локальний кеш AppParams
    int syncPeriod = ui->spinBoxSyncPeriod->value();
    AppParams::instance().setParam("Gandalf", "SyncPeriodDays", syncPeriod);

    QMessageBox::information(this, "Успіх", "Налаштування успішно збережено.");
    accept(); // Закриваємо діалог з результатом QDialog::Accepted
}

// 5. Обробник помилки від сервера
void SettingsDialog::onSettingsUpdateFailed(const ApiError &error)
{
    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText("Не вдалося зберегти налаштування.");
    msgBox.setInformativeText(error.errorString);
    msgBox.setDetailedText(QString("URL: %1\nHTTP Status: %2").arg(error.requestUrl).arg(error.httpStatusCode));
    msgBox.exec();
    // Вікно не закриваємо, щоб користувач міг спробувати ще раз
}
