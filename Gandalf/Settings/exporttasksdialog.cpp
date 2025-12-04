#include "exporttasksdialog.h"
#include "Settings/ui_exporttasksdialog.h"
#include "ui_exporttasksdialog.h"
#include "Oracle/ApiClient.h"
#include "Oracle/Logger.h"

#include <QStandardItemModel>
#include <QMessageBox>
#include <QAbstractItemView>

// --- КОНСТРУКТОР ---
ExportTasksDialog::ExportTasksDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ExportTasksDialog)
{
    ui->setupUi(this);
    setWindowTitle("⚙️ Керування завданнями експорту");

    m_sqlHighlighter = new SqlHighlighter(ui->plainTextEditSQL->document());

    createModel();
    createConnections();
    createUI();
    loadTasks();

    // Початково ховаємо деталі, поки не вибрано завдання
    ui->groupBoxDetails->setEnabled(false);
}

ExportTasksDialog::~ExportTasksDialog()
{
    delete ui;
}

// --- НАЛАШТУВАННЯ ---
void ExportTasksDialog::createModel()
{
    m_model = new QStandardItemModel(0, 4, this);
    m_model->setHorizontalHeaderLabels({"ID", "Назва", "Ім'я файлу", "Активне"});
}

void ExportTasksDialog::createConnections()
{
    // API
    connect(&ApiClient::instance(), &ApiClient::exportTasksFetched, this, &ExportTasksDialog::onExportTasksFetched);
    connect(&ApiClient::instance(), &ApiClient::exportTasksFetchFailed, this, &ExportTasksDialog::onExportTasksFetchFailed);
    connect(&ApiClient::instance(), &ApiClient::exportTaskFetched, this, &ExportTasksDialog::onExportTaskFetched);
    connect(&ApiClient::instance(), &ApiClient::exportTaskFetchFailed, this, &ExportTasksDialog::onExportTaskFetchFailed);
    connect(&ApiClient::instance(), &ApiClient::exportTaskSaved, this, &ExportTasksDialog::onExportTaskSaved);
    connect(&ApiClient::instance(), &ApiClient::exportTaskSaveFailed, this, &ExportTasksDialog::onExportTaskSaveFailed);


}

void ExportTasksDialog::createUI()
{
    // UI
    ui->tableViewTasks->setModel(m_model);
    ui->tableViewTasks->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewTasks->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableViewTasks->setColumnHidden(0, true); // Ховаємо ID
    // --- ДОДАЄМО НАЛАШТУВАННЯ ДЛЯ СТРАТЕГІЇ ОЧИЩЕННЯ ---
    ui->comboBoxDeleteStrategy->addItem("NONE (Нічого не робити)", "NONE");
    ui->comboBoxDeleteStrategy->addItem("FULL_REFRESH (Видалити все)", "FULL_REFRESH");
    ui->comboBoxDeleteStrategy->addItem("SOFT_DELETE (Позначити як неактивний)", "SOFT_DELETE");
    // ----------------------------------------------------
}

void ExportTasksDialog::loadTasks()
{
    // m_model->clear();
    // createModel(); // Скидаємо модель з новими заголовками
    ApiClient::instance().fetchAllExportTasks();
}

// --- ОБРОБНИКИ API ---

void ExportTasksDialog::onExportTasksFetched(const QJsonArray& tasks)
{
    populateTaskList(tasks);
}

void ExportTasksDialog::onExportTasksFetchFailed(const ApiError& error)
{
    QMessageBox::critical(this, "Помилка завантаження",
                          "Не вдалося завантажити список завдань:\\n" + error.errorString);
    logCritical() << "Failed to fetch export tasks:" << error.errorString;
}

void ExportTasksDialog::onExportTaskFetched(const QJsonObject& task)
{
    populateDetailsForm(task);
}

void ExportTasksDialog::onExportTaskFetchFailed(const ApiError& error)
{
    QMessageBox::critical(this, "Помилка завантаження",
                          "Не вдалося завантажити деталі завдання:\\n" + error.errorString);
    logCritical() << "Failed to fetch task details:" << error.errorString;
    // Очищаємо форму і деактивуємо її
    ui->groupBoxDetails->setEnabled(false);
}

