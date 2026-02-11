#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "Oracle/ApiClient.h"
#include <QMainWindow>
#include <QJsonArray>

class StationSearchWidget;

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

    // Нові слоти для завантаження Global налаштувань
    void fetchGlobalSettings();
    void onGlobalSettingsFetched(const QVariantMap& settingsMap);
    void onGlobalSettingsFetchFailed(const ApiError& error);

    /**
     * @brief Викликається, коли користувач вибрав АЗС через пошук
     * @param objectId Внутрішній ID об'єкта (OBJECT_ID з БД)
     */
    void onStationSelected(int objectId);

private:
    void checkAutoSyncNeeded();
    /**
     * @brief Налаштовує віджет пошуку та додає його на форму
     */
    void setupStationSearch();

    /**
     * @brief Запускає таймери та фонові задачі після старту
     */
    void startStartupTimers();
private:
    Ui::MainWindow *ui;
    int m_syncPeriodDays;
    StationSearchWidget *m_searchWidget;
};
#endif // MAINWINDOW_H
