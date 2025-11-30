#ifndef EXPORTTASKSDIALOG_H
#define EXPORTTASKSDIALOG_H

#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include "Oracle/ApiClient.h" // Для ApiError
#include "SqlHighlighter.h"

class QStandardItemModel;
class QModelIndex; // Для роботи з індексом таблиці

namespace Ui {
class ExportTasksDialog;
}

class ExportTasksDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportTasksDialog(QWidget *parent = nullptr);
    ~ExportTasksDialog();

private slots:
    // --- Слоти для обробки API ---
    void onExportTasksFetched(const QJsonArray& tasks);
    void onExportTasksFetchFailed(const ApiError& error);
    void onExportTaskFetched(const QJsonObject& task);
    void onExportTaskFetchFailed(const ApiError& error);
    void onExportTaskSaved(int taskId);
    void onExportTaskSaveFailed(const ApiError& error);

    // --- Слоти для взаємодії з UI ---
    void on_tableViewTasks_clicked(const QModelIndex &index); // Вибір завдання у списку
    void on_pushButtonNewTask_clicked(); // Кнопка "Нове завдання"
    void on_pushButtonRefresh_clicked(); // Кнопка "Оновити список"
    void on_buttonBox_accepted(); // Кнопка "Зберегти"
    void on_buttonBox_rejected(); // Кнопка "Закрити"

private:
    void createModel();
    void createConnections();
    void createUI();
    void loadTasks(); // Запускає fetchAllExportTasks
    void populateTaskList(const QJsonArray& tasks);
    void populateDetailsForm(const QJsonObject& task);
    QJsonObject gatherFormData() const; // Збирає дані з правої частини

private:
    Ui::ExportTasksDialog *ui;
    QStandardItemModel *m_model;
    int m_currentTaskId = -1; // -1 означає "Нове завдання"
    SqlHighlighter *m_sqlHighlighter;
};

#endif // EXPORTTASKSDIALOG_H