void ExportTasksDialog::onExportTaskSaved(int taskId)
{
    m_currentTaskId = taskId; // Оновлюємо ID на той, що присвоїв сервер (якщо це POST)
    QMessageBox::information(this, "Успіх", "Завдання успішно збережено.");
    logInfo() << "Export task ID" << taskId << "saved successfully.";

    // Оновлюємо список, щоб побачити нове/змінене завдання
    loadTasks();

    // Перезавантажуємо деталі, щоб оновити заголовок, якщо це було створення нового завдання
    if (m_currentTaskId != -1) {
        ApiClient::instance().fetchExportTaskById(m_currentTaskId);
    }

    ui->groupBoxDetails->setEnabled(true); // Знову активуємо форму
}

void ExportTasksDialog::onExportTaskSaveFailed(const ApiError& error)
{
    ui->groupBoxDetails->setEnabled(true); // Знову активуємо форму, щоб користувач міг виправити помилку

    QMessageBox::critical(this, "Помилка збереження",
                          "Не вдалося зберегти завдання:\\n" + error.errorString);
    logCritical() << "Failed to save export task:" << error.errorString;
}

// --- ОБРОБНИКИ UI ---

void ExportTasksDialog::on_tableViewTasks_clicked(const QModelIndex &index)
{
    if (!index.isValid()) return;

    // 1. Отримуємо ID із прихованої колонки (колонка 0)
    int taskId = m_model->item(index.row(), 0)->text().toInt();

    // 2. Встановлюємо ID та активуємо деталі
    m_currentTaskId = taskId;
    ui->groupBoxDetails->setEnabled(true);

    // 3. Завантажуємо повні деталі завдання
    ApiClient::instance().fetchExportTaskById(m_currentTaskId);
}

void ExportTasksDialog::on_pushButtonNewTask_clicked()
{
    m_currentTaskId = -1; // Сигналізує про створення
    populateDetailsForm(QJsonObject()); // Очищаємо форму
    ui->groupBoxDetails->setEnabled(true);
}

void ExportTasksDialog::on_pushButtonRefresh_clicked()
{
    loadTasks();
    ui->groupBoxDetails->setEnabled(false);
}

void ExportTasksDialog::on_buttonBox_accepted()
{
    // 1. Збираємо дані з форми
    QJsonObject data = gatherFormData();

    // 2. Валідація обов'язкових полів (залишаємо без змін)
    if (data["task_name"].toString().isEmpty() ||
        data["query_filename"].toString().isEmpty() ||
        data["sql_template"].toString().isEmpty())
    {
        QMessageBox::warning(this, "Попередження",
                             "Будь ласка, заповніть усі обов'язкові поля: 'Назва завдання', 'Ім'я файлу (SQL)' та 'SQL Шаблон'.");
        return;
    }

    // 3. ВИЗНАЧЕННЯ: Створення чи Оновлення.
    if (m_currentTaskId != -1) {
        // Якщо це оновлення, ми ЯВНО додаємо ID до об'єкта, який піде на сервер.
        // Це дозволить ApiClient визначити, що це PUT-запит.
        data["task_id"] = m_currentTaskId;
        logInfo() << "Preparing data for UPDATE (PUT) task ID:" << m_currentTaskId;
    } else {
        // Якщо m_currentTaskId == -1, це нове завдання (POST).
        // Ми НЕ додаємо task_id, тому ApiClient вважатиме це POST-запитом.
        logInfo() << "Preparing data for CREATE NEW (POST) task.";
    }

    // 4. Відправляємо дані на сервер.
    // ApiClient::saveExportTask() тепер використовує наявність 'task_id' для вибору методу.
    ApiClient::instance().saveExportTask(data);

    // 5. Деактивуємо форму, поки не отримаємо відповідь
    ui->groupBoxDetails->setEnabled(false);
}

