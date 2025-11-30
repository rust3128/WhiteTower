#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QJsonArray>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_actionUsers_triggered();
    void on_actionClients_triggered();

    void on_action_triggered();

    void on_actionObjectsList_triggered();

    void on_actionExportTasks_triggered();

    void on_actionOpenSyncMonitor_triggered();
    void onDashboardDataForAutoSync(const QJsonArray& data);

private:
    void checkAutoSyncNeeded();
private:
    Ui::MainWindow *ui;
    int m_syncPeriodDays;
};
#endif // MAINWINDOW_H
