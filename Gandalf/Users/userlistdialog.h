#ifndef USERLISTDIALOG_H
#define USERLISTDIALOG_H

#include <QDialog>
class QStandardItemModel;

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
    void onUsersDataReceived(const QJsonArray& users);
    void on_buttonBox_rejected();

    void on_tableView_doubleClicked(const QModelIndex &index);

private:
    void createUI();
    void createModel();
    void createConnections();
private:
    Ui::UserListDialog *ui;
    QStandardItemModel *m_model;
};

#endif // USERLISTDIALOG_H