void ExportTasksDialog::on_buttonBox_rejected()
{
    this->reject(); // Закрити діалог
}


// --- ДОПОМІЖНІ МЕТОДИ ---

void ExportTasksDialog::populateTaskList(const QJsonArray& tasks)
{
    // !!! ВИПРАВЛЕННЯ !!!
    // Встановлюємо кількість рядків на 0. Це очищує таблицю, НЕ розриваючи зв'язок
    // і НЕ видаляючи заголовки (які були встановлені в createModel).
    m_model->setRowCount(0);

    // Ми ВИДАЛЯЄМО:
    // m_model->clear();
    // createModel();

    for (const QJsonValue& value : tasks) {
        QJsonObject task = value.toObject();

        QList<QStandardItem*> rowItems;
        // Колонки: 0=ID, 1=Назва, 2=Ім'я файлу, 3=Активне
        rowItems.append(new QStandardItem(QString::number(task["task_id"].toInt())));
        rowItems.append(new QStandardItem(task["task_name"].toString()));
        rowItems.append(new QStandardItem(task["query_filename"].toString()));

        QString activeStatus = task["is_active"].toInt() == 1 ? "✅ Так" : "❌ Ні";
        rowItems.append(new QStandardItem(activeStatus));

        m_model->appendRow(rowItems);
    }
    ui->tableViewTasks->resizeColumnsToContents();
}

void ExportTasksDialog::populateDetailsForm(const QJsonObject& task)
{
    if (task.isEmpty()) {
        // Очищення форми
        ui->lineEditTaskName->clear();
        ui->lineEditQueryFilename->clear();
        ui->lineEditTargetTable->clear();
        ui->checkBoxIsActive->setChecked(true);
        ui->plainTextEditSQL->clear();
        ui->groupBoxDetails->setTitle("Деталі завдання (Нове)");
        m_currentTaskId = -1;
    } else {
        // Заповнення форми
        ui->lineEditTaskName->setText(task["task_name"].toString());
        ui->lineEditQueryFilename->setText(task["query_filename"].toString());
        ui->lineEditTargetTable->setText(task["target_table"].toString());
        ui->lineEditMatchFields->setText(task["match_fields"].toString());
        ui->lineEditDescription->setText(task["description"].toString());
        // is_active приходить як 0 або 1
        ui->checkBoxIsActive->setChecked(task["is_active"].toInt() == 1);
        ui->plainTextEditSQL->setPlainText(task["sql_template"].toString());
        // --- НОВИЙ КОД: ЧИТАННЯ СТРАТЕГІЇ ---
        QString strategy = task["delete_strategy"].toString();
        int index = ui->comboBoxDeleteStrategy->findData(strategy);
        if (index != -1) {
            ui->comboBoxDeleteStrategy->setCurrentIndex(index);
        } else {
            // Якщо стратегія невідома, обираємо NONE
            ui->comboBoxDeleteStrategy->setCurrentIndex(ui->comboBoxDeleteStrategy->findData("NONE"));
        }
        // -------------------------------------
        m_currentTaskId = task["task_id"].toInt();
        ui->groupBoxDetails->setTitle(QString("Деталі завдання (ID: %1)").arg(m_currentTaskId));
    }
}

QJsonObject ExportTasksDialog::gatherFormData() const
{
    QJsonObject data;
    data["task_name"] = ui->lineEditTaskName->text();
    data["query_filename"] = ui->lineEditQueryFilename->text();
    data["target_table"] = ui->lineEditTargetTable->text().trimmed();
    data["match_fields"] = ui->lineEditMatchFields->text().trimmed();
    data["description"] = ui->lineEditDescription->text();
    data["is_active"] = ui->checkBoxIsActive->isChecked(); // QBool перетвориться на 1 або 0 в ApiClient
    data["sql_template"] = ui->plainTextEditSQL->toPlainText();
    data["delete_strategy"] = ui->comboBoxDeleteStrategy->currentData().toString();
    // Не додаємо task_id тут, він додається у on_buttonBox_accepted,
    // якщо це не нове завдання.

    return data;
}
