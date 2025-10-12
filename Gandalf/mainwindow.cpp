#include "mainwindow.h"
#include "version.h"
#include "./ui_mainwindow.h"
#include "Users/userlistdialog.h"
#include "Clients/clientslistdialog.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QString windowTitle = QString("Gandalf %1").arg(PROJECT_VERSION_STR);
    setWindowTitle(windowTitle);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionUsers_triggered()
{
    UserListDialog *userList = new UserListDialog();
    userList->exec();
}


void MainWindow::on_actionClients_triggered()
{
    ClientsListDialog *clnList = new ClientsListDialog();
    clnList->exec();
}

