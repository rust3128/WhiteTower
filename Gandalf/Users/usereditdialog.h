#ifndef USEREDITDIALOG_H
#define USEREDITDIALOG_H

#include <QDialog>
#include <QJsonObject>
#include <QJsonArray>

namespace Ui {
class UserEditDialog;
}

class UserEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UserEditDialog(int userId, QWidget *parent = nullptr);
    ~UserEditDialog();
private slots:
    // Слот для прийому даних з сервера
    void onUserDataReceived(const QJsonObject& user);
    void onAllRolesReceived(const QJsonArray& roles);
    void on_buttonBox_rejected();

    void on_buttonBox_accepted();

private:
    void populateForm();
private:
    Ui::UserEditDialog *ui;
    int m_userId; // Зберігаємо ID
    QJsonObject m_currentUserData;
    QJsonArray m_allRolesData;
};

#endif // USEREDITDIALOG_H
