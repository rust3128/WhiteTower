#ifndef USERLISTDIALOG_H
#define USERLISTDIALOG_H

#include <QDialog>
#include "Oracle/ApiClient.h" // Підключаємо для ApiError

class QStandardItemModel;
class QJsonArray; // Попереднє оголошення

namespace Ui {
class UserListDialog;
}

class UserListDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UserListDialog(QWidget *parent = nullptr);
    ~UserListDialog();

private slots:
    // --- Слоти для вкладки "Всі користувачі" ---
    void onUsersDataReceived(const QJsonArray& users);
    void on_tableViewUsers_doubleClicked(const QModelIndex &index); // Виправлено ім'я

    // --- ДОДАНО: Слоти для вкладки "Запити від бота" ---
    void onBotRequestsFetched(const QJsonArray& requests);
    void onBotRequestsFetchFailed(const ApiError& error);
    void on_pushButtonrefreshRequests_clicked(); // Слот для кнопки "Оновити"
    void on_pushButtonRejectBot_clicked(); // Слот для вашої нової кнопки
    void onBotRequestRejected(int requestId);
    void onBotRequestRejectFailed(const ApiError& error);
    void on_pushButtonApproveBot_clicked(); // Слот для вашої нової кнопки
    void onBotRequestApproved(int requestId);
    void onBotRequestApproveFailed(const ApiError& error);
    void on_pushButtonLinkBot_clicked(); // Слот для вашої нової кнопки
    void onBotRequestLinked(int requestId);
    void onBotRequestLinkFailed(const ApiError& error);
    // --- Загальний слот ---
    void on_buttonBox_rejected();


private:
    void createUI();
    void createModel();
    void createConnections();

    Ui::UserListDialog *ui;
    QStandardItemModel *m_model;
    QStandardItemModel *m_requestsModel; // --- ДОДАНО: Модель для запитів
};

#endif // USERLISTDIALOG_H
