#include "mainwindow.h"
#include "version.h"
#include "./ui_mainwindow.h"

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
