#ifndef OBJECTSLISTDIALOG_H
#define OBJECTSLISTDIALOG_H

#include "Oracle/ApiClient.h"

#include <QDialog>
#include <QStandardItemModel>
#include <QJsonArray>
#include <QComboBox>

namespace Ui {
class ObjectsListDialog;
}

class ObjectsListDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ObjectsListDialog(QWidget *parent = nullptr);
    ~ObjectsListDialog();
private slots:
    // Слот для обробки успішно завантажених даних
    void onObjectsReceived(const QJsonArray& objects);
    // Слоти для заповнення фільтрів
    void onClientsReceived(const QJsonArray& clients);
    void onRegionsReceived(const QStringList& regions);

    // Слот, що реагує на зміну будь-якого фільтра
    void onFilterChanged();
private:
    // Допоміжні методи для налаштування
    void setupModel();
    void createConnections();
    void loadFiltersData();
    void adjustComboWidth(QComboBox* combo);
private:
    Ui::ObjectsListDialog *ui;
    QStandardItemModel *m_model; // Модель для наших даних
    QTimer* m_searchDebounceTimer; // Таймер для пошуку
};

#endif // OBJECTSLISTDIALOG_H
