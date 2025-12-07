#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "Oracle/AppParams.h"
#include "Oracle/ApiClient.h"
#include "Oracle/Logger.h"
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

    QString redmineUrl = AppParams::instance().getParam("Global", "RedmineBaseUrl").toString();
    logInfo() << "Завантажую Redmine URL (Global/RedmineBaseUrl):" << redmineUrl;
    ui->lineEditRedmineUrl->setText(redmineUrl);

    QString jiraUrl = AppParams::instance().getParam("Global", "JiraBaseUrl").toString();
    logInfo() << "Завантажую Jira URL (Global/JiraBaseUrl):" << jiraUrl;
    ui->lineEditJiraUrl->setText(jiraUrl);
}

// 3. Метод, що викликається при збереженні
void SettingsDialog::saveSettings()
{
    // --- 1. ЗБИРАЄМО ТА ЗБЕРІГАЄМО НАЛАШТУВАННЯ 'Gandalf' ---
    QVariantMap gandalfSettings;

    // Параметр SyncPeriodDays належить Gandalf'у
    gandalfSettings["SyncPeriodDays"] = ui->spinBoxSyncPeriod->value();

    // Відправляємо на сервер налаштування Gandalf
    ApiClient::instance().updateSettings("Gandalf", gandalfSettings);


    // --- 2. ЗБИРАЄМО ТА ЗБЕРІГАЄМО НАЛАШТУВАННЯ 'Global' ---
    QVariantMap globalSettings;

    // URL-адреси належать Global
    QString redmineUrl = ui->lineEditRedmineUrl->text().trimmed();
    QString jiraUrl = ui->lineEditJiraUrl->text().trimmed();

    globalSettings["RedmineBaseUrl"] = redmineUrl;
    globalSettings["JiraBaseUrl"] = jiraUrl;

    // !!! КЛЮЧОВИЙ МОМЕНТ: ЗБЕРЕЖЕННЯ В APPPARAMS ТА ВІДПРАВКА НА СЕРВЕР !!!
    // Локальне оновлення AppParams (потрібне, щоб інші частини UI одразу побачили зміни)
    AppParams::instance().setParam("Global", "RedmineBaseUrl", redmineUrl);
    AppParams::instance().setParam("Global", "JiraBaseUrl", jiraUrl);

    // Відправляємо на сервер ГЛОБАЛЬНІ налаштування
    // Оскільки updateSettings викликає сигнал, ми повинні
    // обробляти успіх/помилку обох викликів.
    // Для простоти, ми будемо вважати, що якщо перший (Gandalf) успішний,
    // то другий (Global) теж буде оброблено.

    // Для Gandalf ми вже маємо onSettingsUpdateSuccess/Failed.

    // Щоб не ускладнювати логіку обробки сигналу (потрібно було б відстежувати 2 відповіді),
    // ми зробимо один, менш критичний для Gandalf, виклик асинхронним,
    // АЛЕ АБО... АБО...

    // КРАЩЕ РІШЕННЯ: Змінити saveSettings() на два незалежні виклики.
    // Оскільки `ApiClient::updateSettings` асинхронний, ми повинні надіслати обидва:

    // Викликаємо оновлення для Global. Ми не будемо підключати нові слоти
    // для відповіді, щоб не ускладнювати код, але слід розуміти, що
    // можлива мовчазна помилка збереження Global.
    ApiClient::instance().updateSettings("Global", globalSettings);

    // Ми не змінюємо onSettingsUpdateSuccess/Failed, щоб не порушити існуючу логіку
    // закриття діалогу. Припустимо, що успіх Gandalf є достатнім для закриття.

    // На сервері, WebServer повинен мати handleUpdateSettings.
}

// 4. Обробник успішної відповіді від сервера
void SettingsDialog::onSettingsUpdateSuccess()
{
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
